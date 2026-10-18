#pragma once

// C API exposed by lovyangfx_bridge.cpp.
//
// LovyanGFX is a C++ library and cannot be included from rg_display.c, so the
// driver is split in two: this header (plain C) and its .cpp implementation.
// See lovyangfx.h for the lcd_* functions used by rg_display.c.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool rg_lgfx_init(void);
void rg_lgfx_deinit(void);
void rg_lgfx_sync(void);
void rg_lgfx_set_backlight(float percent);
void rg_lgfx_set_window(int left, int top, int width, int height);
uint16_t *rg_lgfx_get_buffer(size_t length);
void rg_lgfx_send_buffer(uint16_t *buffer, size_t length);

#ifdef __cplusplus
}
#endif
