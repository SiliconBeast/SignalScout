// Signal Scout pogo-link protocol (base ESP32-S3 <-> sled UNO Q STM32).
// Spec: docs/PROTOCOL.md. This header is the single source of truth and is
// compiled into the base firmware AND the sled sketch (plain C++11, no deps).
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace ss { namespace proto {

static const uint8_t SOF0 = 0xA5, SOF1 = 0x5A;
static const uint8_t VERSION = 1;
static const size_t MAX_PAYLOAD = 64;
static const size_t OVERHEAD = 8; // SOF0 SOF1 ver type seq len ... crcL crcH

enum Type : uint8_t {
  T_HELLO = 0x01,     // either way. payload: Hello
  T_PING = 0x02,      // base -> sled, empty
  T_PONG = 0x03,      // sled -> base, empty
  T_RF_SAMPLE = 0x10, // sled -> base. payload: RfSample (20 Hz)
  T_VERDICT = 0x11,   // sled -> base. payload: Verdict (about 1 Hz)
  T_SCAN_SUMMARY = 0x20, // base -> sled. payload: ScanSummary (after each scan)
  T_SLEEP = 0x30,     // base -> sled, empty: stop streaming
  T_WAKE = 0x31,      // base -> sled, empty: start streaming
};

enum Role : uint8_t { ROLE_BASE = 1, ROLE_SLED = 2 };

// All multi-byte fields little-endian. Power in centi-dBm (dBm * 100).
struct Hello { uint8_t role; uint8_t proto_ver; uint16_t fw_ver; };
struct RfSample { uint32_t t_ms; int16_t avg_cdbm, min_cdbm, max_cdbm, floor_cdbm; };
struct Verdict { uint8_t state; uint8_t confidence; int16_t excess_cdb; int16_t expected_cdbm; uint8_t n_wifi, n_ble; };
struct ScanSummary { uint8_t n_wifi, n_ble; int16_t strongest_wifi_cdbm, strongest_ble_cdbm; int16_t sum_cdbm; };

uint16_t crc16(const uint8_t* d, size_t n); // CRC-16/CCITT-FALSE over ver..payload

// Build a frame into out (needs len + OVERHEAD bytes). Returns total size, 0 on error.
size_t encode(uint8_t type, uint8_t seq, const uint8_t* payload, uint8_t len, uint8_t* out, size_t cap);

size_t pack(const Hello&, uint8_t* p);
size_t pack(const RfSample&, uint8_t* p);
size_t pack(const Verdict&, uint8_t* p);
size_t pack(const ScanSummary&, uint8_t* p);
bool unpack(const uint8_t* p, size_t n, Hello&);
bool unpack(const uint8_t* p, size_t n, RfSample&);
bool unpack(const uint8_t* p, size_t n, Verdict&);
bool unpack(const uint8_t* p, size_t n, ScanSummary&);

struct Frame { uint8_t ver, type, seq, len; uint8_t payload[MAX_PAYLOAD]; };

// Streaming decoder: feed bytes one at a time; returns true when a valid frame
// is complete in `frame`. Bad CRC / bad length / wrong version are counted and skipped.
class Parser {
 public:
  bool feed(uint8_t b);
  Frame frame;
  uint32_t crc_errors = 0, ver_errors = 0, len_errors = 0;
 private:
  uint8_t st_ = 0, idx_ = 0, crcl_ = 0;
};

}} // namespace
