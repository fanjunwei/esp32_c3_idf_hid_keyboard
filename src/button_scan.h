#ifndef BUTTON_SCAN_H
#define BUTTON_SCAN_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 按键矩阵定义
#define ROW_NUM 3
#define COL_NUM 3
#define MAX_KEYS 6  // 最多支持6个按键同时按下

// 按键矩阵引脚定义
#define ROW1_PIN GPIO_NUM_6
#define ROW2_PIN GPIO_NUM_8
#define ROW3_PIN GPIO_NUM_2

#define COL1_PIN GPIO_NUM_12
#define COL2_PIN GPIO_NUM_13
#define COL3_PIN GPIO_NUM_10
#define DEBOUNCE_THRESHOLD 3

// 按键位置结构体
typedef struct {
    uint8_t row;
    uint8_t col;
} key_position_t;

// 按键状态结构体
typedef struct {
    uint8_t num_keys;                    // 当前按下的按键数量
    key_position_t keys[MAX_KEYS];       // 按下的按键位置数组
} button_state_t;

// 初始化按键扫描
void button_scan_init(void);

// 扫描按键
button_state_t scan_button(void);

// 获取按键对应的键码
uint8_t get_keycode_from_button(uint8_t row, uint8_t col);

// 按键状态结构体
typedef struct {
    uint8_t current : 1;
    uint8_t previous : 1;
    uint8_t count : 6;
} key_state;

#endif /* BUTTON_SCAN_H */ 