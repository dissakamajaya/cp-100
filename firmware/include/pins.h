#pragma once
// Four-pad / Wi-Fi service revision, 31 Oct 2026.
// Current wiring: firmware/README.md. The Rev08 CSV/PDF still describes eight pads.
//
// Avoided on N16R8: GPIO0/3/45/46 (strapping), GPIO19/20 (USB),
// GPIO35-37 (octal PSRAM), GPIO43/44 (UART0 console).

#include <stdint.h>

// Visitor pads, Omron B3F to GND, 10k pull-up to 3V3, pressed = LOW.
constexpr uint8_t PIN_PADS[4] = {1, 2, 4, 5};

// Legacy service GPIO, unused except optional physical MUTE on GPIO16.
constexpr uint8_t PIN_FREQ_A = 10;
constexpr uint8_t PIN_FREQ_B = 11;
constexpr uint8_t PIN_FREQ_PUSH = 12;
constexpr uint8_t PIN_LEVEL_A = 13;
constexpr uint8_t PIN_LEVEL_B = 14;
constexpr uint8_t PIN_SAVE = 15;
constexpr uint8_t PIN_MUTE = 16;

// Historical MCP23017 mapping; the current app does not initialize this panel.
constexpr uint8_t PIN_I2C_SCL = 17;
constexpr uint8_t PIN_I2C_SDA = 42;
constexpr uint8_t MCP23017_ADDR = 0x20;

// GY-PCM5102: SCK/FMT/DEMP/FLT to GND, XSMT to 3V3.
constexpr uint8_t PIN_I2S_BCK = 18;
constexpr uint8_t PIN_I2S_LRCK = 21;
constexpr uint8_t PIN_I2S_DOUT = 47;

// MAX7219 quad 32x8 via SN74AHCT125N (3V3 -> 5V).
constexpr uint8_t PIN_MATRIX_DIN = 39;  // AHCT 1A
constexpr uint8_t PIN_MATRIX_CLK = 40;  // AHCT 2A
constexpr uint8_t PIN_MATRIX_CS = 41;   // AHCT 3A, 10k pull-up

// WS2812B chain (4 pads) via AHCT 4A. DevKitC-1 v1.1 onboard RGB LED is also on GPIO38.
constexpr uint8_t PIN_RGB = 38;
