#pragma once
// ES8311 DAC + speaker amplifier. Wire must already run on PIN_I2C_SDA/SCL,
// and I2S must already be driving MCLK/BCK/LRCK (slave mode, 48 kHz, 32-bit).
bool codecBegin();
