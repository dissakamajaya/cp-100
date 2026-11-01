#pragma once
// Host stand-in for the Arduino-ESP32 2.0.17 core, just the parts the CP-100
// firmware uses. Everything is backed by the simulated board in ../sim.cpp.
// Signatures follow cores/esp32 (esp32-hal.h, HardwareSerial.h, freertos).

#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <cmath>
#include <mutex>

#define LOW 0x0
#define HIGH 0x1
#define INPUT 0x01
#define OUTPUT 0x03
#define PULLUP 0x04
#define INPUT_PULLUP 0x05
#define PULLDOWN 0x08
#define INPUT_PULLDOWN 0x09
#define LSBFIRST 0
#define MSBFIRST 1

// Arduino.h line 76 in the 2.0.17 core: a macro, so arguments are evaluated twice.
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

using std::abs;
using std::isinf;
using std::isnan;
using std::max;
using std::min;
using std::round;

// On the ESP32 "unsigned long" is 32 bits; uint32_t keeps the same wrap-around on a 64-bit host.
uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void pinMode(uint8_t pin, uint8_t mode);
uint32_t getCpuFrequencyMhz();
void enableLoopWDT();  // esp32-hal-misc.c: adds loopTask to the 5 s task watchdog
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);

class HardwareSerial {
 public:
  void begin(unsigned long baud, uint32_t config = 0, int8_t rxPin = -1, int8_t txPin = -1, bool invert = false,
             unsigned long timeout_ms = 20000UL, uint8_t rxfifo_full_thrhd = 112);
  int available();
  int read();
  size_t write(uint8_t c);
  size_t write(const uint8_t* buffer, size_t size);
  size_t print(const char* s);
  size_t print(char c);
  size_t print(int n);
  size_t print(unsigned n);
  size_t print(long n);
  size_t print(unsigned long n);
  size_t print(double n, int digits = 2);
  size_t println();
  template <typename T>
  size_t println(T v) {
    const size_t n = print(v);
    return n + println();
  }
  size_t printf(const char* format, ...) __attribute__((format(printf, 2, 3)));
  // 2.0.17: only before begin(), and only sizes above the 128-byte FIFO; returns 0 otherwise.
  size_t setTxBufferSize(size_t new_size);
};
extern HardwareSerial Serial;

// Esp.h: the cycle counter runs at the CPU clock (240 MHz). Here it follows the
// host thread's CPU time, so render cost reflects this machine, not the chip.
class EspClass {
 public:
  uint32_t getCycleCount();
};
extern EspClass ESP;

// ---- FreeRTOS / ESP-IDF bits pulled in by Arduino.h on the real core ----
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef uint32_t TickType_t;
typedef void* TaskHandle_t;
typedef void (*TaskFunction_t)(void*);
#define portMAX_DELAY ((TickType_t)0xffffffffUL)
#define portTICK_PERIOD_MS 1  // CONFIG_FREERTOS_HZ=1000 in the Arduino core
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define configMAX_PRIORITIES 25
#define pdPASS 1
#define tskNO_AFFINITY 0x7FFFFFFF

// A spinlock on the chip; a mutex here (the audio thread and loop() share it).
struct portMUX_TYPE {
  std::mutex m;
};
#define portMUX_INITIALIZER_UNLOCKED \
  {}
#define portENTER_CRITICAL(mux) ((mux)->m.lock())
#define portEXIT_CRITICAL(mux) ((mux)->m.unlock())

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t code, const char* name, const uint32_t stackDepth, void* params,
                                   UBaseType_t priority, TaskHandle_t* created, const BaseType_t core);

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_INTR_FLAG_LEVEL1 (1 << 1)
void sim_esp_error_check_failed(esp_err_t rc, const char* file, int line, const char* expr);
#define ESP_ERROR_CHECK(x)                                                     \
  do {                                                                         \
    esp_err_t err_rc_ = (x);                                                   \
    if (err_rc_ != ESP_OK) sim_esp_error_check_failed(err_rc_, __FILE__, __LINE__, #x); \
  } while (0)

#include "esp32-hal-rmt.h"
