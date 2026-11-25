#include "lcd.h"

#include <Arduino.h>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <string.h>

#include "board.h"

namespace {
constexpr int kChunkRows = 16;  // even, matches the panel's 2-pixel alignment
spi_device_handle_t dev = nullptr;
uint16_t* dma[2] = {};

// Register write: opcode 0x02, 24-bit address 0x00RR00, single-line data.
void command(uint8_t reg, const uint8_t* data = nullptr, size_t len = 0) {
  spi_transaction_t t = {};
  t.cmd = 0x02;
  t.addr = uint32_t(reg) << 8;
  t.length = len * 8;
  t.tx_buffer = len ? data : nullptr;
  ESP_ERROR_CHECK(spi_device_polling_transmit(dev, &t));
}
void command1(uint8_t reg, uint8_t v) { command(reg, &v, 1); }
void window(int x0, int y0, int x1, int y1) {
  const uint8_t c[4] = {uint8_t(x0 >> 8), uint8_t(x0), uint8_t(x1 >> 8), uint8_t(x1)};
  const uint8_t r[4] = {uint8_t(y0 >> 8), uint8_t(y0), uint8_t(y1 >> 8), uint8_t(y1)};
  command(0x2A, c, 4);
  command(0x2B, r, 4);
}
}  // namespace

void lcdBegin() {
  spi_bus_config_t bus = {};
  bus.data0_io_num = PIN_LCD_D0;
  bus.data1_io_num = PIN_LCD_D1;
  bus.sclk_io_num = PIN_LCD_SCLK;
  bus.data2_io_num = PIN_LCD_D2;
  bus.data3_io_num = PIN_LCD_D3;
  bus.max_transfer_sz = kScreenW * kChunkRows * 2;
  bus.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_QUAD;
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

  spi_device_interface_config_t cfg = {};
  cfg.command_bits = 8;
  cfg.address_bits = 24;
  cfg.mode = 0;
  cfg.clock_speed_hz = 40 * 1000 * 1000;
  cfg.spics_io_num = PIN_LCD_CS;
  cfg.queue_size = 2;
  cfg.flags = SPI_DEVICE_HALFDUPLEX;
  ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &cfg, &dev));

  for (auto& b : dma) b = (uint16_t*)heap_caps_malloc(kScreenW * kChunkRows * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

  pinMode(PIN_LCD_RST, OUTPUT);
  digitalWrite(PIN_LCD_RST, HIGH); delay(10);
  digitalWrite(PIN_LCD_RST, LOW); delay(10);
  digitalWrite(PIN_LCD_RST, HIGH); delay(120);

  // Waveshare BSP init sequence for the 2.16" panel.
  command(0x11); delay(120);
  command1(0xFE, 0x20); command1(0x19, 0x10); command1(0x1C, 0xA0);
  command1(0xFE, 0x00); command1(0xC4, 0x80); command1(0x3A, 0x55);
  command1(0x35, 0x00); command1(0x53, 0x20); command1(0x51, 0x00);
  command1(0x63, 0xFF);
  window(0, 0, kScreenW - 1, kScreenH - 1);
  command1(0x36, 0xA0);
  command(0x29); delay(20);
}

void lcdBrightness(uint8_t level) { command1(0x51, level); }

void lcdPushRows(const uint16_t* frame, int y0, int y1) {
  y0 &= ~1; y1 = (y1 + 1) & ~1;
  if (y1 > kScreenH) y1 = kScreenH;
  if (y0 >= y1) return;
  window(0, y0, kScreenW - 1, y1 - 1);
  bool first = true;
  int slot = 0;
  spi_transaction_t ts[2] = {};
  int pending = 0;
  for (int y = y0; y < y1; y += kChunkRows, slot ^= 1) {
    const int rows = min(kChunkRows, y1 - y);
    const int n = rows * kScreenW;
    if (pending == 2) { spi_transaction_t* done; spi_device_get_trans_result(dev, &done, portMAX_DELAY); --pending; }
    const uint16_t* src = frame + y * kScreenW;
    uint16_t* dst = dma[slot];
    for (int i = 0; i < n; ++i) dst[i] = __builtin_bswap16(src[i]);  // panel wants big-endian
    spi_transaction_t& t = ts[slot];
    t = {};
    t.flags = SPI_TRANS_MODE_QIO;
    t.cmd = 0x32;
    t.addr = first ? 0x002C00 : 0x003C00;  // RAMWR, then RAMWR continue
    t.length = n * 16;
    t.tx_buffer = dst;
    ESP_ERROR_CHECK(spi_device_queue_trans(dev, &t, portMAX_DELAY));
    ++pending; first = false;
  }
  while (pending--) { spi_transaction_t* done; spi_device_get_trans_result(dev, &done, portMAX_DELAY); }
}
