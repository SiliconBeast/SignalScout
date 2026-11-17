#include "i2c_dev.h"
namespace dev {

SemaphoreHandle_t i2c_mutex = nullptr;
struct Lock { Lock() { xSemaphoreTake(i2c_mutex, portMAX_DELAY); } ~Lock() { xSemaphoreGive(i2c_mutex); } };

void i2c_begin() {
  if (!i2c_mutex) i2c_mutex = xSemaphoreCreateMutex();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  Wire.setTimeOut(20);
}
bool wr(uint8_t a, uint8_t reg, uint8_t v) { Lock l; Wire.beginTransmission(a); Wire.write(reg); Wire.write(v); return Wire.endTransmission() == 0; }
bool rd(uint8_t a, uint8_t reg, uint8_t* buf, size_t n) {
  Lock l; Wire.beginTransmission(a); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)a, (int)n) != (int)n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = (uint8_t)Wire.read();
  return true;
}
bool present(uint8_t a) { Lock l; Wire.beginTransmission(a); return Wire.endTransmission() == 0; }

// ---- AW9523B ---------------------------------------------------------------
// Registers: 0x00/01 input, 0x02/03 output, 0x04/05 config (1 = input), 0x06/07 int enable (0 = on),
// 0x10 ID (0x23), 0x11 GCR, 0x12/13 mode (0 = LED, 1 = GPIO), 0x2C..0x2F LED current for P1_4..P1_7.
bool Expander::begin() {
  // The AW9523B powers up with every pin an output driven low (address 0x58 straps AD1/AD0 low).
  // That turns the LED full on and drives P0 low against the key pull-ups, so this runs first
  // in hal_begin(). Rev C7 adds series resistors to the LED (docs/HARDWARE_REVIEW.md, item 1).
  uint8_t id = 0;
  ok_ = rd(ADDR_AW9523, 0x10, &id, 1) && id == 0x23;
  if (!ok_) return false;
  out1_ = (uint8_t)((1u << (EXP_DISP_PWR_EN - 8)) | (1u << (EXP_GNSS_RST - 8))); // display rail on, GNSS out of reset, sled wake low
  wr(ADDR_AW9523, 0x13, 0x1F);  // P1_5..P1_7 in LED mode first: stops the power-on LED current
  wr(ADDR_AW9523, 0x11, 0x13);  // port 0 push-pull, LED current range = Imax / 4 (about 9 mA)
  wr(ADDR_AW9523, 0x03, out1_);
  wr(ADDR_AW9523, 0x12, 0xFF);  // port 0: all GPIO
  wr(ADDR_AW9523, 0x04, 0xFF);  // port 0: all inputs (keys, spare)
  wr(ADDR_AW9523, 0x05, (uint8_t)((1u << (EXP_KEY_LOCK - 8)) | (1u << (EXP_CHG_N - 8)))); // P1: lock + CHG_N in, rest out
  // Interrupt on every key and on SPARE_IO: from rev C7 it carries charger PGOOD and the IMU
  // free-fall interrupt (wired-OR, active low), so plugging in or a fall wakes the base.
  wr(ADDR_AW9523, 0x06, 0x00);
  wr(ADDR_AW9523, 0x07, (uint8_t)~((1u << (EXP_KEY_LOCK - 8)) | (1u << (EXP_CHG_N - 8))));
  led(0, 0, 0);
  inputs();
  return true;
}
uint16_t Expander::inputs() {
  uint8_t b[2];
  if (ok_ && rd(ADDR_AW9523, 0x00, b, 2)) last_ = (uint16_t)(b[0] | (b[1] << 8));
  return last_;
}
void Expander::out(uint8_t bit, bool high) {
  if (!ok_ || bit < 8) return;
  uint8_t m = (uint8_t)(1u << (bit - 8)); uint8_t v = high ? (out1_ | m) : (out1_ & ~m);
  if (v != out1_) { out1_ = v; wr(ADDR_AW9523, 0x03, out1_); }
}
void Expander::led(uint8_t r, uint8_t g, uint8_t b) {
  if (!ok_) return;
  // Common-anode LED on current sinks: 0x2D = P1_5 (red), 0x2E = P1_6 (green), 0x2F = P1_7 (blue).
  wr(ADDR_AW9523, 0x2D, r); wr(ADDR_AW9523, 0x2E, g); wr(ADDR_AW9523, 0x2F, b);
}

// ---- MAX17048 ----------------------------------------------------------------
bool FuelGauge::begin() { uint8_t v[2]; ok_ = rd(ADDR_MAX17048, 0x08, v, 2); return ok_; } // VERSION
bool FuelGauge::read(uint16_t& vcell, uint16_t& soc, uint16_t& crate) {
  uint8_t b[2];
  if (!ok_ && !begin()) return false;
  if (!rd(ADDR_MAX17048, 0x02, b, 2)) return false;
  vcell = (uint16_t)((b[0] << 8) | b[1]);
  if (!rd(ADDR_MAX17048, 0x04, b, 2)) return false;
  soc = (uint16_t)((b[0] << 8) | b[1]);
  if (!rd(ADDR_MAX17048, 0x16, b, 2)) return false;
  crate = (uint16_t)((b[0] << 8) | b[1]);
  return true;
}

// ---- DRV2605L ------------------------------------------------------------------
bool Haptic::begin() {
  pinMode(PIN_HAPTIC_EN, OUTPUT); digitalWrite(PIN_HAPTIC_EN, HIGH); delay(2);
  uint8_t st = 0; ok_ = rd(ADDR_DRV2605, 0x00, &st, 1);
  if (!ok_) return false;
  wr(ADDR_DRV2605, 0x01, 0x00);  // out of standby, internal trigger
  wr(ADDR_DRV2605, 0x1A, 0xB6);  // feedback control: LRA mode
  wr(ADDR_DRV2605, 0x03, 0x06);  // LRA effect library
  return true;
}
void Haptic::play(uint8_t effect) {
  if (!ok_) return;
  wr(ADDR_DRV2605, 0x04, effect); wr(ADDR_DRV2605, 0x05, 0x00); wr(ADDR_DRV2605, 0x0C, 0x01);
}

// ---- BME280 (forced mode, Bosch datasheet compensation formulas) -----------------
bool Bme280::begin() {
  uint8_t id = 0, c[26], h[7];
  ok_ = rd(ADDR_BME280, 0xD0, &id, 1) && id == 0x60;
  if (!ok_ || !rd(ADDR_BME280, 0x88, c, 26) || !rd(ADDR_BME280, 0xE1, h, 7)) { ok_ = false; return false; }
  T1 = (uint16_t)(c[0] | (c[1] << 8)); T2 = (int16_t)(c[2] | (c[3] << 8)); T3 = (int16_t)(c[4] | (c[5] << 8));
  P1 = (uint16_t)(c[6] | (c[7] << 8)); P2 = (int16_t)(c[8] | (c[9] << 8)); P3 = (int16_t)(c[10] | (c[11] << 8)); P4 = (int16_t)(c[12] | (c[13] << 8));
  P5 = (int16_t)(c[14] | (c[15] << 8)); P6 = (int16_t)(c[16] | (c[17] << 8)); P7 = (int16_t)(c[18] | (c[19] << 8)); P8 = (int16_t)(c[20] | (c[21] << 8));
  P9 = (int16_t)(c[22] | (c[23] << 8)); H1 = c[25];
  H2 = (int16_t)(h[0] | (h[1] << 8)); H3 = h[2]; H4 = (int16_t)((h[3] << 4) | (h[4] & 0x0F)); H5 = (int16_t)((h[5] << 4) | (h[4] >> 4)); H6 = (int8_t)h[6];
  wr(ADDR_BME280, 0xF2, 0x01); wr(ADDR_BME280, 0xF5, 0x00);
  return true;
}
bool Bme280::read(float& t, float& rh, float& hpa) {
  if (!ok_) return false;
  wr(ADDR_BME280, 0xF4, 0x25); // x1 oversampling, one forced conversion
  vTaskDelay(pdMS_TO_TICKS(12));
  uint8_t d[8];
  if (!rd(ADDR_BME280, 0xF7, d, 8)) return false;
  int32_t ap = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4), at = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4), ah = ((int32_t)d[6] << 8) | d[7];
  float v1 = (at / 16384.0f - T1 / 1024.0f) * T2, v2 = (at / 131072.0f - T1 / 8192.0f) * (at / 131072.0f - T1 / 8192.0f) * T3;
  float tf = v1 + v2; t = tf / 5120.0f;
  float p1 = tf / 2.0f - 64000.0f, p2 = p1 * p1 * P6 / 32768.0f; p2 = p2 + p1 * P5 * 2.0f; p2 = p2 / 4.0f + P4 * 65536.0f;
  p1 = (P3 * p1 * p1 / 524288.0f + P2 * p1) / 524288.0f; p1 = (1.0f + p1 / 32768.0f) * P1;
  if (p1 == 0) return false;
  float p = 1048576.0f - ap; p = (p - p2 / 4096.0f) * 6250.0f / p1;
  p1 = P9 * p * p / 2147483648.0f; p2 = p * P8 / 32768.0f; hpa = (p + (p1 + p2 + P7) / 16.0f) / 100.0f;
  float hh = tf - 76800.0f;
  hh = (ah - (H4 * 64.0f + H5 / 16384.0f * hh)) * (H2 / 65536.0f * (1.0f + H6 / 67108864.0f * hh * (1.0f + H3 / 67108864.0f * hh)));
  hh = hh * (1.0f - H1 * hh / 524288.0f);
  rh = hh > 100 ? 100 : (hh < 0 ? 0 : hh);
  return true;
}

// ---- LSM6DSO -------------------------------------------------------------------
bool Imu::begin() {
  uint8_t id = 0; ok_ = rd(ADDR_LSM6DSO, 0x0F, &id, 1) && (id == 0x6C || id == 0x6B || id == 0x6A);
  if (!ok_) return false;
  wr(ADDR_LSM6DSO, 0x10, 0x20); // accelerometer 26 Hz, +/- 2 g
  // INT1 open-drain, active low: it shares SPARE_IO with the charger's PGOOD from rev C7.
  wr(ADDR_LSM6DSO, 0x12, 0x74); // CTRL3_C: BDU, H_LACTIVE, PP_OD, IF_INC
  wr(ADDR_LSM6DSO, 0x5D, 0x33); // FREE_FALL: 312 mg for 6 samples (~230 ms at 26 Hz)
  wr(ADDR_LSM6DSO, 0x58, 0x80); // TAP_CFG2: embedded-function interrupts on
  wr(ADDR_LSM6DSO, 0x5E, 0x10); // MD1_CFG: free fall -> INT1
  return true;
}
bool Imu::free_fall() { uint8_t s = 0; return ok_ && rd(ADDR_LSM6DSO, 0x1B, &s, 1) && (s & 0x20); } // WAKE_UP_SRC.FF_IA
bool Imu::read(float& ax, float& ay, float& az) {
  uint8_t d[6];
  if (!ok_ || !rd(ADDR_LSM6DSO, 0x28, d, 6)) return false;
  const float k = 0.061e-3f;
  ax = (int16_t)(d[0] | (d[1] << 8)) * k; ay = (int16_t)(d[2] | (d[3] << 8)) * k; az = (int16_t)(d[4] | (d[5] << 8)) * k;
  return true;
}
}
