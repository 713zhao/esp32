#pragma once
// Waveshare ESP32-C6-Touch-LCD-1.9 hardware definitions. See skill references/board.md.
#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

// 0 = portrait 170x320 (USB at bottom/top), 1 = landscape 320x170
#define BOARD_LANDSCAPE      0

#define BOARD_LCD_NATIVE_W   170
#define BOARD_LCD_NATIVE_H   320
#define BOARD_LCD_X_GAP      35          // panel RAM is 240 wide; visible area starts at col 35
#if BOARD_LANDSCAPE
#define BOARD_LCD_W          BOARD_LCD_NATIVE_H
#define BOARD_LCD_H          BOARD_LCD_NATIVE_W
#else
#define BOARD_LCD_W          BOARD_LCD_NATIVE_W
#define BOARD_LCD_H          BOARD_LCD_NATIVE_H
#endif
#define BOARD_LCD_PCLK_HZ    (40 * 1000 * 1000)   // drop to 20 MHz if you see glitches

// LCD (SPI2)
#define BOARD_PIN_LCD_MOSI   4
#define BOARD_PIN_LCD_SCLK   5
#define BOARD_PIN_LCD_DC     6
#define BOARD_PIN_LCD_CS     7
#define BOARD_PIN_LCD_RST    14
#define BOARD_PIN_LCD_BL     15          // active LOW
// Shared I2C: CST816 touch (0x15) + QMI8658 IMU (0x6B)
#define BOARD_PIN_I2C_SDA    18
#define BOARD_PIN_I2C_SCL    8
#define BOARD_TOUCH_ADDR     0x15
// Misc
#define BOARD_PIN_BOOT_BTN   9           // active LOW
#define BOARD_PIN_RGB_LED    3           // WS2812
#define BOARD_PIN_SD_MISO    19
#define BOARD_PIN_SD_CS      20
#define BOARD_PIN_BAT_ADC    0           // ADC1_CH0

// Init LCD, touch (if present), backlight and LVGL. Returns LVGL display.
lv_display_t *board_init(void);
// 0..100 %
void board_set_backlight(uint8_t percent);
bool board_boot_button_pressed(void);
bool board_has_touch(void);
