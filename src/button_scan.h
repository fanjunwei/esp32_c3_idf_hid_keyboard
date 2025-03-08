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

// 按键矩阵引脚定义
#define ROW1_PIN GPIO_NUM_6
#define ROW2_PIN GPIO_NUM_8
#define ROW3_PIN GPIO_NUM_2

#define COL1_PIN GPIO_NUM_12
#define COL2_PIN GPIO_NUM_13
#define COL3_PIN GPIO_NUM_10

// 按键状态结构体
typedef struct {
    bool pressed;
    uint8_t row;
    uint8_t col;
} button_state_t;

// 初始化按键扫描
void button_scan_init(void);

// 扫描按键
button_state_t scan_button(void);

// 获取按键对应的键码
uint8_t get_keycode_from_button(uint8_t row, uint8_t col);

#endif /* BUTTON_SCAN_H */ 