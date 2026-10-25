// Target definition: ESP32-S3 + ST7789 240x320 SPI display, 2.8" BLE variant
// Pinout taken from the DIJI-NES-0.5.0-2.8 hardware (its src/main.cpp and
// src/lgfx_conf.h are the reference, NOT the original DIJI-NES 0.5.0 README
// pinout). The 2.8" variant re-wires LCD/SD/buttons/I2S and shares the A/B
// buttons on one GPIO and the LEFT/SELECT buttons on another.
// Built with: pio run -e nes-s3-st7789-ble (-DRG_TARGET_MY_S3_ST7789_BLE=1)
#define RG_TARGET_NAME             "MY-S3-ST7789-BLE"

// ---------------- Board feature switches (defined here, consumed by rg_core) ----------------
// Boot logo: startup.png is embedded via board_build.embed_files (platformio.ini)
// and decoded at boot. The embed and this switch must be enabled together.
#define RG_BOOT_LOGO               1
// Recovery menu is driven by two buttons on this board: BOOT (GPIO 0, seen as
// RG_KEY_RIGHT) cycles the selection and the power key activates it. Recovery is
// the only place the power key acts as a button (the BLE gamepad isn't connected
// yet that early in boot); rg_system.c samples it directly instead of mapping it
// to A/B, so a short press can never reach the file picker or a running game.
#define RG_RECOVERY_TWO_KEY_UI     1

// ---------------- Storage (SD card on SPI) ----------------
#define RG_STORAGE_ROOT             "/sd"
// SD lives on SPI2 (FSPI) so the panel can own SPI3, exactly like the
// DIJI-NES-0.5.0-2.8 firmware (SPIClass sdSPI(FSPI)).
#define RG_STORAGE_SDSPI_HOST       SPI2_HOST
// The 2.8 firmware runs the card at 5 MHz (SD_FREQ 5000000); that wiring is
// not verified above that speed.
#define RG_STORAGE_SDSPI_SPEED      5000 // kHz
// Put ROMs in /sd/nes/*.nes (or .zip)

// ---------------- Audio (external I2S DAC, e.g. MAX98357A) ----------------
#define RG_AUDIO_USE_INT_DAC        0   // ESP32-S3 has no internal DAC
#define RG_AUDIO_USE_EXT_DAC        1

// ---------------- Video (ST7789, 240x320 panel used in landscape) ----------------
#define RG_SCREEN_DRIVER            2   // 0 = ILI9341/ST7789 shared driver, 2 = LovyanGFX (ST7789)
#define RG_SCREEN_HOST              SPI3_HOST // Same SPI host as the DIJI-NES-0.5.0-2.8 firmware (lgfx_conf.h)
#define RG_SCREEN_SPEED             SPI_MASTER_FREQ_80M // 2.8 firmware writes at 80MHz; lower to 26M/40M if flaky
#define RG_SCREEN_BACKLIGHT         1   // Backlight is on GPIO 5 (2.8 firmware drives it HIGH): PWM dimming + Brightness option
#define RG_SCREEN_WIDTH             320 // Landscape: NES is 256x240, so 320x240 fits best
#define RG_SCREEN_HEIGHT            240
#define RG_SCREEN_ROTATE            0   // Rotation is handled by LovyanGFX (see RG_LGFX_ROTATION)
#define RG_SCREEN_VISIBLE_AREA      {0, 0, 0, 0}
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}

// LovyanGFX panel options (only used when RG_SCREEN_DRIVER == 2)
#define RG_LGFX_ROTATION            1   // 1 = landscape 320x240, same as the 2.8 firmware (tft.setRotation(1)). Use 3 if the image is upside down.
#define RG_LGFX_INVERT              true  // 2.8 firmware sets invert=true (INVON 0x21). Set to false if colors look inverted.
#define RG_LGFX_RGB_ORDER           false // 2.8 firmware keeps rgb_order=false. On the previous 0.5.0 panel this made the
                                          // panel read the wire value as standard RGB565 (its BGR bit is inverted vs a
                                          // stock ST7789, verified with test patterns), so lcd_send_buffer() only needs to
                                          // undo rg_display's byte pre-swap - no R/B channel swap (see the bridge). Re-check
                                          // colors if the 2.8" module behaves differently.
#define RG_LGFX_BOOT_TEST           0     // Boot colour flash via the streaming path (verified: red/green/blue/white). Set to 1 to re-check the pipeline.

// Init sequence for the ili9341.h driver (RG_SCREEN_DRIVER == 0/1). The
// LovyanGFX path (RG_SCREEN_DRIVER == 2) uses the stock ST7789 sequence built
// into lgfx::Panel_ST7789, i.e. the same one DIJI-NES runs on this panel.
#if RG_SCREEN_DRIVER == 0 || RG_SCREEN_DRIVER == 1
// NOTE: lcd_init() already sends 0x01 (reset), 0x3A 0x05 (RGB565) and then,
// after this macro, 0x11 (sleep out) + 0x29 (display on).
// DIJI-NES/lgfx sets invert=true, matching the 0x21 (INVON) below.
#define RG_SCREEN_INIT()                                                                                         \
    ILI9341_CMD(0x11);              /* SLPOUT */                                                                  \
    rg_usleep(120 * 1000);                                                                                        \
    ILI9341_CMD(0x36, 0xA0);        /* MADCTL: landscape rotated 180 (MV|MY), RGB order (this panel needs BGR=0!). Other rotations: 0x20, 0x60, 0xC0 */      \
    ILI9341_CMD(0xB2, 0x0C, 0x0C, 0x00, 0x33, 0x33); /* PORCTRL (porch control) */                                \
    ILI9341_CMD(0xB7, 0x35);        /* GCTRL (gate control) */                                                    \
    ILI9341_CMD(0xBB, 0x19);        /* VCOMS (VCOM setting) */                                                    \
    ILI9341_CMD(0xC0, 0x2C);        /* LCMCTRL */                                                                 \
    ILI9341_CMD(0xC2, 0x01);        /* VDVVRHEN */                                                                \
    ILI9341_CMD(0xC3, 0x12);        /* VRHS (GVDD=4.45V) */                                                       \
    ILI9341_CMD(0xC4, 0x20);        /* VDVS */                                                                    \
    ILI9341_CMD(0xC6, 0x0F);        /* FRCTRL2: 60Hz */                                                           \
    ILI9341_CMD(0xD0, 0xA4, 0xA1);  /* PWCTRL1 */                                                 \
    ILI9341_CMD(0xE0, 0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23);       \
    ILI9341_CMD(0xE1, 0xD0, 0x04, 0x0C, 0x11, 0x33, 0x44, 0x44, 0x38, 0x0F, 0x18, 0x16, 0x1B, 0x1F);             \
    ILI9341_CMD(0x21);              /* INVON: required (DIJI-NES invert=true). Remove if colors are inverted! */
#endif

// ---------------- Input (active-low GPIO buttons) ----------------
// The 2.8 hardware shares GPIOs: LEFT/SELECT on GPIO 43, so pressing that one
// button registers both keys at once (same as the 2.8 firmware, where
// LEFT_BUTTON==SELECT_BUTTON).
// GPIO 6 is the soft-power key ONLY (see RG_GPIO_PWR_KEY below). It used to be
// mapped to A and B as well, which made a short press of the power key act as an
// "A" press (e.g. launching the selected ROM from the file picker). A/B now come
// from the BLE gamepad alone; the board has no separate A/B buttons.
// MENU has no physical key on this hardware: START + LEFT/SELECT combo = MENU
// (see RG_GAMEPAD_VIRT_MAP)
#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_UP,     .num = GPIO_NUM_15, .pullup = 1, .level = 0},\
    {RG_KEY_DOWN,   .num = GPIO_NUM_18, .pullup = 1, .level = 0},\
    {RG_KEY_LEFT,   .num = GPIO_NUM_43, .pullup = 1, .level = 0},\
    {RG_KEY_RIGHT,  .num = GPIO_NUM_0,  .pullup = 1, .level = 0},\
    {RG_KEY_SELECT, .num = GPIO_NUM_43, .pullup = 1, .level = 0},\
    {RG_KEY_START,  .num = GPIO_NUM_44, .pullup = 1, .level = 0},\
}

// START + LEFT/SELECT together (with no other key) opens the game menu. SELECT
// shares its GPIO with LEFT, so the exact raw state for the combo is
// START|SELECT|LEFT (the virtual map only fires on an exact match).
// The second entry is for the BLE gamepad: its START + SELECT (no LEFT, since
// the pad has separate buttons) is also MENU. The GPIO combo can never produce
// a plain START|SELECT (SELECT implies LEFT there), so the two never clash.
#define RG_GAMEPAD_VIRT_MAP {\
    {RG_KEY_MENU, RG_KEY_START | RG_KEY_SELECT | RG_KEY_LEFT},\
    {RG_KEY_MENU, RG_KEY_START | RG_KEY_SELECT},\
}

// Recovery menu (Reset settings / Restart / Power off) trigger at boot: the
// BOOT key only (GPIO 0, seen as RG_KEY_RIGHT above). The stock default
// RG_KEY_ANY also matches the power key (GPIO 6 = A/B), so holding the power
// key at boot used to drop into the recovery menu instead of starting up.
// Hold BOOT for ~0.5s while powering on to enter the menu. NOTE: BOOT is on
// GPIO 0 (a strapping pin), so a board without strapping protection may drop
// into the ROM download mode instead (black screen). Fallback: short-press
// BOOT while the boot logo is visible (~1.5s window, main.cpp).
#define RG_RECOVERY_BTN             RG_KEY_RIGHT

// ---------------- BLE gamepad (Shanwan Q36, Android/BLE mode) ----------------
// NimBLE scans in the background and merges the pad's keys into the regular
// input state (dpad/A/B/X->B/Y->A/Select/Start; START+SELECT = MENU above).
// The pad must be in Android mode (the mode it pairs over BLE, NOT the
// X-input/Switch modes). Report layout and pairing flow verified on the
// DIJI-NES-0.5.0-BLE firmware.
#define RG_INPUT_BLE_GAMEPAD      1

// ---------------- Battery (ADC on GPIO 8) ----------------
// Mirrors the DIJI-NES-2.8 BAT_Driver:
//   pin = 8, divider 1:3, calibration factor 0.990476, i.e.
//   volts = analogReadMilliVolts(8) * 3.0 / 1000.0 / 0.990476
// GPIO 8 is ADC1 channel 7 on the ESP32-S3 and is not used by the LCD, SD,
// I2S, buttons or the power latch. rg_input.c feeds that channel through
// esp_adc_cal_raw_to_voltage() with the same 12-bit / 11dB attenuation that
// Arduino's analogReadMilliVolts() uses, so the constants below are equivalent.
#define RG_BATTERY_DRIVER           1
#define RG_BATTERY_ADC_UNIT         ADC_UNIT_1
#define RG_BATTERY_ADC_CHANNEL      ADC_CHANNEL_7
// "raw" is the divider output in mV: undo the 1:3 ratio, then apply the
// 0.990476 offset (the measured divider reads ~1% low).
#define RG_BATTERY_CALC_VOLTAGE(raw) (((raw) * 3.0f / 1000.0f) / 0.990476f)
// Linear 3.0V (0%) .. 4.2V (100%) of the cell. A Li-ion discharge curve would
// be more accurate, but the icon only distinguishes >40% / >20% / below anyway.
// Out-of-range results are clamped by rg_input_read_battery_raw().
#define RG_BATTERY_CALC_PERCENT(raw) ((RG_BATTERY_CALC_VOLTAGE(raw) - 3.0f) / (4.2f - 3.0f) * 100.0f)

// ---------------- SPI display pins (DIJI-NES-2.8: SCLK 40, MOSI 45, MISO 46, DC 41, CS 42, RST 39) ----------------
#define RG_GPIO_LCD_MISO            GPIO_NUM_46
#define RG_GPIO_LCD_MOSI            GPIO_NUM_45
#define RG_GPIO_LCD_CLK             GPIO_NUM_40
#define RG_GPIO_LCD_CS              GPIO_NUM_42
#define RG_GPIO_LCD_DC              GPIO_NUM_41
#define RG_GPIO_LCD_RST             GPIO_NUM_39
// Backlight enable on GPIO 5 (the 2.8 firmware drives it HIGH; here it becomes
// a PWM dimmable output with a Brightness menu entry).
#define RG_GPIO_LCD_BCKL            GPIO_NUM_5

// ---------------- SD card pins (SPI2/FSPI, DIJI-NES-2.8: CS 21, SCLK 14, MISO 16, MOSI 17) ----------------
#define RG_GPIO_SDSPI_MISO          GPIO_NUM_16
#define RG_GPIO_SDSPI_MOSI          GPIO_NUM_17
#define RG_GPIO_SDSPI_CLK           GPIO_NUM_14
#define RG_GPIO_SDSPI_CS            GPIO_NUM_21

// ---------------- I2S DAC pins (DIJI-NES-2.8, MAX98357A: BCLK 48, LRCLK 38, DATA 47) ----------------
#define RG_GPIO_SND_I2S_BCK         GPIO_NUM_48
#define RG_GPIO_SND_I2S_WS          GPIO_NUM_38
#define RG_GPIO_SND_I2S_DATA        GPIO_NUM_47

// ---------------- Power key (soft-power circuit, same as NewESP32_260501) ----------------
// The power key shorts the supply on at boot; firmware must latch it by driving
// PWR_HOLD high, or the device powers off as soon as the key is released.
// KEY is active-low with an external pullup on the board. This is a power key:
// a short press does nothing except in the recovery menu (where it confirms the
// selection, since the BLE gamepad isn't connected yet), and a ~2s hold shuts the
// device down. It is NOT mapped to any gamepad key (it used to double as A/B,
// which made a short press launch the selected ROM). rg_pwrkey.c monitors it on
// every boot path, so a hold works even when USB powered (where the latch stays off).
#define RG_GPIO_PWR_KEY             GPIO_NUM_6
#define RG_GPIO_PWR_HOLD            GPIO_NUM_7
