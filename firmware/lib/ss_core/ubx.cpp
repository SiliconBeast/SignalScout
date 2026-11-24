// SPDX-License-Identifier: MIT
#include "ubx.h"
#include <string.h>
namespace ss { namespace ubx {
size_t frame(uint8_t cls, uint8_t id, const uint8_t* p, uint16_t len, uint8_t* o, size_t cap) {
  if (cap < (size_t)len + 8) return 0;
  o[0] = 0xB5; o[1] = 0x62; o[2] = cls; o[3] = id; o[4] = (uint8_t)len; o[5] = (uint8_t)(len >> 8);
  if (len) memcpy(o + 6, p, len);
  uint8_t a = 0, b = 0;
  for (size_t i = 2; i < (size_t)len + 6; i++) { a = (uint8_t)(a + o[i]); b = (uint8_t)(b + a); }
  o[6 + len] = a; o[7 + len] = b;
  return (size_t)len + 8;
}
ValSet::ValSet() : n_(4) { buf_[0] = 0; buf_[1] = 0x01; buf_[2] = 0; buf_[3] = 0; } // version 0, RAM layer
bool ValSet::add(uint32_t key, uint32_t v) {
  int sz = (key >> 28) & 7; // 1: bit, 2: 1 byte, 3: 2 bytes, 4: 4 bytes
  int bytes = sz <= 2 ? 1 : (sz == 3 ? 2 : 4);
  if (n_ + 4 + bytes > sizeof buf_) return false;
  for (int i = 0; i < 4; i++) buf_[n_++] = (uint8_t)(key >> (8 * i));
  for (int i = 0; i < bytes; i++) buf_[n_++] = (uint8_t)(v >> (8 * i));
  return true;
}
size_t ValSet::finish(uint8_t* o, size_t cap) { return frame(CLS_CFG, ID_CFG_VALSET, buf_, (uint16_t)n_, o, cap); }
size_t boot_config(bool psm, uint8_t* o, size_t cap) {
  ValSet v;
  v.add(K_UART1OUT_UBX, 1); v.add(K_UART1OUT_NMEA, 0); v.add(K_MSGOUT_NAV_PVT_UART1, 1); v.add(K_RATE_MEAS, 1000);
  v.add(K_SIG_GPS, 1); v.add(K_SIG_GAL, 1); v.add(K_SIG_BDS, 1); v.add(K_SIG_GLO, 0);
  v.add(K_PM_OPERATEMODE, psm ? 2 : 0); // 2 = cyclic tracking power save, 0 = full power
  return v.finish(o, cap);
}
size_t backup_request(uint8_t* o, size_t cap) {
  // version 0, duration 0 (until woken), flags = backup | force, wakeupSources = uartrx
  const uint8_t p[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0x06, 0, 0, 0, 0x08, 0, 0, 0};
  return frame(0x02, 0x41, p, sizeof p, o, cap);
}
bool Parser::feed(uint8_t c) {
  switch (st_) {
    case 0: if (c == 0xB5) st_ = 1; break;
    case 1: st_ = (c == 0x62) ? 2 : (c == 0xB5 ? 1 : 0); break;
    case 2: msg.cls = c; a_ = c; b_ = c; st_ = 3; break;
    case 3: msg.id = c; a_ = (uint8_t)(a_ + c); b_ = (uint8_t)(b_ + a_); st_ = 4; break;
    case 4: msg.len = c; a_ = (uint8_t)(a_ + c); b_ = (uint8_t)(b_ + a_); st_ = 5; break;
    case 5:
      msg.len |= (uint16_t)(c << 8); a_ = (uint8_t)(a_ + c); b_ = (uint8_t)(b_ + a_); i_ = 0;
      if (msg.len > 1024) { st_ = 0; break; }
      st_ = msg.len ? 6 : 7; break;
    case 6:
      if (i_ < sizeof msg.payload) msg.payload[i_] = c;
      a_ = (uint8_t)(a_ + c); b_ = (uint8_t)(b_ + a_);
      if (++i_ >= msg.len) st_ = 7;
      break;
    case 7: st_ = (c == a_) ? 8 : 0; if (st_ == 0) ck_errors++; break;
    case 8: st_ = 0; if (c == b_) return true; ck_errors++; break;
  }
  return false;
}
static int32_t i32(const uint8_t* p) { return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24)); }
bool decode_nav_pvt(const Msg& m, GnssFix& f) {
  if (m.cls != CLS_NAV || m.id != ID_NAV_PVT || m.len < 92) return false;
  const uint8_t* p = m.payload;
  uint8_t valid = p[11], flags = p[21];
  f.year = (uint16_t)(p[4] | (p[5] << 8)); f.month = p[6]; f.day = p[7]; f.hour = p[8]; f.minute = p[9]; f.second = p[10];
  f.time_valid = (valid & 0x03) == 0x03;
  f.fix_type = p[20]; f.valid = (flags & 1) && f.fix_type >= 2;
  f.sats = p[23];
  f.lon = i32(p + 24) * 1e-7; f.lat = i32(p + 28) * 1e-7;
  f.alt_m = i32(p + 36) / 1000.0f; f.hacc_m = (uint32_t)i32(p + 40) / 1000.0f;
  f.speed_mps = i32(p + 60) / 1000.0f; f.heading_deg = i32(p + 64) * 1e-5f;
  return true;
}
}}
