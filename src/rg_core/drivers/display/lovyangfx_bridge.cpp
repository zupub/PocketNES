/*
 * LovyanGFX display backend for rg_display.
 *
 * rg_display.c is plain C and LovyanGFX requires C++, so the actual driver
 * lives here and exposes a small C API declared in lovyangfx_bridge.h. The
 * lcd_* wrappers in lovyangfx.h adapt that API for rg_display.c.
 *
 * Behaviour that matters for the rest of the display code:
 *  - lcd_get_buffer()/lcd_send_buffer() stream RGB565 pixels to the panel.
 *    rg_display hands us RGB565 with the bytes pre-swapped (big-endian, the
 *    convention of the old ili9341.h driver), while pushPixels() transmits
 *    each 16-bit value unchanged (the pixelcopy no_convert path, verified on
 *    this panel). lcd_send_buffer() therefore byte-swaps every pixel back to
 *    a plain RGB565 value before sending it.
 *  - With rgb_order = false (MADCTL 0xB8, same as DIJI-NES) this panel reads
 *    the wire value as standard RGB565, so native LovyanGFX drawing
 *    (fillRect etc.) shows correct colours as well.
 *  - The window is set once and several buffer chunks are then streamed into
 *    it, so the panel's address counter keeps incrementing across calls.
 *  - Bus/panel settings mirror DIJI-NES 0.5.0 (src/lgfx_conf.h), which is
 *    known to run on this hardware: panel on SPI3 with the stock ST7789 init
 *    sequence, SD card on SPI2 (only that combination is covered there).
 */
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <driver/spi_master.h> // SPI_MASTER_FREQ_* (used by RG_SCREEN_SPEED)

#include "rg_system.h"

// RG_GPIO_LCD_BCKL comes from the target config.h, which rg_system.h pulls in
// above - the guard must sit after it or driver/ledc.h is never included.
#ifdef RG_GPIO_LCD_BCKL
#include <driver/ledc.h>
#endif

#include "lovyangfx_bridge.h"

#ifndef RG_LGFX_ROTATION
#define RG_LGFX_ROTATION 3
#endif
#ifndef RG_LGFX_INVERT
#define RG_LGFX_INVERT false
#endif
#ifndef RG_LGFX_RGB_ORDER
#define RG_LGFX_RGB_ORDER false
#endif
#ifndef RG_LGFX_BOOT_TEST
// Flash red/green/blue/white through the streaming path (including the
// byte-swap conversion) at boot, to verify the panel wiring and the pipeline.
#define RG_LGFX_BOOT_TEST 1
#endif

class LGFXDevice : public lgfx::LGFX_Device
{
public:
    LGFXDevice()
    {
        {
            auto cfg = _panel.config();
            cfg.pin_cs          = RG_GPIO_LCD_CS;
            cfg.pin_rst         = RG_GPIO_LCD_RST;
            cfg.pin_busy        = -1;
            cfg.memory_width    = 240;
            cfg.memory_height   = 320;
            cfg.panel_width     = 240;
            cfg.panel_height    = 320;
            cfg.offset_x        = 0;
            cfg.offset_y        = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable        = true;
            cfg.invert          = RG_LGFX_INVERT;
            cfg.rgb_order       = RG_LGFX_RGB_ORDER;
            cfg.dlen_16bit      = false;
            cfg.bus_shared      = false;
            _panel.config(cfg);
        }
        {
            auto cfg = _bus.config();
            cfg.spi_host   = RG_SCREEN_HOST;
            cfg.spi_mode   = 0;
            cfg.freq_write = RG_SCREEN_SPEED;
            cfg.freq_read  = 6000000;
            cfg.spi_3wire  = true;
            cfg.use_lock   = false;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk   = RG_GPIO_LCD_CLK;
            cfg.pin_mosi   = RG_GPIO_LCD_MOSI;
            cfg.pin_miso   = RG_GPIO_LCD_MISO;
            cfg.pin_dc     = RG_GPIO_LCD_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        panel(&_panel);
    }

private:
    // Stock lgfx::Panel_ST7789: its built-in init sequence is the one the
    // DIJI-NES 0.5.0 firmware uses on this exact panel.
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_SPI _bus;
};

static LGFXDevice *gfx = nullptr;
static uint16_t *dma_buffer = nullptr;
static size_t dma_buffer_pixels = 0;

bool rg_lgfx_init(void)
{
    if (gfx)
        return true;

    gfx = new LGFXDevice();
    if (!gfx->init())
    {
        RG_LOGE("LovyanGFX init failed!");
        delete gfx;
        gfx = nullptr;
        return false;
    }

    gfx->setRotation(RG_LGFX_ROTATION);

    RG_LOGI("LovyanGFX ready (%dx%d, rotation %d)", gfx->width(), gfx->height(), RG_LGFX_ROTATION);

#if RG_LGFX_BOOT_TEST
    // Colour check through the *streaming* path (set_window + send_buffer,
    // i.e. exactly what rg_display uses for the GUI and the emulator), so it
    // exercises the byte-swap conversion in rg_lgfx_send_buffer(). Expected
    // order: red, green, blue, white, each shown for ~1s.
    {
        static const int test_colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE};
        const int width = gfx->width();
        const int height = gfx->height();
        const int strip_lines = 4;
        const size_t strip_px = (size_t)width * strip_lines;
        uint16_t *strip = rg_lgfx_get_buffer(strip_px);

        for (size_t i = 0; i < RG_COUNT(test_colors); ++i)
        {
            // rg_display's convention: colours stored byte-swapped (big-endian)
            const uint16_t be = (uint16_t)((test_colors[i] >> 8) | (test_colors[i] << 8));
            rg_lgfx_set_window(0, 0, width, height);
            for (int y = 0; y < height; y += strip_lines)
            {
                // send_buffer() converts in place, so refill the strip before
                // every chunk (reusing the converted data would garble it)
                for (size_t j = 0; j < strip_px; ++j)
                    strip[j] = be;
                rg_lgfx_send_buffer(strip, strip_px);
            }
            rg_lgfx_sync();
            RG_LOGI("Boot test: color #%u", (unsigned)i);
            delay(900);
        }

        gfx->fillScreen(TFT_BLACK);
        gfx->waitDisplay();
    }
#endif

#ifdef RG_GPIO_LCD_BCKL
    // Local structs (not C99 compound literals): C++ requires the designated
    // initializers to follow the field declaration order in driver/ledc.h.
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = RG_GPIO_LCD_BCKL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0, // Avoid the flash caused by the panel reset
        .hpoint = 0,
    };
    ledc_channel_config(&ledc_channel);
    ledc_fade_func_install(0);
#endif

    return true;
}

void rg_lgfx_deinit(void)
{
    if (!gfx)
        return;

    gfx->waitDisplay();
    gfx->releaseBus();
    delete gfx;
    gfx = nullptr;

    free(dma_buffer);
    dma_buffer = nullptr;
    dma_buffer_pixels = 0;
}

void rg_lgfx_sync(void)
{
    if (gfx)
        gfx->waitDisplay();
}

void rg_lgfx_set_backlight(float percent)
{
    float level = RG_MIN(RG_MAX(percent / 100.f, 0.f), 1.f);
#ifdef RG_GPIO_LCD_BCKL
    if (ledc_set_fade_time_and_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0x1FFF * level, 50, LEDC_FADE_NO_WAIT) != ESP_OK)
        RG_LOGE("Failed to set backlight to %d%%", (int)(100 * level));
#else
    (void)level; // Backlight is hardwired on this target
#endif
}

void rg_lgfx_set_window(int left, int top, int width, int height)
{
    if (!gfx || width <= 0 || height <= 0)
        return;

    // Panel_LCD::setWindow() only pushes CASET/RASET/RAMWR onto the bus; unlike
    // the drawing helpers it does not open a transaction itself. Since
    // pushPixels()/endWrite() raises CS again, the window commands would be
    // sent with CS high and silently ignored by the panel. Hold a transaction
    // open around them so the writes actually reach the controller.
    gfx->startWrite();
    gfx->setWindow((uint_fast16_t)left, (uint_fast16_t)top,
                   (uint_fast16_t)(left + width - 1), (uint_fast16_t)(top + height - 1));
    gfx->endWrite();
}

uint16_t *rg_lgfx_get_buffer(size_t length)
{
    if (length > dma_buffer_pixels)
    {
        free(dma_buffer);
        dma_buffer = (uint16_t *)rg_alloc(length * sizeof(uint16_t), MEM_DMA);
        dma_buffer_pixels = dma_buffer ? length : 0;
        if (!dma_buffer)
            RG_PANIC("display out of DMA memory");
    }
    return dma_buffer;
}

// With rgb_order = false (MADCTL 0xB8, same as DIJI-NES) this panel reads the
// 16-bit wire value as standard RGB565 (red in the high bits). Note its MADCTL
// BGR bit has inverted polarity compared to a stock ST7789: 0xB0 makes it
// read blue in the high bits instead (both verified with test patterns).
// rg_display hands us colours as big-endian RGB565 (the old ili9341 driver's
// convention: the pixel value is byte-swapped), so byte-swap each pixel back
// to its RGB565 value. pushPixels() then transmits each value unchanged (the
// pixelcopy no_convert path), which is exactly what the panel expects.
static inline uint16_t be_rgb565_to_panel(uint16_t x)
{
    return (uint16_t)((x >> 8) | (x << 8)); // big-endian -> RGB565 value
}

void rg_lgfx_send_buffer(uint16_t *buffer, size_t length)
{
    if (!gfx || !buffer || !length)
        return;

    // In-place is safe: rg_display fills every chunk completely before
    // sending it and never re-reads a chunk after lcd_send_buffer() returns.
    for (size_t i = 0; i < length; ++i)
        buffer[i] = be_rgb565_to_panel(buffer[i]);

    gfx->pushPixels((const uint16_t *)buffer, (int32_t)length, true);
}
