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
// | A(0,0) | B(0,1) | C(0,2) |
// | D(1,0) | E(1,1) | F(1,2) |
// | G(2,0) | H(2,1) | I(2,2) |
static const uint8_t keycode_map[ROW_NUM][COL_NUM] = {
    {0x52, 0x4F, 0x06}, // UP, RIGHT, C
    {0x50, 0x08, 0x09}, // LEFT, E, F
    {0x51, 0x0B, 0x0C}  // DOWN, H, I
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

static bool is_button_pressed(uint8_t row, uint8_t col) {
    static uint8_t last_row = 0xFF;  // 记录上次扫描的行
    
    // 只有当切换到不同的行时才改变GPIO状态
    if (row != last_row) {
        // 将所有行设为高电平
        for (int i = 0; i < ROW_NUM; i++) {
            gpio_set_level(row_pins[i], 1);
        }
        // 将当前行设为低电平
        gpio_set_level(row_pins[row], 0);
        last_row = row;
        
        // 给一个短暂的延时让电平稳定
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    
    // 读取列状态
    int col_state = gpio_get_level(col_pins[col]);
    return (col_state == 0);
}

button_state_t scan_button(void) {
    button_state_t result = {0};  // 初始化为0个按键
    static TickType_t last_scan_time = 0;
    TickType_t current_time = xTaskGetTickCount();
    
    // 限制扫描频率，每20ms扫描一次
    if ((current_time - last_scan_time) < pdMS_TO_TICKS(20)) {
        return result;
    }
    last_scan_time = current_time;
    
    // 扫描所有按键
    for (int row = 0; row < ROW_NUM; row++) {
        for (int col = 0; col < COL_NUM; col++) {
            if (is_button_pressed(row, col)) {
                // 如果还没有达到最大按键数，添加这个按键
                if (result.num_keys < MAX_KEYS) {
                    result.keys[result.num_keys].row = row;
                    result.keys[result.num_keys].col = col;
                    result.num_keys++;
                    ESP_LOGI(TAG, "按键按下: 行=%d, 列=%d", row, col);
                }
            }
        }
    }
    
    return result;
}

uint8_t get_keycode_from_button(uint8_t row, uint8_t col) {
    if (row < ROW_NUM && col < COL_NUM) {
        return keycode_map[row][col];
    }
    return 0; // 无效的行列返回0
} 