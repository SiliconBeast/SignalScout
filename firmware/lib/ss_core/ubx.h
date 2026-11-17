// Minimal u-blox UBX support for the MAX-M10S: frame build/parse, NAV-PVT
// decode and CFG-VALSET configuration (M10 has no legacy CFG-* messages).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "model.h"
namespace ss { namespace ubx {

static const uint8_t CLS_NAV = 0x01, ID_NAV_PVT = 0x07, CLS_CFG = 0x06, ID_CFG_VALSET = 0x8A, CLS_ACK = 0x05;

// Configuration keys (u-blox M10 SPG 5.10 interface description).
static const uint32_t K_UART1OUT_UBX = 0x10740001, K_UART1OUT_NMEA = 0x10740002,
  K_MSGOUT_NAV_PVT_UART1 = 0x20910007, K_RATE_MEAS = 0x30210001,
  K_SIG_GPS = 0x1031001F, K_SIG_SBAS = 0x10310020, K_SIG_GAL = 0x10310021, K_SIG_BDS = 0x10310022,
  K_SIG_QZSS = 0x10310024, K_SIG_GLO = 0x10310025, K_PM_OPERATEMODE = 0x20D00001;

size_t frame(uint8_t cls, uint8_t id, const uint8_t* payload, uint16_t len, uint8_t* out, size_t cap);

// VALSET builder (RAM layer). Value size comes from the key's size nibble.
class ValSet {
 public:
  ValSet();
  bool add(uint32_t key, uint32_t value);
  size_t finish(uint8_t* out, size_t cap); // returns a complete UBX frame
 private:
  uint8_t buf_[96]; size_t n_;
};
// Boot configuration: UBX only, NAV-PVT at 1 Hz, GPS+Galileo+BeiDou, optional power save.
size_t boot_config(bool power_save, uint8_t* out, size_t cap);

struct Msg { uint8_t cls, id; uint16_t len; uint8_t payload[100]; };
class Parser {
 public:
  bool feed(uint8_t b); // true when msg holds a checksum-valid message (payload truncated to 100 bytes)
  Msg msg;
  uint32_t ck_errors = 0;
 private:
  uint8_t st_ = 0, a_ = 0, b_ = 0; uint16_t i_ = 0;
};
bool decode_nav_pvt(const Msg& m, GnssFix& fix);
}}
