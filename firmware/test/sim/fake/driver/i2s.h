#pragma once
// The subset of ESP-IDF 4.4 driver/i2s.h (legacy driver) that audio.cpp uses.
#include <stddef.h>
#include <stdint.h>

#include "Arduino.h"

typedef enum { I2S_NUM_0 = 0, I2S_NUM_1 = 1, I2S_NUM_MAX } i2s_port_t;
typedef enum {
  I2S_MODE_MASTER = (0x1 << 0),
  I2S_MODE_SLAVE = (0x1 << 1),
  I2S_MODE_TX = (0x1 << 2),
  I2S_MODE_RX = (0x1 << 3),
} i2s_mode_t;
typedef enum {
  I2S_BITS_PER_SAMPLE_8BIT = 8,
  I2S_BITS_PER_SAMPLE_16BIT = 16,
  I2S_BITS_PER_SAMPLE_24BIT = 24,
  I2S_BITS_PER_SAMPLE_32BIT = 32,
} i2s_bits_per_sample_t;
typedef enum {
  I2S_BITS_PER_CHAN_DEFAULT = 0,
  I2S_BITS_PER_CHAN_8BIT = 8,
  I2S_BITS_PER_CHAN_16BIT = 16,
  I2S_BITS_PER_CHAN_24BIT = 24,
  I2S_BITS_PER_CHAN_32BIT = 32,
} i2s_bits_per_chan_t;
typedef enum {
  I2S_CHANNEL_FMT_RIGHT_LEFT,
  I2S_CHANNEL_FMT_ALL_RIGHT,
  I2S_CHANNEL_FMT_ALL_LEFT,
  I2S_CHANNEL_FMT_ONLY_RIGHT,
  I2S_CHANNEL_FMT_ONLY_LEFT,
} i2s_channel_fmt_t;
typedef enum {
  I2S_COMM_FORMAT_STAND_I2S = 0X01,
  I2S_COMM_FORMAT_STAND_MSB = 0X02,
  I2S_COMM_FORMAT_STAND_PCM_SHORT = 0x04,
  I2S_COMM_FORMAT_STAND_PCM_LONG = 0x0C,
} i2s_comm_format_t;
typedef enum { I2S_MCLK_MULTIPLE_DEFAULT = 0 } i2s_mclk_multiple_t;
#define I2S_PIN_NO_CHANGE (-1)

typedef struct {
  i2s_mode_t mode;
  uint32_t sample_rate;
  i2s_bits_per_sample_t bits_per_sample;
  i2s_channel_fmt_t channel_format;
  i2s_comm_format_t communication_format;
  int intr_alloc_flags;
  int dma_buf_count;
  int dma_buf_len;
  bool use_apll;
  bool tx_desc_auto_clear;
  int fixed_mclk;
  i2s_mclk_multiple_t mclk_multiple;
  i2s_bits_per_chan_t bits_per_chan;
  uint32_t chan_mask;
  uint32_t total_chan;
  bool left_align;
  bool big_edin;
  bool bit_order_msb;
  bool skip_msk;
} i2s_config_t;

typedef struct {
  int mck_io_num;
  int bck_io_num;
  int ws_io_num;
  int data_out_num;
  int data_in_num;
} i2s_pin_config_t;

esp_err_t i2s_driver_install(i2s_port_t i2s_num, const i2s_config_t* i2s_config, int queue_size, void* i2s_queue);
esp_err_t i2s_set_pin(i2s_port_t i2s_num, const i2s_pin_config_t* pin);
esp_err_t i2s_zero_dma_buffer(i2s_port_t i2s_num);
esp_err_t i2s_write(i2s_port_t i2s_num, const void* src, size_t size, size_t* bytes_written, TickType_t ticks_to_wait);
