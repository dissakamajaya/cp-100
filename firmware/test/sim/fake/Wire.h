#pragma once
// libraries/Wire/src/Wire.h (core 2.0.17) subset. The overload set mirrors the
// real header so overload resolution in panel.cpp is the same as on the chip.
// endTransmission(false) behaves like the real core: it only marks a repeated
// start and returns 0; the write+read happens inside requestFrom().
#include "Arduino.h"

class TwoWire {
 public:
  bool begin(int sda, int scl, uint32_t frequency = 0);
  bool begin(uint8_t slaveAddr, int sda, int scl, uint32_t frequency);
  void setTimeOut(uint16_t timeOutMillis);
  uint16_t getTimeOut();

  void beginTransmission(uint16_t address);
  void beginTransmission(uint8_t address);
  void beginTransmission(int address);

  uint8_t endTransmission(bool sendStop);
  uint8_t endTransmission(void);

  size_t requestFrom(uint16_t address, size_t size, bool sendStop);
  uint8_t requestFrom(uint16_t address, uint8_t size, bool sendStop);
  uint8_t requestFrom(uint16_t address, uint8_t size, uint8_t sendStop);
  size_t requestFrom(uint8_t address, size_t len, bool stopBit);
  uint8_t requestFrom(uint16_t address, uint8_t size);
  uint8_t requestFrom(uint8_t address, uint8_t size, uint8_t sendStop);
  uint8_t requestFrom(uint8_t address, uint8_t size);
  uint8_t requestFrom(int address, int size, int sendStop);
  uint8_t requestFrom(int address, int size);

  size_t write(uint8_t);
  size_t write(const uint8_t*, size_t);
  int available(void);
  int read(void);
  inline size_t write(unsigned long n) { return write((uint8_t)n); }
  inline size_t write(long n) { return write((uint8_t)n); }
  inline size_t write(unsigned int n) { return write((uint8_t)n); }
  inline size_t write(int n) { return write((uint8_t)n); }

 private:
  uint16_t txAddress = 0;
  uint8_t txBuffer[128];
  size_t txLength = 0;
  bool nonStop = false;
  uint8_t rxBuffer[128];
  size_t rxIndex = 0, rxLength = 0;
};
extern TwoWire Wire;
