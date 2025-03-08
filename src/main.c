/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#if CONFIG_BT_BLE_ENABLED
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gatts_api.h"
#endif
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_hid_gap.h"
#include "esp_hidd.h"

static const char *TAG = "HID_DEV_DEMO";

typedef struct {
  TaskHandle_t task_hdl;
  esp_hidd_dev_t *hid_dev;
  uint8_t protocol_mode;
  uint8_t *buffer;
} local_param_t;

#if CONFIG_BT_BLE_ENABLED
static local_param_t s_ble_hid_param = {0};
static bool s_ble_is_connected = false;  // 添加连接状态变量

const unsigned char keyboardReportMap[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)
    0x85, 0x01,  //   Report ID (1)
    
    // 修饰键 (左Ctrl, 左Shift等)
    0x05, 0x07,  //   Usage Page (Key Codes)
    0x19, 0xE0,  //   Usage Minimum (Left Control)
    0x29, 0xE7,  //   Usage Maximum (Right GUI)
    0x15, 0x00,  //   Logical Minimum (0)
    0x25, 0x01,  //   Logical Maximum (1)
    0x75, 0x01,  //   Report Size (1)
    0x95, 0x08,  //   Report Count (8)
    0x81, 0x02,  //   Input (Data, Variable, Absolute)
    
    // 保留字节
    0x95, 0x01,  //   Report Count (1)
    0x75, 0x08,  //   Report Size (8)
    0x81, 0x01,  //   Input (Constant)
    
    // LED状态 (Num Lock, Caps Lock等)
    0x95, 0x05,  //   Report Count (5)
    0x75, 0x01,  //   Report Size (1)
    0x05, 0x08,  //   Usage Page (LEDs)
    0x19, 0x01,  //   Usage Minimum (Num Lock)
    0x29, 0x05,  //   Usage Maximum (Kana)
    0x91, 0x02,  //   Output (Data, Variable, Absolute)
    
    // LED状态的保留3位
    0x95, 0x01,  //   Report Count (1)
    0x75, 0x03,  //   Report Size (3)
    0x91, 0x01,  //   Output (Constant)
    
    // 6个按键
    0x95, 0x06,  //   Report Count (6)
    0x75, 0x08,  //   Report Size (8)
    0x15, 0x00,  //   Logical Minimum (0)
    0x25, 0x65,  //   Logical Maximum (101)
    0x05, 0x07,  //   Usage Page (Key Codes)
    0x19, 0x00,  //   Usage Minimum (0)
    0x29, 0x65,  //   Usage Maximum (101)
    0x81, 0x00,  //   Input (Data, Array)
    
    0xC0         // End Collection
};

const unsigned char mediaReportMap[] = {
    0x05,
    0x0C,  // Usage Page (Consumer)
    0x09,
    0x01,  // Usage (Consumer Control)
    0xA1,
    0x01,  // Collection (Application)
    0x85,
    0x03,  //   Report ID (3)
    0x09,
    0x02,  //   Usage (Numeric Key Pad)
    0xA1,
    0x02,  //   Collection (Logical)
    0x05,
    0x09,  //     Usage Page (Button)
    0x19,
    0x01,  //     Usage Minimum (0x01)
    0x29,
    0x0A,  //     Usage Maximum (0x0A)
    0x15,
    0x01,  //     Logical Minimum (1)
    0x25,
    0x0A,  //     Logical Maximum (10)
    0x75,
    0x04,  //     Report Size (4)
    0x95,
    0x01,  //     Report Count (1)
    0x81,
    0x00,  //     Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null
           //     Position)
    0xC0,  //   End Collection
    0x05,
    0x0C,  //   Usage Page (Consumer)
    0x09,
    0x86,  //   Usage (Channel)
    0x15,
    0xFF,  //   Logical Minimum (-1)
    0x25,
    0x01,  //   Logical Maximum (1)
    0x75,
    0x02,  //   Report Size (2)
    0x95,
    0x01,  //   Report Count (1)
    0x81,
    0x46,  //   Input (Data,Var,Rel,No Wrap,Linear,Preferred State,Null State)
    0x09,
    0xE9,  //   Usage (Volume Increment)
    0x09,
    0xEA,  //   Usage (Volume Decrement)
    0x15,
    0x00,  //   Logical Minimum (0)
    0x75,
    0x01,  //   Report Size (1)
    0x95,
    0x02,  //   Report Count (2)
    0x81,
    0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
           //   Position)
    0x09,
    0xE2,  //   Usage (Mute)
    0x09,
    0x30,  //   Usage (Power)
    0x09,
    0x83,  //   Usage (Recall Last)
    0x09,
    0x81,  //   Usage (Assign Selection)
    0x09,
    0xB0,  //   Usage (Play)
    0x09,
    0xB1,  //   Usage (Pause)
    0x09,
    0xB2,  //   Usage (Record)
    0x09,
    0xB3,  //   Usage (Fast Forward)
    0x09,
    0xB4,  //   Usage (Rewind)
    0x09,
    0xB5,  //   Usage (Scan Next Track)
    0x09,
    0xB6,  //   Usage (Scan Previous Track)
    0x09,
    0xB7,  //   Usage (Stop)
    0x15,
    0x01,  //   Logical Minimum (1)
    0x25,
    0x0C,  //   Logical Maximum (12)
    0x75,
    0x04,  //   Report Size (4)
    0x95,
    0x01,  //   Report Count (1)
    0x81,
    0x00,  //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null
           //   Position)
    0x09,
    0x80,  //   Usage (Selection)
    0xA1,
    0x02,  //   Collection (Logical)
    0x05,
    0x09,  //     Usage Page (Button)
    0x19,
    0x01,  //     Usage Minimum (0x01)
    0x29,
    0x03,  //     Usage Maximum (0x03)
    0x15,
    0x01,  //     Logical Minimum (1)
    0x25,
    0x03,  //     Logical Maximum (3)
    0x75,
    0x02,  //     Report Size (2)
    0x81,
    0x00,  //     Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null
           //     Position)
    0xC0,  //   End Collection
    0x81,
    0x03,  //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null
           //   Position)
    0xC0,  // End Collection
};

static esp_hid_raw_report_map_t ble_report_maps[] = {
    {.data = keyboardReportMap, .len = sizeof(keyboardReportMap)},
    {.data = mediaReportMap, .len = sizeof(mediaReportMap)}};

static esp_hid_device_config_t ble_hid_config = {
    .vendor_id = 0x16C0,
    .product_id = 0x05DF,
    .version = 0x0100,
    .device_name = "MY KEYBOARD",
    .manufacturer_name = "Espressif",
    .serial_number = "1234567890",
    .report_maps = ble_report_maps,
    .report_maps_len = 2};

#define HID_CC_RPT_MUTE 1
#define HID_CC_RPT_POWER 2
#define HID_CC_RPT_LAST 3
#define HID_CC_RPT_ASSIGN_SEL 4
#define HID_CC_RPT_PLAY 5
#define HID_CC_RPT_PAUSE 6
#define HID_CC_RPT_RECORD 7
#define HID_CC_RPT_FAST_FWD 8
#define HID_CC_RPT_REWIND 9
#define HID_CC_RPT_SCAN_NEXT_TRK 10
#define HID_CC_RPT_SCAN_PREV_TRK 11
#define HID_CC_RPT_STOP 12

#define HID_CC_RPT_CHANNEL_UP 0x10
#define HID_CC_RPT_CHANNEL_DOWN 0x30
#define HID_CC_RPT_VOLUME_UP 0x40
#define HID_CC_RPT_VOLUME_DOWN 0x80

// HID Consumer Control report bitmasks
#define HID_CC_RPT_NUMERIC_BITS 0xF0
#define HID_CC_RPT_CHANNEL_BITS 0xCF
#define HID_CC_RPT_VOLUME_BITS 0x3F
#define HID_CC_RPT_BUTTON_BITS 0xF0
#define HID_CC_RPT_SELECTION_BITS 0xCF

// Macros for the HID Consumer Control 2-byte report
#define HID_CC_RPT_SET_NUMERIC(s, x) \
  (s)[0] &= HID_CC_RPT_NUMERIC_BITS; \
  (s)[0] = (x)
#define HID_CC_RPT_SET_CHANNEL(s, x) \
  (s)[0] &= HID_CC_RPT_CHANNEL_BITS; \
  (s)[0] |= ((x) & 0x03) << 4
#define HID_CC_RPT_SET_VOLUME_UP(s) \
  (s)[0] &= HID_CC_RPT_VOLUME_BITS; \
  (s)[0] |= 0x40
#define HID_CC_RPT_SET_VOLUME_DOWN(s) \
  (s)[0] &= HID_CC_RPT_VOLUME_BITS;   \
  (s)[0] |= 0x80
#define HID_CC_RPT_SET_BUTTON(s, x) \
  (s)[1] &= HID_CC_RPT_BUTTON_BITS; \
  (s)[1] |= (x)
#define HID_CC_RPT_SET_SELECTION(s, x) \
  (s)[1] &= HID_CC_RPT_SELECTION_BITS; \
  (s)[1] |= ((x) & 0x03) << 4

// HID Consumer Usage IDs (subset of the codes available in the USB HID Usage
// Tables spec)
#define HID_CONSUMER_POWER 48  // Power
#define HID_CONSUMER_RESET 49  // Reset
#define HID_CONSUMER_SLEEP 50  // Sleep

#define HID_CONSUMER_MENU 64           // Menu
#define HID_CONSUMER_SELECTION 128     // Selection
#define HID_CONSUMER_ASSIGN_SEL 129    // Assign Selection
#define HID_CONSUMER_MODE_STEP 130     // Mode Step
#define HID_CONSUMER_RECALL_LAST 131   // Recall Last
#define HID_CONSUMER_QUIT 148          // Quit
#define HID_CONSUMER_HELP 149          // Help
#define HID_CONSUMER_CHANNEL_UP 156    // Channel Increment
#define HID_CONSUMER_CHANNEL_DOWN 157  // Channel Decrement

#define HID_CONSUMER_PLAY 176           // Play
#define HID_CONSUMER_PAUSE 177          // Pause
#define HID_CONSUMER_RECORD 178         // Record
#define HID_CONSUMER_FAST_FORWARD 179   // Fast Forward
#define HID_CONSUMER_REWIND 180         // Rewind
#define HID_CONSUMER_SCAN_NEXT_TRK 181  // Scan Next Track
#define HID_CONSUMER_SCAN_PREV_TRK 182  // Scan Previous Track
#define HID_CONSUMER_STOP 183           // Stop
#define HID_CONSUMER_EJECT 184          // Eject
#define HID_CONSUMER_RANDOM_PLAY 185    // Random Play
#define HID_CONSUMER_SELECT_DISC 186    // Select Disk
#define HID_CONSUMER_ENTER_DISC 187     // Enter Disc
#define HID_CONSUMER_REPEAT 188         // Repeat
#define HID_CONSUMER_STOP_EJECT 204     // Stop/Eject
#define HID_CONSUMER_PLAY_PAUSE 205     // Play/Pause
#define HID_CONSUMER_PLAY_SKIP 206      // Play/Skip

#define HID_CONSUMER_VOLUME 224       // Volume
#define HID_CONSUMER_BALANCE 225      // Balance
#define HID_CONSUMER_MUTE 226         // Mute
#define HID_CONSUMER_BASS 227         // Bass
#define HID_CONSUMER_VOLUME_UP 233    // Volume Increment
#define HID_CONSUMER_VOLUME_DOWN 234  // Volume Decrement

#define HID_RPT_ID_CC_IN 3   // Consumer Control input report ID
#define HID_CC_IN_RPT_LEN 2  // Consumer Control input report Len
#define HID_RPT_ID_KEY_IN 1
#define HID_KEY_IN_RPT_LEN 8
void esp_hidd_send_consumer_value(uint8_t key_cmd, bool key_pressed);
void esp_hidd_send_key_value(uint8_t keycode, bool key_pressed);
void esp_hidd_send_modifier_key_value(uint8_t modifier, uint8_t keycode, bool key_pressed);
void ble_hid_demo_task(void *pvParameters);

void ble_hid_demo_task(void *pvParameters) {
  int reconnect_counter = 0;
  
  while (1) {
    if (s_ble_is_connected) {
      ESP_LOGI(TAG, "设备已连接，发送按键");
      
      // 测试普通按键
      ESP_LOGI(TAG, "测试普通按键 A (0x04)");
      esp_hidd_send_key_value(0x04, true);
      vTaskDelay(200 / portTICK_PERIOD_MS);
      esp_hidd_send_key_value(0x04, false);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      
      // 测试空格键
      ESP_LOGI(TAG, "测试空格键 SPACE (0x2C)");
      esp_hidd_send_key_value(0x2C, true);
      vTaskDelay(200 / portTICK_PERIOD_MS);
      esp_hidd_send_key_value(0x2C, false);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      
      // 测试组合键 Ctrl+A
      ESP_LOGI(TAG, "测试组合键 Ctrl+A");
      esp_hidd_send_modifier_key_value(0x01, 0x04, true);  // 左Ctrl(0x01) + A(0x04)
      vTaskDelay(200 / portTICK_PERIOD_MS);
      esp_hidd_send_modifier_key_value(0x01, 0x04, false);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      
      // 测试组合键 Shift+A
      ESP_LOGI(TAG, "测试组合键 Shift+A");
      esp_hidd_send_modifier_key_value(0x02, 0x04, true);  // 左Shift(0x02) + A(0x04)
      vTaskDelay(200 / portTICK_PERIOD_MS);
      esp_hidd_send_modifier_key_value(0x02, 0x04, false);
      
      // 如果多次尝试后仍然没有效果，尝试重新连接
      reconnect_counter++;
      if (reconnect_counter >= 3) {
        ESP_LOGI(TAG, "多次尝试后仍无效果，尝试重新连接...");
        // 断开连接并重新初始化
        s_ble_is_connected = false;
        // 重新开始广播
        esp_hid_ble_gap_adv_start();
        reconnect_counter = 0;
      }
    } else {
      ESP_LOGI(TAG, "设备未连接，等待连接...");
    }
    
    // 增加延时，方便观察
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
}

void ble_hid_task_start_up(void) {
  xTaskCreate(ble_hid_demo_task, "ble_hid_demo_task", 2 * 1024, NULL,
              configMAX_PRIORITIES - 3, &s_ble_hid_param.task_hdl);
}

void ble_hid_task_shut_down(void) {
  if (s_ble_hid_param.task_hdl) {
    vTaskDelete(s_ble_hid_param.task_hdl);
    s_ble_hid_param.task_hdl = NULL;
  }
}

static void ble_hidd_event_callback(void *handler_args, esp_event_base_t base,
                                    int32_t id, void *event_data) {
  esp_hidd_event_t event = (esp_hidd_event_t)id;
  esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;
  static const char *TAG = "HID_DEV_BLE";

  switch (event) {
    case ESP_HIDD_START_EVENT: {
      ESP_LOGI(TAG, "START");
      esp_hid_ble_gap_adv_start();
      break;
    }
    case ESP_HIDD_CONNECT_EVENT: {
      ESP_LOGI(TAG, "CONNECT");
      s_ble_is_connected = true;
      
      // 打印连接信息
      ESP_LOGI(TAG, "连接成功，准备发送HID报告");
      
      // 启动HID任务
      ble_hid_task_start_up();
      break;
    }
    case ESP_HIDD_PROTOCOL_MODE_EVENT: {
      ESP_LOGI(TAG, "PROTOCOL MODE[%u]: %s", param->protocol_mode.map_index,
               param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
      
      // 确保设备处于Report模式
      if (param->protocol_mode.protocol_mode == 0) {  // 如果是Boot模式
        ESP_LOGI(TAG, "当前为Boot模式，建议切换到Report模式");
        // 不使用未声明的函数，只记录日志
      }
      break;
    }
    case ESP_HIDD_CONTROL_EVENT: {
      ESP_LOGI(TAG, "CONTROL[%u]: %sSUSPEND", param->control.map_index,
               param->control.control ? "EXIT_" : "");
      break;
    }
    case ESP_HIDD_OUTPUT_EVENT: {
      ESP_LOGI(TAG, "OUTPUT[%u]: %8s ID: %2u, Len: %d, Data:",
               param->output.map_index, esp_hid_usage_str(param->output.usage),
               param->output.report_id, param->output.length);
      ESP_LOG_BUFFER_HEX(TAG, param->output.data, param->output.length);
      break;
    }
    case ESP_HIDD_FEATURE_EVENT: {
      ESP_LOGI(TAG, "FEATURE[%u]: %8s ID: %2u, Len: %d, Data:",
               param->feature.map_index,
               esp_hid_usage_str(param->feature.usage),
               param->feature.report_id, param->feature.length);
      ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
      break;
    }
    case ESP_HIDD_DISCONNECT_EVENT: {
      ESP_LOGI(TAG, "DISCONNECT: %s",
               esp_hid_disconnect_reason_str(
                   esp_hidd_dev_transport_get(param->disconnect.dev),
                   param->disconnect.reason));
      ble_hid_task_shut_down();
      esp_hid_ble_gap_adv_start();
      s_ble_is_connected = false;
      break;
    }
    case ESP_HIDD_STOP_EVENT: {
      ESP_LOGI(TAG, "STOP");
      break;
    }
    default:
      break;
  }
  return;
}
#endif

#if CONFIG_BT_HID_DEVICE_ENABLED
static local_param_t s_bt_hid_param = {0};
const unsigned char mouseReportMap[] = {
    0x05, 0x01,  // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,  // USAGE (Mouse)
    0xa1, 0x01,  // COLLECTION (Application)

    0x09, 0x01,  //   USAGE (Pointer)
    0xa1, 0x00,  //   COLLECTION (Physical)

    0x05, 0x09,  //     USAGE_PAGE (Button)
    0x19, 0x01,  //     USAGE_MINIMUM (Button 1)
    0x29, 0x03,  //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00,  //     LOGICAL_MINIMUM (0)
    0x25, 0x01,  //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,  //     REPORT_COUNT (3)
    0x75, 0x01,  //     REPORT_SIZE (1)
    0x81, 0x02,  //     INPUT (Data,Var,Abs)
    0x95, 0x01,  //     REPORT_COUNT (1)
    0x75, 0x05,  //     REPORT_SIZE (5)
    0x81, 0x03,  //     INPUT (Cnst,Var,Abs)

    0x05, 0x01,  //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,  //     USAGE (X)
    0x09, 0x31,  //     USAGE (Y)
    0x09, 0x38,  //     USAGE (Wheel)
    0x15, 0x81,  //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f,  //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,  //     REPORT_SIZE (8)
    0x95, 0x03,  //     REPORT_COUNT (3)
    0x81, 0x06,  //     INPUT (Data,Var,Rel)

    0xc0,  //   END_COLLECTION
    0xc0   // END_COLLECTION
};

static esp_hid_raw_report_map_t bt_report_maps[] = {
    {.data = mouseReportMap, .len = sizeof(mouseReportMap)},
};

static esp_hid_device_config_t bt_hid_config = {
    .vendor_id = 0x16C0,
    .product_id = 0x05DF,
    .version = 0x0100,
    .device_name = "Test ESP Keyboard",
    .manufacturer_name = "Espressif",
    .serial_number = "1234567890",
    .report_maps = bt_report_maps,
    .report_maps_len = 1};

// send the buttons, change in x, and change in y
void send_mouse(uint8_t buttons, char dx, char dy, char wheel) {
  static uint8_t buffer[4] = {0};
  buffer[0] = buttons;
  buffer[1] = dx;
  buffer[2] = dy;
  buffer[3] = wheel;
  esp_hidd_dev_input_set(s_bt_hid_param.hid_dev, 0, 0, buffer, 4);
}

void bt_hid_demo_task(void *pvParameters) {
  static const char *help_string =
      "########################################################################"
      "\n"
      "BT hid mouse demo usage:\n"
      "You can input these value to simulate mouse: 'q', 'w', 'e', 'a', 's', "
      "'d', 'h'\n"
      "q -- click the left key\n"
      "w -- move up\n"
      "e -- click the right key\n"
      "a -- move left\n"
      "s -- move down\n"
      "d -- move right\n"
      "h -- show the help\n"
      "########################################################################"
      "\n";
  printf("%s\n", help_string);
  char c;
  while (1) {
    c = fgetc(stdin);
    switch (c) {
      case 'q':
        send_mouse(1, 0, 0, 0);
        break;
      case 'w':
        send_mouse(0, 0, -10, 0);
        break;
      case 'e':
        send_mouse(2, 0, 0, 0);
        break;
      case 'a':
        send_mouse(0, -10, 0, 0);
        break;
      case 's':
        send_mouse(0, 0, 10, 0);
        break;
      case 'd':
        send_mouse(0, 10, 0, 0);
        break;
      case 'h':
        printf("%s\n", help_string);
        break;
      default:
        break;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void bt_hid_task_start_up(void) {
  xTaskCreate(bt_hid_demo_task, "bt_hid_demo_task", 2 * 1024, NULL,
              configMAX_PRIORITIES - 3, &s_bt_hid_param.task_hdl);
  return;
}

void bt_hid_task_shut_down(void) {
  if (s_bt_hid_param.task_hdl) {
    vTaskDelete(s_bt_hid_param.task_hdl);
    s_bt_hid_param.task_hdl = NULL;
  }
}

static void bt_hidd_event_callback(void *handler_args, esp_event_base_t base,
                                   int32_t id, void *event_data) {
  esp_hidd_event_t event = (esp_hidd_event_t)id;
  esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;
  static const char *TAG = "HID_DEV_BT";

  switch (event) {
    case ESP_HIDD_START_EVENT: {
      if (param->start.status == ESP_OK) {
        ESP_LOGI(TAG, "START OK");
        ESP_LOGI(TAG, "Setting to connectable, discoverable");
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE,
                                 ESP_BT_GENERAL_DISCOVERABLE);
      } else {
        ESP_LOGE(TAG, "START failed!");
      }
      break;
    }
    case ESP_HIDD_CONNECT_EVENT: {
      ESP_LOGI(TAG, "CONNECT OK");
      ESP_LOGI(TAG, "Setting to non-connectable, non-discoverable");
      esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE,
                               ESP_BT_NON_DISCOVERABLE);
      bt_hid_task_start_up();
      break;
    }
    case ESP_HIDD_PROTOCOL_MODE_EVENT: {
      ESP_LOGI(TAG, "PROTOCOL MODE[%u]: %s", param->protocol_mode.map_index,
               param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
      break;
    }
    case ESP_HIDD_OUTPUT_EVENT: {
      ESP_LOGI(TAG, "OUTPUT[%u]: %8s ID: %2u, Len: %d, Data:",
               param->output.map_index, esp_hid_usage_str(param->output.usage),
               param->output.report_id, param->output.length);
      ESP_LOG_BUFFER_HEX(TAG, param->output.data, param->output.length);
      break;
    }
    case ESP_HIDD_FEATURE_EVENT: {
      ESP_LOGI(TAG, "FEATURE[%u]: %8s ID: %2u, Len: %d, Data:",
               param->feature.map_index,
               esp_hid_usage_str(param->feature.usage),
               param->feature.report_id, param->feature.length);
      ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
      break;
    }
    case ESP_HIDD_DISCONNECT_EVENT: {
      if (param->disconnect.status == ESP_OK) {
        ESP_LOGI(TAG, "DISCONNECT OK");
        bt_hid_task_shut_down();
        ESP_LOGI(TAG, "Setting to connectable, discoverable again");
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE,
                                 ESP_BT_GENERAL_DISCOVERABLE);
      } else {
        ESP_LOGE(TAG, "DISCONNECT failed!");
      }
      break;
    }
    case ESP_HIDD_STOP_EVENT: {
      ESP_LOGI(TAG, "STOP");
      break;
    }
    default:
      break;
  }
  return;
}
#endif

void app_main(void) {
  esp_err_t ret;
  
  ESP_LOGI(TAG, "启动蓝牙HID键盘示例...");

#if CONFIG_BT_BLE_ENABLED || CONFIG_BT_HID_DEVICE_ENABLED
  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(TAG, "设置HID GAP模式: %d", HID_DEV_MODE);
  ret = esp_hid_gap_init(HID_DEV_MODE);
  ESP_ERROR_CHECK(ret);

#if CONFIG_BT_BLE_ENABLED
  ESP_LOGI(TAG, "初始化BLE广播...");
  ret = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_GENERIC,
                                 ble_hid_config.device_name);
  ESP_ERROR_CHECK(ret);

  if ((ret = esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler)) !=
      ESP_OK) {
    ESP_LOGE(TAG, "GATTS注册回调失败: %d", ret);
    return;
  }
  ESP_LOGI(TAG, "设置BLE设备...");
  ESP_ERROR_CHECK(esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE,
                                    ble_hidd_event_callback,
                                    &s_ble_hid_param.hid_dev));
  ESP_LOGI(TAG, "BLE HID设备初始化完成，等待连接...");
#endif
#if CONFIG_BT_HID_DEVICE_ENABLED
  ESP_LOGI(TAG, "setting device name");
  esp_bt_dev_set_device_name(bt_hid_config.device_name);
  ESP_LOGI(TAG, "setting cod major, peripheral");
  esp_bt_cod_t cod;
  cod.major = ESP_BT_COD_MAJOR_DEV_PERIPHERAL;
  esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_MAJOR_MINOR);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  ESP_LOGI(TAG, "setting bt device");
  ESP_ERROR_CHECK(esp_hidd_dev_init(&bt_hid_config, ESP_HID_TRANSPORT_BT,
                                    bt_hidd_event_callback,
                                    &s_bt_hid_param.hid_dev));
#endif
#endif  // CONFIG_BT_BLE_ENABLED || CONFIG_BT_HID_DEVICE_ENABLED
}

void esp_hidd_send_consumer_value(uint8_t key_cmd, bool key_pressed) {
  uint8_t buffer[HID_CC_IN_RPT_LEN] = {0, 0};
  if (key_pressed) {
    switch (key_cmd) {
      case HID_CONSUMER_CHANNEL_UP:
        HID_CC_RPT_SET_CHANNEL(buffer, HID_CC_RPT_CHANNEL_UP);
        break;

      case HID_CONSUMER_CHANNEL_DOWN:
        HID_CC_RPT_SET_CHANNEL(buffer, HID_CC_RPT_CHANNEL_DOWN);
        break;

      case HID_CONSUMER_VOLUME_UP:
        HID_CC_RPT_SET_VOLUME_UP(buffer);
        break;

      case HID_CONSUMER_VOLUME_DOWN:
        HID_CC_RPT_SET_VOLUME_DOWN(buffer);
        break;

      case HID_CONSUMER_MUTE:
        HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_MUTE);
        break;

      case HID_CONSUMER_POWER:
        HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_POWER);
        break;

      case HID_CONSUMER_RECALL_LAST:
        HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_LAST);
        break;

      case HID_CONSUMER_ASSIGN_SEL:
        HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_ASSIGN_SEL);
        break;

      case HID_CONSUMER_PLAY:
        HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_PLAY);
        break;

      case HID_CONSUMER_PAUSE:
        HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_PAUSE);
        break;

      case HID_CONSUMER_RECORD:
        HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_RECORD);
        break;

      case HID_CONSUMER_FAST_FORWARD:
        HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_FAST_FWD);
        break;

      case HID_CONSUMER_REWIND:
        HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_REWIND);
        break;

      case HID_CONSUMER_SCAN_NEXT_TRK:
        HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_SCAN_NEXT_TRK);
        break;

      case HID_CONSUMER_SCAN_PREV_TRK:
        HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_SCAN_PREV_TRK);
        break;

      case HID_CONSUMER_STOP:
        HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_STOP);
        break;

      default:
        break;
    }
  }
  esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 1, HID_RPT_ID_CC_IN, buffer,
                         HID_CC_IN_RPT_LEN);
  return;
}

void esp_hidd_send_key_value(uint8_t keycode, bool key_pressed) {
  uint8_t buf[HID_KEY_IN_RPT_LEN];
  
  ESP_LOGI(TAG, "Sending key: 0x%02x, pressed: %d", keycode, key_pressed);
  
  // 清空缓冲区
  memset(buf, 0, HID_KEY_IN_RPT_LEN);
  
  if (key_pressed) {
    // 按键按下时，设置keycode
    // 标准HID键盘报告格式：
    // buf[0] = 修饰键
    // buf[1] = 保留字节
    // buf[2] = 按键1
    // buf[3] = 按键2
    // ...
    buf[2] = keycode;
  }
  
  // 发送报告
  esp_err_t err = esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 1, HID_RPT_ID_KEY_IN, buf, HID_KEY_IN_RPT_LEN);
  ESP_LOGI(TAG, "Send key result: %s", esp_err_to_name(err));
  
  // 添加调试信息
  ESP_LOGI(TAG, "键盘报告内容:");
  ESP_LOG_BUFFER_HEX(TAG, buf, HID_KEY_IN_RPT_LEN);
}

// 添加发送带修饰键的组合键的函数
void esp_hidd_send_modifier_key_value(uint8_t modifier, uint8_t keycode, bool key_pressed) {
  uint8_t buf[HID_KEY_IN_RPT_LEN];
  
  ESP_LOGI(TAG, "Sending modifier: 0x%02x, key: 0x%02x, pressed: %d", modifier, keycode, key_pressed);
  
  // 清空缓冲区
  memset(buf, 0, HID_KEY_IN_RPT_LEN);
  
  if (key_pressed) {
    buf[0] = modifier;  // 修饰键
    buf[2] = keycode;   // 普通键
  }
  
  // 发送报告
  esp_err_t err = esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 1, HID_RPT_ID_KEY_IN, buf, HID_KEY_IN_RPT_LEN);
  ESP_LOGI(TAG, "Send key result: %s", esp_err_to_name(err));
  
  // 添加调试信息
  ESP_LOGI(TAG, "组合键报告内容:");
  ESP_LOG_BUFFER_HEX(TAG, buf, HID_KEY_IN_RPT_LEN);
}
