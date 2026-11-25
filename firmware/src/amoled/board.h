#pragma once
// Waveshare ESP32-S3-Touch-AMOLED-2.16 (480x480 CO5300 QSPI AMOLED,
// CST9217 touch, ES8311 codec + speaker amplifier). Pins from Waveshare's BSP.
#include <stdint.h>

constexpr int kScreenW = 480, kScreenH = 480;

constexpr int8_t PIN_LCD_CS = 12, PIN_LCD_SCLK = 38, PIN_LCD_RST = 39;
constexpr int8_t PIN_LCD_D0 = 4, PIN_LCD_D1 = 5, PIN_LCD_D2 = 6, PIN_LCD_D3 = 7;

constexpr int8_t PIN_I2C_SDA = 15, PIN_I2C_SCL = 14;
constexpr int8_t PIN_TOUCH_RST = 40, PIN_TOUCH_INT = 11;
constexpr uint8_t TOUCH_ADDR = 0x5A;

constexpr int8_t PIN_I2S_MCLK = 42, PIN_I2S_BCK = 9, PIN_I2S_LRCK = 45, PIN_I2S_DOUT = 8;
constexpr int8_t PIN_SPEAKER_AMP = 46;
constexpr uint8_t ES8311_ADDR = 0x18;
