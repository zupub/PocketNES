# PocketNES

A handheld **NES emulator firmware for ESP32-S3** — built with **PlatformIO** on the [Retro-Go](https://github.com/ducalex/retro-go) framework and the [nofrendo](https://github.com/ducalex/retro-go/tree/dev/cores) core, driven by **LovyanGFX**, with a fully localized **Simplified Chinese UI**.

<div align="center">
  
![PlatformIO](https://img.shields.io/badge/PlatformIO-espressif32-blue)
![ESP32-S3](https://img.shields.io/badge/MCU-ESP32--S3%20N16R8-red)
![License](https://img.shields.io/badge/License-GPL--2.0-green)

</div>

<p align="center">
  <strong><a href="README.md">English</a> | <a href="README_CN.md">中文</a></strong>
</p>

Drop `.nes` ROMs on an SD card, pick a game from the built-in file browser and play at full speed with sound. Every part of the UI — menus, file browser, settings, alerts — is available in Simplified Chinese.

The same firmware also runs on Waveshare's **ESP32-S3-Touch-LCD-2.8** board, where a BLE gamepad replaces the button cluster — see [Hardware](#hardware).

![PocketNES boot screen](assets/pocketnes-boot.png)
![PocketNES handheld](assets/pocketnes-cover.jpg)
![PocketNES menu](assets/pocketnes-menu.jpg)
![PocketNES options](assets/pocketnes-option.jpg)

## Features

- **Full-speed NES emulation** — nofrendo core with 50+ mappers, 32 kHz I2S audio output
- **Two hardware targets** — 8-button handheld and the button-less Waveshare ESP32-S3-Touch-LCD-2.8, built from one codebase
- **LovyanGFX display backend** — `Panel_ST7789` on a dedicated SPI3 bus at 80 MHz with DMA, 320×240 landscape UI
- **BLE gamepad input** — NimBLE HID host scans, pairs and reconnects on its own, and a Bluetooth icon appears in the status bar
- **Soft power key** — the Waveshare board's battery-power latch is held by the firmware, and a ~2 s press powers the device off
- **Complete Simplified Chinese interface** — 206 translated strings plus a 16×16 bitmap font with 7,544 glyphs (English / French / German are still available in the language menu)
- **File browser** — UTF-8 filenames, long names are truncated with an ellipsis instead of wrapping
- **Save states** — one slot per ROM, stored on SD
- **ZIP ROM support** — load `.zip` archives directly, no need to extract
- **Persistent settings** — JSON files on SD (`/sd/retro-go/config`)
- **Video options** — overscan, auto-crop, NES palette selection, sprite-limit toggle
- **One-command build & flash** with PlatformIO

## Hardware

One codebase, two board profiles. Pick the PlatformIO environment that matches your hardware:

| Board | Display | Input | Environment |
|-------|---------|-------|-------------|
| 8-button handheld (DIJI-NES) | ST7789 240×320 SPI | 8 GPIO buttons | `nes-s3-st7789` |
| Waveshare ESP32-S3-Touch-LCD-2.8 | ST7789 240×320 SPI + CST328 touch | BLE gamepad + BOOT / power keys | `nes-s3-st7789-ble` |

### 8-button handheld (DIJI-NES)

| Part | Recommendation | Notes |
|------|----------------|-------|
| MCU | ESP32-S3 module, **N16R8** | 16 MB flash, 8 MB **OPI** PSRAM (`qio_opi`) |
| Display | ST7789 240×320 SPI panel | Used in landscape (320×240); backlight hardwired to VCC |
| Audio | MAX98357A I2S DAC/amp | Mono, 32 kHz sample rate |
| Storage | microSD card (FAT32) | On its own SPI bus |
| Input | 8 tactile buttons | Active-low, internal pull-ups |

The hardware wiring follows the [DIJI-NES](https://github.com/UF-Evan/DIJI-NES) design by UF-Evan.

The display and the SD card **must** be wired to separate SPI peripherals — the firmware puts the panel on `SPI3_HOST` and the SD card on `SPI2_HOST` so they never contend for the bus.

### Pinout (8-button handheld)

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

### Waveshare ESP32-S3-Touch-LCD-2.8 + BLE gamepad

This is Waveshare's [ESP32-S3-Touch-LCD-2.8](https://docs.waveshare.net/ESP32-S3-Touch-LCD-2.8/) board (SKU 27690). It has no d-pad, no A/B and no MENU key, so it is played entirely from a BLE gamepad:

- **Input** — a BLE HID gamepad supplies d-pad/A/B/Select/Start (see [Controls](#controls)); on the PCB only BOOT (GPIO 0) and the BAT power key remain, and they drive the recovery menu.
- **Display** — the onboard ST7789 panel on SPI3, with SCLK 40 / MOSI 45 / DC 41 / CS 42 / RST 39 and a PWM-dimmable backlight on GPIO 5 (which is why the Brightness option appears).
- **SD card** — the microSD slot on SPI2 with CS 21 / CLK 14 / MISO 16 / MOSI 17, clocked at 5 MHz.
- **Audio** — the onboard PCM5101A DAC and amplifier on I2S BCLK 48 / LRCLK 38 / DIN 47; the board's volume knob still works.
- **Power** — soft-power latch: the BAT key (GPIO 6) switches the rail on and the firmware holds it through GPIO 7. Hold it ~2 s to power off; a short press does nothing outside the recovery menu.
- **Battery** — the MX1.25 cell input, read on GPIO 8 through a 1:3 divider.

The capacitive touch panel (CST328 on V1, CST3530 on V2), QMI8658 IMU, PCF85063 RTC and charge management are not used by the firmware.

The complete pin map lives in [`src/rg_core/targets/my-s3-st7789-ble/config.h`](src/rg_core/targets/my-s3-st7789-ble/config.h).

<!-- TODO(photo): replace this placeholder with the real photo, keeping the same path:
     ![PocketNES on the Waveshare ESP32-S3-Touch-LCD-2.8](assets/pocketnes-28-ble.jpg) -->

> **Photo placeholder** — `assets/pocketnes-28-ble.jpg`: the Waveshare ESP32-S3-Touch-LCD-2.8 with a BLE gamepad connected.

## Build & Flash

Requirements: [PlatformIO](https://platformio.org/) (Core CLI or the VS Code extension) and a USB cable.

```bash
# Build the 8-button handheld
pio run -e nes-s3-st7789

# Build the Waveshare ESP32-S3-Touch-LCD-2.8 (BLE gamepad)
pio run -e nes-s3-st7789-ble

# Flash (auto-detected port, or pass --upload-port COM5 / /dev/ttyUSB0)
pio run -e nes-s3-st7789 -t upload
pio run -e nes-s3-st7789-ble -t upload

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

### BLE gamepad (Waveshare board)

The Waveshare board has no gamepad buttons, so the pad is the only controller. Put it in Android/BLE mode (not X-input or Switch) and switch the device on — the firmware scans for it at boot and reconnects by itself after a disconnect.

| Gamepad | PocketNES |
|---------|-----------|
| D-pad | D-pad |
| A | A |
| B | B |
| X | B |
| Y | A |
| Select | Select |
| Start | Start |
| **Start + Select** | **Open menu** |

A Bluetooth icon appears in the status bar while a pad is connected.

## Configuration

Hardware and build-time options live in the `config.h` of the selected target:

- [`src/rg_core/targets/my-s3-st7789/config.h`](src/rg_core/targets/my-s3-st7789/config.h) — 8-button handheld
- [`src/rg_core/targets/my-s3-st7789-ble/config.h`](src/rg_core/targets/my-s3-st7789-ble/config.h) — Waveshare ESP32-S3-Touch-LCD-2.8

The main options:

- `RG_SCREEN_HOST` / `RG_SCREEN_SPEED` — panel bus and clock (SPI3 @ 80 MHz)
- `RG_LGFX_ROTATION`, `RG_LGFX_INVERT`, `RG_LGFX_RGB_ORDER` — panel orientation and colour mode
- `RG_GPIO_*` — display, SD, I2S and button pin maps
- `RG_LANG_DEFAULT`, `RG_FONT_DEFAULT` — default UI language and font
- `RG_INPUT_BLE_GAMEPAD` — enables the NimBLE gamepad driver (BLE target only)
- `RG_GPIO_PWR_KEY` / `RG_GPIO_PWR_HOLD` — soft-power key and latch (BLE target only)

> **Panel note:** some ST7789 clones read the MADCTL BGR bit with **inverted polarity** (0xB0 = blue in the high bits, 0xB8 = red in the high bits). This target uses `rgb_order = false` (MADCTL 0xB8) and `lcd_send_buffer()` only undoes the byte pre-swap. If your panel shows red/blue swapped, flip `RG_LGFX_RGB_ORDER`.

## Project Structure

```
├── platformio.ini              # PlatformIO environment, flags, include paths
├── partitions_nes.csv          # NVS / OTA / 4 MB app partition table
├── startup.png                 # boot splash (embedded at build time, decoded with lodepng)
├── lib/LovyanGFX/              # local LovyanGFX copy (large CJK fonts excluded)
├── lib/NimBLE-Arduino/         # local NimBLE-Arduino 1.4.3 (BLE target only)
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
        ├── rg_pwrkey.c         # soft-power key: latch at boot, ~2 s hold to shut down
        ├── drivers/input/
        │   └── ble_gamepad.{h,cpp}       # NimBLE HID gamepad host
        ├── drivers/display/
        │   ├── lovyangfx_bridge.{h,cpp}  # C++ bridge for rg_display.c
        │   └── lovyangfx.h               # lcd_* C API
        ├── fonts/efontCN16.c   # 16x16 Chinese bitmap font (7,544 glyphs)
        └── targets/
            ├── my-s3-st7789/config.h     # 8-button handheld
            └── my-s3-st7789-ble/config.h # Waveshare ESP32-S3-Touch-LCD-2.8 (BLE)
```

## Architecture Notes

- **Boot path:** `main.cpp` → `rg_system_init()` (display, audio, input, storage, settings) → `startup.png` boot splash (cleared after ~1.5 s) → `nes_main()` which never returns.
- **Rendering:** two `RG_PIXEL_PAL565_BE` surfaces are written alternately; `blit_screen()` applies overscan/auto-crop, then `rg_display_submit()` streams to the panel. The palette is byte-swapped at build time so both the GUI and game paths share one byte-order convention.
- **LovyanGFX bridge:** `rg_display.c` is plain C, so `lovyangfx_bridge.cpp` exposes a small `extern "C"` API (`rg_lgfx_*`). Streaming uses `setWindow()` wrapped in `startWrite()/endWrite()` (the low-level window command does not open a SPI transaction itself) followed by `pushPixels()`.
- **BLE input:** `ble_gamepad.cpp` owns a NimBLE scan/connect task and publishes the pad's keys through `rg_ble_gamepad_get_state()`, which `rg_input.c` ORs into the regular gamepad state. On targets without `RG_INPUT_BLE_GAMEPAD` the implementation compiles out.
- **GUI text engine:** custom bitmap-font renderer in `rg_gui.c` with UTF-8 decoding, multiline layout, alignment and `RG_TEXT_ELLIPSIS` single-line truncation.
- **Chinese font:** `efontCN16.c` is generated from u8g2's `lgfx_efont_cn_16` table and stored as fixed-stride records so `find_glyph()` can binary-search it.

## Credits & License

This project stands on the shoulders of:

- [Retro-Go](https://github.com/ducalex/retro-go) by ducalex — framework, GUI, settings, audio pipeline
- [nofrendo](https://github.com/ducalex/retro-go/tree/dev/cores) — NES emulation core
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) by lovyan03 — display driver
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) by h2zero — BLE HID host for the gamepad
- [DIJI-NES](https://github.com/UF-Evan/DIJI-NES) by UF-Evan — 8-button handheld hardware design and pinout reference
- [Waveshare ESP32-S3-Touch-LCD-2.8](https://docs.waveshare.net/ESP32-S3-Touch-LCD-2.8/) — the 2.8" test board and its official pinout
- [u8g2 / efont](https://github.com/olikraus/u8g2) by olikraus — Chinese glyph data
- [cJSON](https://github.com/DaveGamble/cJSON) and [lodepng](https://lodev.org/lodepng/)

Retro-Go and nofrendo are licensed under **GPL-2.0**, so this project is distributed under the same license. See `LICENSE` for the full text.
