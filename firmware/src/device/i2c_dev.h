// Small register-level drivers for the I2C parts on the base board.
// One global mutex serialises the bus across FreeRTOS tasks.
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "pins.h"

namespace dev {

extern SemaphoreHandle_t i2c_mutex;
void i2c_begin();
bool wr(uint8_t addr, uint8_t reg, uint8_t val);
bool rd(uint8_t addr, uint8_t reg, uint8_t* buf, size_t n);
bool present(uint8_t addr);

static const uint8_t ADDR_AW9523 = 0x58, ADDR_MAX17048 = 0x36, ADDR_DRV2605 = 0x5A, ADDR_BME280 = 0x76, ADDR_LSM6DSO = 0x6A;

// AW9523B: keys, lock switch, charger status, power enables, RGB LED.
class Expander {
 public:
  bool begin();
  bool ok() const { return ok_; }
  uint16_t inputs();                     // raw 16 bits, bit index = EXP_* from pins.h
  void out(uint8_t bit, bool high);      // port-1 push-pull outputs
  void led(uint8_t r, uint8_t g, uint8_t b);
 private:
  bool ok_ = false; uint8_t out1_ = 0; uint16_t last_ = 0xFFFF;
};

class FuelGauge { public: bool begin(); bool read(uint16_t& vcell, uint16_t& soc, uint16_t& crate); private: bool ok_ = false; };
class Haptic { public: bool begin(); void play(uint8_t effect); private: bool ok_ = false; };
class Bme280 {
 public:
  bool begin(); bool read(float& t, float& rh, float& hpa);
 private:
  bool ok_ = false; uint16_t T1, P1; int16_t T2, T3, P2, P3, P4, P5, P6, P7, P8, P9, H2, H4, H5; uint8_t H1, H3; int8_t H6; int32_t tfine_ = 0;
};
class Imu { public: bool begin(); bool read(float& ax, float& ay, float& az); private: bool ok_ = false; };
}
