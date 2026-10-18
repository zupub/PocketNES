// LovyanGFX display driver glue for rg_display.c.
//
// rg_display.c expects a set of static lcd_* symbols from the driver header it
// includes. LovyanGFX itself is C++ (see lovyangfx_bridge.cpp), so these are
// thin wrappers over the C bridge API.
//
// Select it with: #define RG_SCREEN_DRIVER 2

#include "drivers/display/lovyangfx_bridge.h"

static void lcd_init(void)
{
    if (!rg_lgfx_init())
        RG_PANIC("LovyanGFX initialization failed!");
}

static void lcd_deinit(void)
{
    rg_lgfx_deinit();
}

static inline uint16_t *lcd_get_buffer(size_t length)
{
    return rg_lgfx_get_buffer(length);
}

static inline void lcd_send_buffer(uint16_t *buffer, size_t length)
{
    rg_lgfx_send_buffer(buffer, length);
}

static void lcd_sync(void)
{
    rg_lgfx_sync();
}

static void lcd_set_backlight(float percent)
{
    rg_lgfx_set_backlight(percent);
}

static void lcd_set_window(int left, int top, int width, int height)
{
    rg_lgfx_set_window(left, top, width, height);
}

const rg_display_driver_t rg_display_driver_lovyangfx = {
    .name = "lovyangfx",
};
