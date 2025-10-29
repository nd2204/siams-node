#ifndef CONFIG_H
#define CONFIG_H

#include "sdkconfig.h"

#define I2C_BUS_PORT 0

#define LCD_PIXEL_CLOCK_HZ (400 * 1000)
#define SDA_PIN_NUM        21
#define SCL_PIN_NUM        22
#define PIN_NUM_RST        -1
#define I2C_HW_ADDR        0x3C

#define LCD_H_RES 128
#define LCD_V_RES 64

// Bit number used to represent command and parameter
#define LCD_CMD_BITS   8
#define LCD_PARAM_BITS 8

#define LVGL_TICK_PERIOD_MS    5
#define LVGL_TASK_STACK_SIZE   (4 * 1024)
#define LVGL_TASK_PRIORITY     2
#define LVGL_PALETTE_SIZE      8
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1000 / CONFIG_FREERTOS_HZ

#endif
