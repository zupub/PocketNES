// Target definition: ESP32-S3 + ST7789 240x320 SPI display
// Pinout taken from the DIJI-NES hardware (see DIJI-NES README.md)
#define RG_TARGET_NAME             "MY-S3-ST7789"

// Boot logo: startup.png is embedded via board_build.embed_files (platformio.ini)
// and decoded at boot. The embed and this switch must be enabled together.
#define RG_BOOT_LOGO               1

// ---------------- Storage (SD card on SPI) ----------------
#define RG_STORAGE_ROOT             "/sd"
// SD lives on SPI2 (FSPI) so the panel can own SPI3, exactly like the
// DIJI-NES 0.5.0 firmware. LovyanGFX hijacks the Arduino SPI object when it
// is asked to use SPI2, so the two must not share a host.
#define RG_STORAGE_SDSPI_HOST       SPI2_HOST
#define RG_STORAGE_SDSPI_SPEED      SDMMC_FREQ_DEFAULT
// Put ROMs in /sd/nes/*.nes (or .zip)

// ---------------- Audio (external I2S DAC, e.g. MAX98357A) ----------------
#define RG_AUDIO_USE_INT_DAC        0   // ESP32-S3 has no internal DAC
#define RG_AUDIO_USE_EXT_DAC        1

// ---------------- Video (ST7789, 240x320 panel used in landscape) ----------------
#define RG_SCREEN_DRIVER            2   // 0 = ILI9341/ST7789 shared driver, 2 = LovyanGFX (ST7789)
#define RG_SCREEN_HOST              SPI3_HOST // Same SPI host as the DIJI-NES 0.5.0 firmware
#define RG_SCREEN_SPEED             SPI_MASTER_FREQ_80M // DIJI-NES hardware is verified at 80MHz; lower to 26M/40M if flaky
#define RG_SCREEN_BACKLIGHT         0   // Panel backlight is hardwired (no GPIO), hides the Brightness option
#define RG_SCREEN_WIDTH             320 // Landscape: NES is 256x240, so 320x240 fits best
#define RG_SCREEN_HEIGHT            240
#define RG_SCREEN_ROTATE            0   // Rotation is handled by LovyanGFX (see RG_LGFX_ROTATION)
#define RG_SCREEN_VISIBLE_AREA      {0, 0, 0, 0}
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}

// LovyanGFX panel options (only used when RG_SCREEN_DRIVER == 2)
#define RG_LGFX_ROTATION            3   // 3 = landscape 320x240 (MADCTL 0xB0 = MV|MY|ML, same as the old 0xA0)
#define RG_LGFX_INVERT              true  // DIJI-NES panel needs INVON (0x21). Set to false if colors look inverted.
#define RG_LGFX_RGB_ORDER           false // false keeps MADCTL at 0xB8 (same as DIJI-NES) and makes the panel read the
                                          // wire value as standard RGB565. This panel's BGR bit is inverted vs a stock
                                          // ST7789 (0xB0 = blue in the high bits, 0xB8 = red in the high bits, both
                                          // verified with test patterns), so with 0xB8 lcd_send_buffer() only needs to
                                          // undo rg_display's byte pre-swap - no R/B channel swap (see the bridge).
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

// ---------------- Input (active-low GPIO buttons, DIJI-NES has 8 keys) ----------------
// MENU has no physical key on this hardware: START+SELECT combo = MENU (see RG_GAMEPAD_VIRT_MAP)
#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_UP,     .num = GPIO_NUM_17, .pullup = 1, .level = 0},\
    {RG_KEY_DOWN,   .num = GPIO_NUM_3,  .pullup = 1, .level = 0},\
    {RG_KEY_LEFT,   .num = GPIO_NUM_8,  .pullup = 1, .level = 0},\
    {RG_KEY_RIGHT,  .num = GPIO_NUM_18, .pullup = 1, .level = 0},\
    {RG_KEY_A,      .num = GPIO_NUM_48, .pullup = 1, .level = 0},\
    {RG_KEY_B,      .num = GPIO_NUM_47, .pullup = 1, .level = 0},\
    {RG_KEY_SELECT, .num = GPIO_NUM_16, .pullup = 1, .level = 0},\
    {RG_KEY_START,  .num = GPIO_NUM_15, .pullup = 1, .level = 0},\
}

// START+SELECT together (with no other key) opens the game menu
#define RG_GAMEPAD_VIRT_MAP {\
    {RG_KEY_MENU, RG_KEY_START | RG_KEY_SELECT},\
}

// ---------------- Battery (disabled) ----------------
#define RG_BATTERY_DRIVER           0

// ---------------- SPI display pins (DIJI-NES) ----------------
#define RG_GPIO_LCD_MISO            GPIO_NUM_NC
#define RG_GPIO_LCD_MOSI            GPIO_NUM_13
#define RG_GPIO_LCD_CLK             GPIO_NUM_14
#define RG_GPIO_LCD_CS              GPIO_NUM_10
#define RG_GPIO_LCD_DC              GPIO_NUM_11
#define RG_GPIO_LCD_RST             GPIO_NUM_12
// No backlight GPIO on this hardware (tied to VCC). Do NOT define RG_GPIO_LCD_BCKL here,
// it would make the driver run LEDC fade on an undefined pin.

// ---------------- SD card pins (SPI2, DIJI-NES) ----------------
#define RG_GPIO_SDSPI_MISO          GPIO_NUM_39
#define RG_GPIO_SDSPI_MOSI          GPIO_NUM_41
#define RG_GPIO_SDSPI_CLK           GPIO_NUM_40
#define RG_GPIO_SDSPI_CS            GPIO_NUM_42

// ---------------- I2S DAC pins (DIJI-NES, MAX98357A) ----------------
#define RG_GPIO_SND_I2S_BCK         GPIO_NUM_5
#define RG_GPIO_SND_I2S_WS          GPIO_NUM_4
#define RG_GPIO_SND_I2S_DATA        GPIO_NUM_6
