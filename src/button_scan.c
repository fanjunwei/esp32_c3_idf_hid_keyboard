#include "button_scan.h"
#include "esp_log.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BUTTON_SCAN";

// 行引脚数组
static const gpio_num_t row_pins[ROW_NUM] = {
    ROW1_PIN, ROW2_PIN, ROW3_PIN
};

// 列引脚数组
static const gpio_num_t col_pins[COL_NUM] = {
    COL1_PIN, COL2_PIN, COL3_PIN
};

// 按键映射到键码 (HID键码)
// 按键矩阵布局:
// | 1(0,0) | 2(0,1) | 3(0,2) |
// | 4(1,0) | 5(1,1) | 6(1,2) |
// | 7(2,0) | 8(2,1) | 9(2,2) |
static const uint8_t keycode_map[ROW_NUM][COL_NUM] = {
    {0x04, 0x05, 0x06}, // A, B, C
    {0x07, 0x08, 0x09}, // D, E, F
    {0x0A, 0x0B, 0x0C}  // G, H, I
};

void button_scan_init(void) {
    ESP_LOGI(TAG, "初始化按键扫描");
    
    // 配置行引脚为输出
    uint64_t pin_bit_mask = 0;
    for (int i = 0; i < ROW_NUM; i++) {
        pin_bit_mask |= (1ULL << row_pins[i]);
    }
    gpio_config_t io_conf = {
        .pin_bit_mask = pin_bit_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    for (int i = 0; i < ROW_NUM; i++) {
        gpio_set_level(row_pins[i], 1); // 默认设置为高电平
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    pin_bit_mask = 0;
    // 配置列引脚为输入，启用上拉电阻
    for (int i = 0; i < COL_NUM; i++) {
        pin_bit_mask |= (1ULL << col_pins[i]);
    }
    gpio_config_t io_conf2 = {
        .pin_bit_mask = pin_bit_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf2);
}

// 检查特定行列的按键是否被按下
static bool is_button_pressed(uint8_t row, uint8_t col) {
    // 将当前行设置为低电平
    for (int i = 0; i < ROW_NUM; i++) {
        gpio_set_level(row_pins[i], 1); // 所有行设为高电平
    }
    gpio_set_level(row_pins[row], 0); // 当前行设为低电平
    
    // 读取列状态
    vTaskDelay(1 / portTICK_PERIOD_MS); // 短暂延时，等待电平稳定
    int col_state = gpio_get_level(col_pins[col]);
    
    // 如果列为低电平，表示按键被按下
    return (col_state == 0);
}

button_state_t scan_button(void) {
    button_state_t result = {false, 0, 0};
    
    // 扫描所有按键
    for (int row = 0; row < ROW_NUM; row++) {
        for (int col = 0; col < COL_NUM; col++) {
            if (is_button_pressed(row, col)) {
                result.pressed = true;
                result.row = row;
                result.col = col;
                ESP_LOGI(TAG, "按键按下: 行=%d, 列=%d", row, col);
                return result; // 找到第一个按下的按键就返回
            }
        }
    }
    
    // 恢复所有行为高电平
    for (int i = 0; i < ROW_NUM; i++) {
        gpio_set_level(row_pins[i], 1);
    }
    
    return result;
}

uint8_t get_keycode_from_button(uint8_t row, uint8_t col) {
    if (row < ROW_NUM && col < COL_NUM) {
        return keycode_map[row][col];
    }
    return 0; // 无效的行列返回0
} 