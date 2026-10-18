/*
 * Retro-Go NES port entry point for PlatformIO + Arduino (ESP32-S3).
 *
 * Replaces retro-core/main/main.c: rg_system_init() performs all global
 * hardware initialization (display, audio, input, storage, settings),
 * then nes_main() runs the emulator and never returns.
 */
#include <Arduino.h>
#include "rg_system.h"
#include "shared.h"

static void emulator_task(void *arg)
{
    rg_system_init(AUDIO_SAMPLE_RATE, NULL, NULL);
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
