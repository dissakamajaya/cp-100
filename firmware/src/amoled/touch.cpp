#include "touch.h"

#include <Arduino.h>
#include <Wire.h>

#include "board.h"

void touchBegin() {
  pinMode(PIN_TOUCH_INT, INPUT_PULLUP);
  pinMode(PIN_TOUCH_RST, OUTPUT);
  digitalWrite(PIN_TOUCH_RST, LOW); delay(10);
  digitalWrite(PIN_TOUCH_RST, HIGH); delay(50);
}

// Frame at register 0xD000: point 0 in bytes 0-4, count in byte 5, ACK 0xAB in byte 6.
TouchPoint touchRead() {
  TouchPoint t;
  uint8_t d[10] = {};
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(0xD0); Wire.write(0x00);
  if (Wire.endTransmission() != 0) return t;
  if (Wire.requestFrom(TOUCH_ADDR, uint8_t(sizeof(d))) != sizeof(d)) return t;
  for (auto& b : d) b = Wire.read();
  if (d[6] != 0xAB || (d[5] & 0x7F) == 0 || (d[0] & 0x0F) != 0x06) return t;
  const int rawX = (d[1] << 4) | (d[3] >> 4);
  const int rawY = (d[2] << 4) | (d[3] & 0x0F);
  // Same transform as the Waveshare BSP for MADCTL 0xA0: mirror Y, then swap.
  t.x = constrain(kScreenW - rawY, 0, kScreenW - 1);
  t.y = constrain(rawX, 0, kScreenH - 1);
  t.down = true;
  return t;
}
