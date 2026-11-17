#include "proto.h"
#include <string.h>
namespace ss { namespace proto {

uint16_t crc16(const uint8_t* d, size_t n) {
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    c ^= (uint16_t)d[i] << 8;
    for (int b = 0; b < 8; b++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
  }
  return c;
}

size_t encode(uint8_t type, uint8_t seq, const uint8_t* payload, uint8_t len, uint8_t* out, size_t cap) {
  if (len > MAX_PAYLOAD || cap < (size_t)len + OVERHEAD) return 0;
  out[0] = SOF0; out[1] = SOF1; out[2] = VERSION; out[3] = type; out[4] = seq; out[5] = len;
  if (len) memcpy(out + 6, payload, len);
  uint16_t c = crc16(out + 2, (size_t)len + 4);
  out[6 + len] = (uint8_t)(c & 0xFF); out[7 + len] = (uint8_t)(c >> 8);
  return (size_t)len + OVERHEAD;
}

static inline void w16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static inline void w32(uint8_t* p, uint32_t v) { w16(p, (uint16_t)v); w16(p + 2, (uint16_t)(v >> 16)); }
static inline uint16_t r16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t r32(const uint8_t* p) { return (uint32_t)r16(p) | ((uint32_t)r16(p + 2) << 16); }

size_t pack(const Hello& h, uint8_t* p) { p[0] = h.role; p[1] = h.proto_ver; w16(p + 2, h.fw_ver); return 4; }
size_t pack(const RfSample& s, uint8_t* p) {
  w32(p, s.t_ms); w16(p + 4, (uint16_t)s.avg_cdbm); w16(p + 6, (uint16_t)s.min_cdbm);
  w16(p + 8, (uint16_t)s.max_cdbm); w16(p + 10, (uint16_t)s.floor_cdbm); return 12;
}
size_t pack(const Verdict& v, uint8_t* p) {
  p[0] = v.state; p[1] = v.confidence; w16(p + 2, (uint16_t)v.excess_cdb); w16(p + 4, (uint16_t)v.expected_cdbm);
  p[6] = v.n_wifi; p[7] = v.n_ble; return 8;
}
size_t pack(const ScanSummary& s, uint8_t* p) {
  p[0] = s.n_wifi; p[1] = s.n_ble; w16(p + 2, (uint16_t)s.strongest_wifi_cdbm);
  w16(p + 4, (uint16_t)s.strongest_ble_cdbm); w16(p + 6, (uint16_t)s.sum_cdbm); return 8;
}
bool unpack(const uint8_t* p, size_t n, Hello& h) {
  if (n < 4) return false;
  h.role = p[0]; h.proto_ver = p[1]; h.fw_ver = r16(p + 2); return true;
}
bool unpack(const uint8_t* p, size_t n, RfSample& s) {
  if (n < 12) return false;
  s.t_ms = r32(p); s.avg_cdbm = (int16_t)r16(p + 4); s.min_cdbm = (int16_t)r16(p + 6);
  s.max_cdbm = (int16_t)r16(p + 8); s.floor_cdbm = (int16_t)r16(p + 10); return true;
}
bool unpack(const uint8_t* p, size_t n, Verdict& v) {
  if (n < 8) return false;
  v.state = p[0]; v.confidence = p[1]; v.excess_cdb = (int16_t)r16(p + 2); v.expected_cdbm = (int16_t)r16(p + 4);
  v.n_wifi = p[6]; v.n_ble = p[7]; return true;
}
bool unpack(const uint8_t* p, size_t n, ScanSummary& s) {
  if (n < 8) return false;
  s.n_wifi = p[0]; s.n_ble = p[1]; s.strongest_wifi_cdbm = (int16_t)r16(p + 2);
  s.strongest_ble_cdbm = (int16_t)r16(p + 4); s.sum_cdbm = (int16_t)r16(p + 6); return true;
}

bool Parser::feed(uint8_t b) {
  switch (st_) {
    case 0: if (b == SOF0) st_ = 1; break;
    case 1: st_ = (b == SOF1) ? 2 : (b == SOF0 ? 1 : 0); break;
    case 2: frame.ver = b; st_ = 3; break;
    case 3: frame.type = b; st_ = 4; break;
    case 4: frame.seq = b; st_ = 5; break;
    case 5:
      frame.len = b; idx_ = 0;
      if (b > MAX_PAYLOAD) { len_errors++; st_ = 0; }
      else st_ = b ? 6 : 7;
      break;
    case 6: frame.payload[idx_++] = b; if (idx_ >= frame.len) st_ = 7; break;
    case 7: crcl_ = b; st_ = 8; break;
    case 8: {
      st_ = 0;
      uint8_t hdr[4 + MAX_PAYLOAD];
      hdr[0] = frame.ver; hdr[1] = frame.type; hdr[2] = frame.seq; hdr[3] = frame.len;
      memcpy(hdr + 4, frame.payload, frame.len);
      uint16_t c = crc16(hdr, (size_t)frame.len + 4);
      if (c != (uint16_t)(crcl_ | (b << 8))) { crc_errors++; return false; }
      if (frame.ver != VERSION) { ver_errors++; return false; }
      return true;
    }
  }
  return false;
}
}} // namespace
