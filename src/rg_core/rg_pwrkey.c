// 电源键驱动：开机锁存供电 + 长按关机。
//
// 硬件为软开关电源电路（与 NewESP32_260501 相同）：电源键按下时硬件强制上电，
// 固件启动后必须拉高电源保持脚（PWR_HOLD）旁路按键完成锁存，否则松开按键即掉电；
// 关机则是拉低保持脚切断电源。逻辑移植自该工程的 PWR_Key.cpp/PWR_Loop()，
// 时基同样为 100ms，长按 20 个周期（2 秒）关机；其睡眠/重启两档不适用于本固件。
//
// 该按键是纯电源键：短按不产生任何游戏输入（它曾被映射为 A/B，导致短按即打开
// 选中的 ROM），长按则不论电池还是 USB 供电、不论当前在菜单还是游戏中都关机。
// 监控任务在每次启动时无条件创建，不依赖锁存是否成功。
#include "rg_pwrkey.h"

#include "rg_system.h" // 必须在前：RG_GPIO_PWR_KEY 等宏经 config.h 链才可见

#ifdef RG_GPIO_PWR_KEY

#include "rg_display.h"

#include <driver/gpio.h>

// 100ms 周期轮询（同参考工程的 DriverTask），按住 20 个周期 = 2 秒关机
#define PWR_TICK_MS         100
#define PWR_SHUTDOWN_TICKS  20
// 需要连续 3 个周期（300ms）读到松开才清零计数，滤掉按键抖动
#define PWR_RELEASE_TICKS   3

static void power_off(void)
{
    RG_LOGW("Power off: releasing the power latch...\n");
    gpio_hold_dis(RG_GPIO_PWR_HOLD);     // 先解除保持（hold 生效时 set_level 无效）
    gpio_set_level(RG_GPIO_PWR_HOLD, 0); // 释放锁存，硬件断电（电池供电到此结束）
    // 断电必须排在灭屏之前：开机早期也可能触发关机。本函数在 rg_system_init()
    // 之前就会被调用到，那时 LCD 和 settings 都还没就绪（rg_display_set_backlight
    // 会写设置），因此仅在显示初始化完成后才去灭屏。
    // USB 供电时电源无法被切断，灭背光作为视觉反馈，然后原地等待（拔线或复位离开）。
    if (rg_display_get_width() > 0)
        rg_display_set_backlight(RG_DISPLAY_BACKLIGHT_MIN);
    while (1)
        rg_task_delay(1000);
}

static void pwrkey_task(void *arg)
{
    int ticks = 0;
    int released = 0;

    // 开机时按键通常还按着（硬件靠它短接供电）。先等它松开再开始计时，
    // 否则“按住久一点开机”会变成“刚开机就立即关机”。
    while (gpio_get_level(RG_GPIO_PWR_KEY) == 0)
        rg_task_delay(PWR_TICK_MS);

    while (1)
    {
        if (gpio_get_level(RG_GPIO_PWR_KEY) == 0)
        {
            released = 0;
            if (++ticks >= PWR_SHUTDOWN_TICKS)
                power_off(); // never returns on battery power
        }
        else if (++released >= PWR_RELEASE_TICKS)
        {
            // 连续多个周期读到松开才清零：单个抖动样本不能打断长按倒计时，
            // 否则接触不良的按键可能永远凑不满 20 个周期。
            ticks = 0;
        }

        rg_task_delay(PWR_TICK_MS);
    }
}

void rg_pwrkey_shutdown(void)
{
    power_off();
}

bool rg_pwrkey_is_down(void)
{
    return gpio_get_level(RG_GPIO_PWR_KEY) == 0;
}

void rg_pwrkey_init(void)
{
    // 软件重启场景（恢复菜单重启/切换应用）：esp_restart() 会复位外设，
    // 若锁存脚在复位期间回到高阻，电池供电立即断开（表现为"重启变关机"）。
    // 因此锁存时同步调用 gpio_hold_en()，让电平跨过软件复位（上电复位自动失效）。
    bool hold_active = (gpio_get_level(RG_GPIO_PWR_HOLD) == 1);

    // 保持脚配置为输出；冷上电时先拉低：只有检测到电源键按下才锁存。
    // （hold 保持期间写电平无效，不会破坏软件重启保持下来的锁存。）
    gpio_set_direction(RG_GPIO_PWR_HOLD, GPIO_MODE_OUTPUT);
    if (!hold_active)
        gpio_set_level(RG_GPIO_PWR_HOLD, 0);

    // 按键低电平有效，板上自带外部上拉（参考工程仅设 INPUT）；内部上拉双保险
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << RG_GPIO_PWR_KEY,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    rg_task_delay(100); // 同参考工程：等电平稳定再采样

    if (hold_active || gpio_get_level(RG_GPIO_PWR_KEY) == 0)
    {
        gpio_set_level(RG_GPIO_PWR_HOLD, 1); // 锁存供电，此时可松开电源键
        gpio_hold_en(RG_GPIO_PWR_HOLD);      // 让锁存跨过 esp_restart（上电复位自动失效）
        RG_LOGI("Power latched (HOLD=GPIO%d), long-press to shutdown.\n", (int)RG_GPIO_PWR_HOLD);
    }
    else
    {
        RG_LOGI("Power key free at boot: not latching (USB powered run).\n");
    }

    // 监控任务必须无条件创建：以前只在锁存成功时创建，USB 供电启动后长按电源键
    // 毫无反应。不锁存时 power_off() 无法真正断电，但会灭屏并原地等待拔线，
    // 与恢复菜单的 "Power off" 行为一致。
    rg_task_create("rg_pwrkey", &pwrkey_task, NULL, 2 * 1024, RG_TASK_PRIORITY_2, 0);
}

#else // !RG_GPIO_PWR_KEY

void rg_pwrkey_init(void)
{
}

void rg_pwrkey_shutdown(void)
{
}

bool rg_pwrkey_is_down(void)
{
    return false;
}
#endif
