// Trimmed version of retro-core/main/shared.h for the standalone NES build.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <rg_system.h>

#define AUDIO_SAMPLE_RATE   (32000)

#ifdef __cplusplus
extern "C" {
#endif

void nes_main();

#ifdef __cplusplus
}
#endif
