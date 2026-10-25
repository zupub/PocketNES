// Shanwan Q36 BLE 游戏手柄驱动（NimBLE-Arduino 1.4.3）
//
// 连接流程参考 DIJI-NES-0.5.0-BLE 的 joystick.cpp：扫描 -> 连接 HID 服务
// (0x1812) -> 订阅 Report 特征 (0x2A4D) -> 在通知回调中解析按键。
// 与其不同的是：本工程全程（菜单 + 游戏）使用 BLE 输入，因此连接建立后
// 保持不断开，断线后由后台任务自动清理并重新扫描连接。
//
// Q36 HID 输入报告布局（实测）：
//   data[4]: Hat switch  0=上 1=右上 2=右 3=右下 4=下 5=左下 6=左 7=左上 >=8 松开
//   data[5]: bit0=A bit1=B bit3=X bit4=Y
//   data[6]: bit2=Select bit3=Start
//
// 按键映射：A->A B->B X->B Y->A（X/Y 冗余映射为 NES 的 B/A，兼顾 SNES 布局握持习惯）
// 菜单呼出：Start+Select 组合键（在 RG_GAMEPAD_VIRT_MAP 中转换为 RG_KEY_MENU）
#include "rg_system.h" // pulls in the target config.h -> RG_INPUT_BLE_GAMEPAD

// NOTE: PlatformIO's LDF preprocessor does not know macros passed via
// build_flags, so anything #include'd inside `#if RG_INPUT_BLE_GAMEPAD`
// would be invisible to it and NimBLE-Arduino would not be auto-detected
// from lib/ (an explicit lib_deps would be required instead). Keep all
// includes unconditional - they only pull declarations - and let the
// macro trim the implementation below.
#include "drivers/input/ble_gamepad.h"

#include <algorithm>
#include <string>

#include <NimBLEDevice.h>

#include "rg_input.h" // rg_key_t

#if RG_INPUT_BLE_GAMEPAD // entire driver compiles out on targets without BLE

#define BLE_TASK_STACK (8 * 1024)
#define BLE_SCAN_DURATION_MS 5000

static NimBLEUUID HID_SERVICE_UUID("1812");
static NimBLEUUID REPORT_CHAR_UUIDS[] = {
    NimBLEUUID("2A4D"), // Report（Q36 实测命中）
    NimBLEUUID("2A8A"), // 游戏手柄输入
    NimBLEUUID("2A4E"), // Report Map（兜底）
    NimBLEUUID("2A4F"), // Report Reference（兜底）
};

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_state = 0;
static volatile bool s_connected = false;
static volatile bool s_running = false;

// 仅在 BLE 任务上下文访问（扫描 start() 同步阻塞，返回后回调不再执行，无竞态）
static NimBLEAddress *s_target_address = nullptr;
static NimBLEClient *s_client = nullptr;
static NimBLERemoteCharacteristic *s_report_char = nullptr;

static void set_state(uint32_t state)
{
    portENTER_CRITICAL(&s_mux);
    s_state = state;
    portEXIT_CRITICAL(&s_mux);
}

// 通知回调：运行于 NimBLE host 任务，仅做报告解析，不做耗时操作
static void on_report_notify(NimBLERemoteCharacteristic *, uint8_t *pData, size_t length, bool)
{
    if (length < 6)
        return; // 异常长度，忽略该报告

    uint32_t state = 0;

    switch (pData[4]) // Hat switch
    {
    case 0: state |= RG_KEY_UP; break;
    case 1: state |= RG_KEY_UP | RG_KEY_RIGHT; break;
    case 2: state |= RG_KEY_RIGHT; break;
    case 3: state |= RG_KEY_RIGHT | RG_KEY_DOWN; break;
    case 4: state |= RG_KEY_DOWN; break;
    case 5: state |= RG_KEY_DOWN | RG_KEY_LEFT; break;
    case 6: state |= RG_KEY_LEFT; break;
    case 7: state |= RG_KEY_LEFT | RG_KEY_UP; break;
    default: break; // >=8 松开
    }

    uint8_t buttons = pData[5];
    if (buttons & (1 << 0)) state |= RG_KEY_A; // A
    if (buttons & (1 << 1)) state |= RG_KEY_B; // B
    if (buttons & (1 << 3)) state |= RG_KEY_B; // X -> B
    if (buttons & (1 << 4)) state |= RG_KEY_A; // Y -> A

    if (length >= 7)
    {
        buttons = pData[6];
        if (buttons & (1 << 2)) state |= RG_KEY_SELECT;
        if (buttons & (1 << 3)) state |= RG_KEY_START;
    }

    set_state(state);
}

class GamepadScanCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
    void onResult(NimBLEAdvertisedDevice *device) override
    {
        std::string name = device->getName();
        if (name.empty())
            return;

        std::string lowered(name);
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);

        // Q36 广播名为 "Q36 for Android"（Android/BLE 模式）
        if (lowered.find("q36") == std::string::npos &&
            lowered.find("shanwan") == std::string::npos &&
            lowered.find("gamepad") == std::string::npos &&
            lowered.find("controller") == std::string::npos &&
            lowered.find("android") == std::string::npos)
            return;

        RG_LOGI("ble_gamepad: found '%s' (%s)\n", name.c_str(), device->getAddress().toString().c_str());
        NimBLEDevice::getScan()->stop();

        delete s_target_address;
        s_target_address = new NimBLEAddress(device->getAddress());
    }
};

class GamepadSecurityCallbacks : public NimBLESecurityCallbacks
{
    uint32_t onPassKeyRequest() override { return 123456; }
    bool onConfirmPIN(uint32_t pass_key) override
    {
        RG_LOGI("ble_gamepad: confirm PIN %lu\n", (unsigned long)pass_key);
        return true;
    }
    void onPassKeyNotify(uint32_t pass_key) override
    {
        RG_LOGI("ble_gamepad: passkey notify %lu\n", (unsigned long)pass_key);
    }
    bool onSecurityRequest() override { return true; }
    void onAuthenticationComplete(ble_gap_conn_desc *desc) override
    {
        RG_LOGI("ble_gamepad: auth complete encrypted=%d bonded=%d\n",
                desc->sec_state.encrypted, desc->sec_state.bonded);
    }
};

static GamepadScanCallbacks s_scan_callbacks;
static GamepadSecurityCallbacks s_security_callbacks;

static void cleanup_connection(void)
{
    s_connected = false;
    set_state(0);

    s_report_char = nullptr;
    if (s_client)
    {
        if (s_client->isConnected())
            s_client->disconnect();
        NimBLEDevice::deleteClient(s_client);
        s_client = nullptr;
    }
    delete s_target_address;
    s_target_address = nullptr;
}

static bool connect_gamepad(void)
{
    RG_LOGI("ble_gamepad: connecting to %s\n", s_target_address->toString().c_str());

    s_client = NimBLEDevice::createClient();
    s_client->setConnectTimeout(15);

    if (!s_client->connect(*s_target_address))
    {
        RG_LOGW("ble_gamepad: connect failed\n");
        return false;
    }

    // 主动发起配对（部分手柄要求加密后才上报通知）
    s_client->secureConnection();

    NimBLERemoteService *service = s_client->getService(HID_SERVICE_UUID);
    if (service == nullptr)
    {
        RG_LOGW("ble_gamepad: HID service not found\n");
        return false;
    }

    for (const NimBLEUUID &uuid : REPORT_CHAR_UUIDS)
    {
        NimBLERemoteCharacteristic *chr = service->getCharacteristic(uuid);
        if (chr != nullptr && chr->canNotify())
        {
            s_report_char = chr;
            break;
        }
    }
    if (s_report_char == nullptr)
    {
        RG_LOGW("ble_gamepad: no report characteristic found\n");
        return false;
    }

    if (!s_report_char->subscribe(true, on_report_notify))
    {
        RG_LOGW("ble_gamepad: subscribe failed\n");
        return false;
    }

    s_connected = true;
    RG_LOGI("ble_gamepad: connected, listening for reports\n");
    return true;
}

static void ble_task(void *)
{
    while (s_running)
    {
        if (s_target_address != nullptr)
        {
            if (connect_gamepad())
            {
                while (s_running && s_client->isConnected())
                    vTaskDelay(pdMS_TO_TICKS(200));
                RG_LOGI("ble_gamepad: disconnected\n");
            }
            cleanup_connection(); // 失败或断线后清理并重新扫描
        }
        else
        {
            RG_LOGD("ble_gamepad: scanning...\n");
            NimBLEScan *scan = NimBLEDevice::getScan();
            scan->setAdvertisedDeviceCallbacks(&s_scan_callbacks);
            scan->setActiveScan(true);
            scan->setInterval(100);
            scan->setWindow(99);
            scan->setMaxResults(0);
            scan->start(BLE_SCAN_DURATION_MS / 1000, false);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    cleanup_connection();
}

bool rg_ble_gamepad_init(void)
{
    if (s_running)
        return true;

    RG_LOGI("ble_gamepad: initializing NimBLE\n");
    NimBLEDevice::init("");
    NimBLEDevice::setSecurityAuth(false, false, false);
    NimBLEDevice::setSecurityCallbacks(&s_security_callbacks);

    s_running = true;
    rg_task_create("rg_blepad", &ble_task, NULL, BLE_TASK_STACK, RG_TASK_PRIORITY_2, 0);
    return true;
}

void rg_ble_gamepad_deinit(void)
{
    s_running = false; // ble_task 检测到后自行清理退出
}

uint32_t rg_ble_gamepad_get_state(void)
{
    portENTER_CRITICAL(&s_mux);
    uint32_t state = s_state;
    portEXIT_CRITICAL(&s_mux);
    return state;
}

bool rg_ble_gamepad_is_connected(void)
{
    return s_connected;
}

#endif // RG_INPUT_BLE_GAMEPAD
