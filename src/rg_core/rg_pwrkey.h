#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 电源键管理（软开关电源电路，参考 NewESP32_260501 的 PWR_Key.cpp）：
//  - 开机时若电源键处于按下状态，拉高 RG_GPIO_PWR_HOLD 锁存供电，
//    此后可松开电源键；若未按下则不锁存（电池态下松手即断电，USB 态正常）。
//  - 运行期间后台任务监测电源键，按住约 2 秒触发关机（灭背光 + 释放锁存）。
// 需要在目标 config.h 中定义 RG_GPIO_PWR_KEY / RG_GPIO_PWR_HOLD。
void rg_pwrkey_init(void);

// 请求关机：灭背光并释放电源锁存（USB 供电时硬件无法断电，将原地等待）。
void rg_pwrkey_shutdown(void);

// 电源键当前是否按下（低电平有效）。
// 仅供恢复菜单使用：开机早期 BLE 手柄尚未连接，只有物理按键可操作，此时电源键
// 充当“确认”键。它不在 gamepad keymap 里，因此 ROM 选择、菜单和游戏都收不到它。
bool rg_pwrkey_is_down(void);

#ifdef __cplusplus
}
#endif
