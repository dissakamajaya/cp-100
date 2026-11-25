#include "codec.h"

#include <Arduino.h>
#include <Wire.h>

#include "board.h"

namespace {
// DAC volume register 0x32: 0xBF = 0 dB, 0.5 dB per step. The onboard
// speaker is a preview only; -12 dB keeps LEVEL 100 from clipping the amp.
constexpr uint8_t kDacVolume = 0xBF - 24;

bool write(uint8_t reg, uint8_t value) {
  for (int attempt = 0; attempt < 3; ++attempt) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg); Wire.write(value);
    if (Wire.endTransmission() == 0) return true;
    delay(2);
  }
  return false;
}
}  // namespace

// Register sequence follows Espressif esp_codec_dev es8311.c for slave mode,
// MCLK = 256 fs, 48 kHz, I2S normal format, 32-bit words, DAC only.
bool codecBegin() {
  pinMode(PIN_SPEAKER_AMP, OUTPUT);
  digitalWrite(PIN_SPEAKER_AMP, LOW);
  bool ok = true;
  const uint8_t seq[][2] = {
    {0x0D, 0xFA}, {0x44, 0x08}, {0x44, 0x08},
    {0x01, 0x30}, {0x02, 0x00}, {0x03, 0x10}, {0x16, 0x24}, {0x04, 0x10}, {0x05, 0x00},
    {0x0B, 0x00}, {0x0C, 0x00}, {0x10, 0x1F}, {0x11, 0x7F}, {0x00, 0x80},
    {0x00, 0x80},  // slave mode
    {0x01, 0x3F},  // clocks on, MCLK from pin, not inverted
    {0x06, 0x03},  // SCLK not inverted, BCLK = MCLK / 4
    {0x13, 0x10}, {0x1B, 0x0A}, {0x1C, 0x6A}, {0x44, 0x58},
    // 48 kHz from 12.288 MHz: pre_div 1, x1, adc/dac div 1, osr 0x10, LRCK 256.
    {0x02, 0x00}, {0x05, 0x00}, {0x03, 0x10}, {0x04, 0x10}, {0x07, 0x00}, {0x08, 0xFF},
    {0x09, 0x10}, {0x0A, 0x50},  // I2S, 32-bit; ADC input left disabled
    // start
    {0x00, 0x80}, {0x01, 0x3F}, {0x09, 0x10},
    {0x17, 0xBF}, {0x0E, 0x02}, {0x12, 0x00}, {0x14, 0x1A}, {0x0D, 0x01},
    {0x15, 0x40}, {0x37, 0x08}, {0x45, 0x00},
    {0x32, kDacVolume}, {0x31, 0x00},  // volume, unmute
  };
  for (const auto& r : seq) ok = write(r[0], r[1]) && ok;
  if (ok) digitalWrite(PIN_SPEAKER_AMP, HIGH);
  return ok;
}
