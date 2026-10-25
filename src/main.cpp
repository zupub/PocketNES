/*
 * Retro-Go NES port entry point for PlatformIO + Arduino (ESP32-S3).
 *
 * Replaces retro-core/main/main.c: rg_system_init() performs all global
 * hardware initialization (display, audio, input, storage, settings),
 * then nes_main() runs the emulator and never returns.
 */
#include <Arduino.h>
#include <driver/gpio.h>
#include "rg_system.h"
#include "rg_pwrkey.h"
#include "rg_surface.h"
#include "rg_display.h"
#include "drivers/display/lovyangfx_bridge.h"
#include "shared.h"

#ifdef RG_BOOT_LOGO
// startup.png is embedded into the firmware (see board_build.embed_files in
// platformio.ini, enabled for both envs). objcopy names the symbols
// after the file path.
extern const uint8_t boot_png_start[] asm("_binary_startup_png_start");
extern const uint8_t boot_png_end[] asm("_binary_startup_png_end");

// Box-filter downscale of an RGB565_LE buffer: every destination pixel is
// the average of ALL source pixels it covers. Nearest-neighbor sampling
// (rg_surface_copy) drops ~84% of the pixels on a 2.5x downscale, which
// breaks up text strokes and gradients into a noisy, dirty-looking image.
static void scale_box_565le(const uint16_t *src, int src_w, int src_h, uint16_t *dst, int dst_w, int dst_h)
{
    for (int dy = 0; dy < dst_h; ++dy)
    {
        int sy0 = (int)((int64_t)dy * src_h / dst_h);
        int sy1 = (int)((int64_t)(dy + 1) * src_h / dst_h);
        if (sy1 <= sy0)
            sy1 = sy0 + 1;
        for (int dx = 0; dx < dst_w; ++dx)
        {
            int sx0 = (int)((int64_t)dx * src_w / dst_w);
            int sx1 = (int)((int64_t)(dx + 1) * src_w / dst_w);
            if (sx1 <= sx0)
                sx1 = sx0 + 1;
            unsigned r = 0, g = 0, b = 0;
            for (int sy = sy0; sy < sy1; ++sy)
            {
                const uint16_t *row = src + (size_t)sy * src_w;
                for (int sx = sx0; sx < sx1; ++sx)
                {
                    uint16_t p = row[sx];
                    r += (p >> 8) & 0xF8;
                    g += (p >> 3) & 0xFC;
                    b += (p << 3) & 0xF8;
                }
            }
            int n = (sx1 - sx0) * (sy1 - sy0);
            dst[(size_t)dy * dst_w + dx] =
                (uint16_t)((((r / n) >> 3) << 11) | (((g / n) >> 2) << 5) | ((b / n) >> 3));
        }
    }
}

// Decode and show startup.png on the streaming path, letterboxed to the panel.
// Must run after rg_system_init() (panel + backlight up) and before nes_main()
// starts pushing frames. The PNG can be any size: it is scaled to fit and
// centered on a black background. Failure is silent (boot proceeds).
static void show_boot_logo(void)
{
    rg_surface_t *img = rg_surface_load_image(boot_png_start, boot_png_end - boot_png_start, 0);
    if (!img)
    {
        RG_LOGW("startup.png could not be decoded, skipping boot logo.\n");
        return;
    }

    // Scale proportionally to fit the panel (an 800x600 image fills the
    // 320x240 panel exactly). The wider-than-panel branch must fit the WIDTH
    // (not the height), or the scaled image ends up wider than the window and
    // every streamed line overflows into the next one (torn, slanted output).
    int dst_w, dst_h;
    if (img->width * RG_SCREEN_HEIGHT >= img->height * RG_SCREEN_WIDTH)
    {
        dst_w = RG_SCREEN_WIDTH; // image is relatively wider: fit the width
        dst_h = img->height * dst_w / img->width;
    }
    else
    {
        dst_h = RG_SCREEN_HEIGHT; // image is relatively taller: fit the height
        dst_w = img->width * dst_h / img->height;
    }

    // Convert the format WITHOUT resampling: the rg_surface_copy scaler is
    // nearest-neighbor, which turns a 2.5x+ downscale dirty. Rescale below
    // with the box filter instead.
    rg_surface_t *tmp = rg_surface_convert(img, 0, 0, RG_PIXEL_565_LE);
    rg_surface_free(img);
    if (!tmp)
        return;
    rg_surface_t *scaled = rg_surface_create(dst_w, dst_h, RG_PIXEL_565_LE, 0);
    if (!scaled)
    {
        rg_surface_free(tmp);
        return;
    }
    scale_box_565le((const uint16_t *)tmp->data, tmp->width, tmp->height,
                    (uint16_t *)scaled->data, dst_w, dst_h);
    rg_surface_free(tmp);

    // rg_lgfx_send_buffer() expects the rg_display convention: RGB565 with the
    // bytes pre-swapped (it swaps each pixel back before pushing to the panel).
    uint16_t *pixels = (uint16_t *)scaled->data;
    for (int i = 0; i < dst_w * dst_h; ++i)
        pixels[i] = __builtin_bswap16(pixels[i]);

    // One window for the whole panel, then stream one full-width line at a
    // time. Each line is rebuilt into the line buffer as [left border | image
    // | right border], so the panel's address counter stays aligned even when
    // the scaled image is narrower than the panel. send_buffer() converts in
    // place, so every line must be refilled before it is sent.
    int top = (RG_SCREEN_HEIGHT - dst_h) / 2;
    int left = (RG_SCREEN_WIDTH - dst_w) / 2;

    rg_lgfx_set_window(0, 0, RG_SCREEN_WIDTH, RG_SCREEN_HEIGHT);
    uint16_t *line = rg_lgfx_get_buffer(RG_SCREEN_WIDTH);
    for (int y = 0; y < RG_SCREEN_HEIGHT; ++y)
    {
        int ty = y - top;
        memset(line, 0, RG_SCREEN_WIDTH * sizeof(uint16_t)); // RGB565 black
        if (ty >= 0 && ty < dst_h)
            memcpy(line + left, pixels + (size_t)ty * dst_w, dst_w * sizeof(uint16_t));
        rg_lgfx_send_buffer(line, RG_SCREEN_WIDTH);
    }
    rg_surface_free(scaled);

    // While the logo is up, a short press of BOOT (GPIO 0, already configured
    // as input-pullup by rg_input_init) also jumps into the recovery menu.
    // This is the reliable entry point: BOOT is a strapping pin, so holding it
    // at power-on may drop the chip into the ROM download mode instead (black
    // screen), in which case this path still works on the next boot attempt.
    uint32_t deadline = millis() + 1500;
    while (millis() < deadline)
    {
        if (gpio_get_level(GPIO_NUM_0) == 0)
            rg_system_enter_recovery(); // never returns
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Fade the logo out to black before entering the GUI. The file picker and
    // the other menus draw incrementally (only their own rects) and assume the
    // panel starts out black - any leftover logo would otherwise stay visible
    // around their dialogs, like a burn-in (it shares the screen center).
    rg_display_clear(C_BLACK);
}
#endif // RG_BOOT_LOGO

static void emulator_task(void *arg)
{
    // Latch the soft-power circuit before anything else, or the device dies as
    // soon as the power key is released (battery powered boots only).
    rg_pwrkey_init();

    rg_system_init(AUDIO_SAMPLE_RATE, NULL, NULL);

#ifdef RG_BOOT_LOGO
    show_boot_logo();
#endif

    nes_main(); // Never returns (main loop runs forever)

    RG_PANIC("Never reached");
    vTaskDelete(NULL);
}

void setup()
{
    // Run the emulator on core 0 with a generous stack, like the stock
    // ESP-IDF main task (loopTask would also work but has only 8KB).
    xTaskCreatePinnedToCore(emulator_task, "nes", 16384, NULL, 1, NULL, 0);
}

void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
