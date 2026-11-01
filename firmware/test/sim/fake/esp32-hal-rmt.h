#pragma once
// Same declarations as cores/esp32/esp32-hal-rmt.h (core 2.0.17).
#include <stddef.h>
#include <stdint.h>

#define RMT_TX_MODE true
#define RMT_RX_MODE false

typedef enum {
  RMT_MEM_64 = 1,
  RMT_MEM_128 = 2,
  RMT_MEM_192 = 3,
  RMT_MEM_256 = 4,
  RMT_MEM_320 = 5,
  RMT_MEM_384 = 6,
  RMT_MEM_448 = 7,
  RMT_MEM_512 = 8,
} rmt_reserve_memsize_t;

typedef struct rmt_obj_s rmt_obj_t;

typedef struct {
  union {
    struct {
      uint32_t duration0 : 15;
      uint32_t level0 : 1;
      uint32_t duration1 : 15;
      uint32_t level1 : 1;
    };
    uint32_t val;
  };
} rmt_data_t;

rmt_obj_t* rmtInit(int pin, bool tx_not_rx, rmt_reserve_memsize_t memsize);
float rmtSetTick(rmt_obj_t* rmt, float tick);
bool rmtWriteBlocking(rmt_obj_t* rmt, rmt_data_t* data, size_t size);
