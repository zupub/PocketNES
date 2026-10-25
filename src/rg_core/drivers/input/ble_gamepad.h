#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 初始化 NimBLE 并启动后台扫描/连接任务（非阻塞，扫描在低优先级任务中进行）
bool rg_ble_gamepad_init(void);
void rg_ble_gamepad_deinit(void);

// 返回 BLE 手柄当前按下的 RG_KEY_* 位掩码（未连接时为 0）
uint32_t rg_ble_gamepad_get_state(void);

bool rg_ble_gamepad_is_connected(void);

#ifdef __cplusplus
}
#endif
