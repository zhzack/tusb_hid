#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"

static const char *TAG = "usb_hid_mouse";

/************* TinyUSB 描述符 ****************/

// 报告描述符：只定义鼠标（不含键盘）
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(1))
};

// 配置描述符：1 个接口，含鼠标 HID
static const uint8_t hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 0, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

// TinyUSB 回调：返回报告描述符
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    return hid_report_descriptor;
}

// 空实现：未使用 GET_REPORT 回调
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    return 0;
}

// 空实现：未使用 SET_REPORT 回调
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
}

/************* 鼠标移动逻辑 ****************/

typedef enum {
    MOUSE_DIR_RIGHT,
    MOUSE_DIR_DOWN,
    MOUSE_DIR_LEFT,
    MOUSE_DIR_UP,
    MOUSE_DIR_MAX,
} mouse_dir_t;

#define DISTANCE_MAX        125  // 每段最大移动距离
#define DELTA_SCALAR        5    // 每步移动距离

// 生成绘制正方形的下一步移动值
static void mouse_draw_square_next_delta(int8_t *dx, int8_t *dy)
{
    static mouse_dir_t dir = MOUSE_DIR_RIGHT;
    static uint32_t distance = 0;

    switch (dir) {
        case MOUSE_DIR_RIGHT: *dx = DELTA_SCALAR; *dy = 0; break;
        case MOUSE_DIR_DOWN:  *dx = 0; *dy = DELTA_SCALAR; break;
        case MOUSE_DIR_LEFT:  *dx = -DELTA_SCALAR; *dy = 0; break;
        case MOUSE_DIR_UP:    *dx = 0; *dy = -DELTA_SCALAR; break;
        default: break;
    }

    distance += DELTA_SCALAR;
    if (distance >= DISTANCE_MAX) {
        distance = 0;
        dir = (dir + 1) % MOUSE_DIR_MAX;
    }
}

/************* 主程序入口 ****************/

void app_main(void)
{
    // 初始化 TinyUSB
    ESP_LOGI(TAG, "初始化 USB...");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .string_descriptor_count = 0,
        .external_phy = false,
#if (TUD_OPT_HIGH_SPEED)
        .fs_configuration_descriptor = hid_configuration_descriptor,
        .hs_configuration_descriptor = hid_configuration_descriptor,
        .qualifier_descriptor = NULL,
#else
        .configuration_descriptor = hid_configuration_descriptor,
#endif
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB 初始化完成");

    // 不断发送鼠标移动数据（绘制正方形轨迹）
    int8_t dx, dy;
    while (1) {
        if (tud_mounted()) { // USB 已连接主机
            mouse_draw_square_next_delta(&dx, &dy);
            tud_hid_mouse_report(1, 0x00, dx, dy, 0, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // 控制鼠标移动速度
    }
}
