# PocketNES

A handheld **NES emulator firmware for ESP32-S3** — built on the [Retro-Go](https://github.com/ducalex/retro-go) framework and the [nofrendo](https://github.com/ducalex/retro-go/tree/dev/cores) core, driven by **LovyanGFX**, with a fully localized **Simplified Chinese UI**.

![PlatformIO](https://img.shields.io/badge/PlatformIO-espressif32-blue)
![ESP32-S3](https://img.shields.io/badge/MCU-ESP32--S3%20N16R8-red)
![License](https://img.shields.io/badge/License-GPL--2.0-green)

<p align="center">
  <strong><a href="README.md">English</a> | <a href="README_CN.md">中文</a></strong>
</p>

Drop `.nes` ROMs on an SD card, pick a game from the built-in file browser and play at full speed with sound. Every part of the UI — menus, file browser, settings, alerts — is available in Simplified Chinese.

<!-- Add a photo of your device here, it sells the project better than any text -->
<!-- ![PocketNES handheld](docs/screenshot.png) -->

## Features

- **Full-speed NES emulation** — nofrendo core with 50+ mappers, 32 kHz I2S audio output
- **LovyanGFX display backend** — `Panel_ST7789` on a dedicated SPI3 bus at 80 MHz with DMA, 320×240 landscape UI
- **Complete Simplified Chinese interface** — 195 translated strings plus a 16×16 bitmap font with 7,544 glyphs (English / French / German are still available in the language menu)
- **File browser** — UTF-8 filenames, long names are truncated with an ellipsis instead of wrapping
- **Save states** — one slot per ROM, stored on SD
- **ZIP ROM support** — load `.zip` archives directly, no need to extract
- **Persistent settings** — JSON files on SD (`/sd/retro-go/config`)
- **Video options** — overscan, auto-crop, NES palette selection, sprite-limit toggle
- **One-command build & flash** with PlatformIO

## Hardware

| Part | Recommendation | Notes |
|------|----------------|-------|
| MCU | ESP32-S3 module, **N16R8** | 16 MB flash, 8 MB **OPI** PSRAM (`qio_opi`) |
| Display | ST7789 240×320 SPI panel | Used in landscape (320×240); backlight hardwired to VCC |
| Audio | MAX98357A I2S DAC/amp | Mono, 32 kHz sample rate |
| Storage | microSD card (FAT32) | On its own SPI bus |
| Input | 8 tactile buttons | Active-low, internal pull-ups |

The display and the SD card **must** be wired to separate SPI peripherals — the firmware puts the panel on `SPI3_HOST` and the SD card on `SPI2_HOST` so they never contend for the bus.

### Pinout

**Display (SPI3)**

| Signal | GPIO |
|--------|------|
| SCLK | 14 |
| MOSI | 13 |
| DC | 11 |
| CS | 10 |
| RST | 12 |
| MISO | not connected |
| Backlight | hardwired on |

**microSD (SPI2)**

| Signal | GPIO |
|--------|------|
| MISO | 39 |
| MOSI | 41 |
| CLK | 40 |
| CS | 42 |

**I2S DAC (MAX98357A)**

| Signal | GPIO |
|--------|------|
| BCK | 5 |
| WS | 4 |
| DIN | 6 |

**Buttons** (active-low, to GND)

| Button | GPIO | Button | GPIO |
|--------|------|--------|------|
| Up | 17 | A | 48 |
| Down | 3 | B | 47 |
| Left | 8 | Select | 16 |
| Right | 18 | Start | 15 |

## Build & Flash

Requirements: [PlatformIO](https://platformio.org/) (Core CLI or the VS Code extension) and a USB cable.

```bash
# Build
pio run

# Flash (auto-detected port, or pass --upload-port COM5 / /dev/ttyUSB0)
pio run -t upload

# Serial log
pio device monitor -b 115200
```

The board profile is `esp32-s3-devkitc-1` with `board_build.arduino.memory_type = qio_opi` and the custom partition table `partitions_nes.csv` (4 MB app partition).

## SD Card Layout

```
/sd/
├── nes/                  # put .nes and .zip ROMs here
└── retro-go/
    ├── config/           # global.json, wifi.json, boot.json ...
    └── saves/            # save states and per-ROM settings
```

The file browser opens in `/sd/nes` on boot.

## Controls

| Button | In game | In menus |
|--------|---------|----------|
| D-pad | Move | Navigate |
| A | A | Confirm |
| B | B | Back |
| Start | Start | — |
| Select | Select | — |
| **Start + Select** | **Open menu** | — |

The in-game menu offers resume, save state, load state, reset, options and screenshot.

## Configuration

Hardware and build-time options live in [`src/rg_core/targets/my-s3-st7789/config.h`](src/rg_core/targets/my-s3-st7789/config.h):

- `RG_SCREEN_HOST` / `RG_SCREEN_SPEED` — panel bus and clock (SPI3 @ 80 MHz)
- `RG_LGFX_ROTATION`, `RG_LGFX_INVERT`, `RG_LGFX_RGB_ORDER` — panel orientation and colour mode
- `RG_GPIO_*` — display, SD, I2S and button pin maps
- `RG_LANG_DEFAULT`, `RG_FONT_DEFAULT` — default UI language and font

> **Panel note:** some ST7789 clones read the MADCTL BGR bit with **inverted polarity** (0xB0 = blue in the high bits, 0xB8 = red in the high bits). This target uses `rgb_order = false` (MADCTL 0xB8) and `lcd_send_buffer()` only undoes the byte pre-swap. If your panel shows red/blue swapped, flip `RG_LGFX_RGB_ORDER`.

## Project Structure

```
├── platformio.ini              # PlatformIO environment, flags, include paths
├── partitions_nes.csv          # NVS / OTA / 4 MB app partition table
├── lib/LovyanGFX/              # local LovyanGFX copy (large CJK fonts excluded)
└── src/
    ├── main.cpp                # Arduino entry: rg_system_init() + emulator task
    ├── shared.h                # C/C++ ABI boundary
    ├── nes_main.c              # nofrendo main loop, blit, input, audio, handlers
    ├── nofrendo/               # NES core + 50+ mappers
    └── rg_core/                # Retro-Go framework
        ├── rg_display.c        # streaming display pipeline
        ├── rg_gui.c            # dialog / text engine (ellipsis truncation, etc.)
        ├── rg_audio.c          # I2S output
        ├── rg_settings.c       # JSON settings on SD
        ├── drivers/display/
        │   ├── lovyangfx_bridge.{h,cpp}  # C++ bridge for rg_display.c
        │   └── lovyangfx.h               # lcd_* C API
        ├── fonts/efontCN16.c   # 16x16 Chinese bitmap font (7,544 glyphs)
        └── targets/my-s3-st7789/config.h # hardware definition
```

## Architecture Notes

- **Boot path:** `main.cpp` → `rg_system_init()` (display, audio, input, storage, settings) → `nes_main()` which never returns.
- **Rendering:** two `RG_PIXEL_PAL565_BE` surfaces are written alternately; `blit_screen()` applies overscan/auto-crop, then `rg_display_submit()` streams to the panel. The palette is byte-swapped at build time so both the GUI and game paths share one byte-order convention.
- **LovyanGFX bridge:** `rg_display.c` is plain C, so `lovyangfx_bridge.cpp` exposes a small `extern "C"` API (`rg_lgfx_*`). Streaming uses `setWindow()` wrapped in `startWrite()/endWrite()` (the low-level window command does not open a SPI transaction itself) followed by `pushPixels()`.
- **GUI text engine:** custom bitmap-font renderer in `rg_gui.c` with UTF-8 decoding, multiline layout, alignment and `RG_TEXT_ELLIPSIS` single-line truncation.
- **Chinese font:** `efontCN16.c` is generated from u8g2's `lgfx_efont_cn_16` table and stored as fixed-stride records so `find_glyph()` can binary-search it.

## Credits & License

This project stands on the shoulders of:

- [Retro-Go](https://github.com/ducalex/retro-go) by ducalex — framework, GUI, settings, audio pipeline
- [nofrendo](https://github.com/ducalex/retro-go/tree/dev/cores) — NES emulation core
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) by lovyan03 — display driver
- [u8g2 / efont](https://github.com/olikraus/u8g2) by olikraus — Chinese glyph data
- [cJSON](https://github.com/DaveGamble/cJSON) and [lodepng](https://lodev.org/lodepng/)

Retro-Go and nofrendo are licensed under **GPL-2.0**, so this project is distributed under the same license. See `LICENSE` for the full text.
