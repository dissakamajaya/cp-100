#pragma once
// libraries/SPI/src/SPI.h (core 2.0.17) subset, wired to the simulated MAX7219 chain.
#include "Arduino.h"

#define FSPI 0  // ESP32-S3 value in esp32-hal-spi.h
#define HSPI 1
#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3

class SPISettings {
 public:
  SPISettings() : _clock(1000000), _bitOrder(MSBFIRST), _dataMode(SPI_MODE0) {}
  SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode)
      : _clock(clock), _bitOrder(bitOrder), _dataMode(dataMode) {}
  uint32_t _clock;
  uint8_t _bitOrder;
  uint8_t _dataMode;
};

class SPIClass {
 public:
  SPIClass(uint8_t spi_bus = HSPI) : bus_(spi_bus) {}
  void begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1);
  void end() {}
  void beginTransaction(SPISettings settings);
  void endTransaction(void);
  void transfer(void* data, uint32_t size);
  uint8_t transfer(uint8_t data);
  uint16_t transfer16(uint16_t data);

 private:
  uint8_t bus_;
};
