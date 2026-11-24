// SPDX-License-Identifier: MIT
// Signal Scout LoRa mesh: managed flooding with TTL, de-duplication, SOS
// priority and a self-imposed airtime budget. Radio-independent (unit-tested).
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace ss { namespace mesh {

static const uint8_t MAGIC = 0x53, VERSION = 1;
static const uint16_t BROADCAST = 0xFFFF;
static const size_t HEADER = 10, CRC = 2, MAX_PAYLOAD = 96, MAX_PACKET = HEADER + MAX_PAYLOAD + CRC;
static const uint8_t DEFAULT_TTL = 3;

enum Type : uint8_t { TEXT = 1, POSITION = 2, SOS = 3, SOS_CLEAR = 4 };
enum Prio : uint8_t { P_NORMAL = 0, P_SOS = 1 };

struct Packet {
  uint8_t type = 0, ttl = 0, hops = 0;
  uint16_t src = 0, dst = BROADCAST, id = 0;
  uint8_t len = 0;
  uint8_t payload[MAX_PAYLOAD];
};
// Position payload (POSITION, SOS): lat/lon in 1e-7 deg, battery %, fix flag.
struct Position { int32_t lat_e7, lon_e7; uint8_t batt_pct; uint8_t fix; };

size_t encode(const Packet& p, uint8_t* out, size_t cap);       // 0 on error
bool decode(const uint8_t* in, size_t n, Packet& p);            // checks magic, version, length, CRC
size_t pack(const Position& p, uint8_t* out);
bool unpack(const uint8_t* in, size_t n, Position& p);

// LoRa time on air in ms (explicit header, CRC on, low-data-rate-optimise auto).
float airtime_ms(size_t bytes, int sf, float bw_khz, int cr_denom, int preamble = 8);

// Token-bucket airtime limiter. duty = 0.10 means 10 % of wall time.
class DutyLimiter {
 public:
  DutyLimiter(float duty = 0.10f, uint32_t burst_ms = 6000) : duty_(duty), burst_(burst_ms), tokens_((float)burst_ms) {}
  bool allow(uint32_t now_ms, float cost_ms, bool force = false); // force: SOS may overdraw
  float tokens() const { return tokens_; }
 private:
  float duty_; uint32_t burst_; float tokens_; uint32_t last_ = 0; bool init_ = false;
};

struct RxResult { bool valid = false, duplicate = false, deliver = false, forwarded = false; Packet pkt; };

class Node {
 public:
  explicit Node(uint16_t self_id, uint32_t seed = 1) : self_(self_id), rng_(seed ? seed : 1) {}
  uint16_t id() const { return self_; }
  // Originate a packet. Returns the message id, or 0 if the tx queue is full.
  uint16_t originate(Type t, const uint8_t* payload, uint8_t len, uint32_t now_ms, uint16_t dst = BROADCAST, uint8_t ttl = DEFAULT_TTL);
  // Handle a received raw packet: dedupe, decide deliver/forward, queue the rebroadcast.
  RxResult on_receive(const uint8_t* raw, size_t n, uint32_t now_ms);
  // Next raw packet due for transmission (SOS first), or 0 if none is due.
  size_t next_tx(uint32_t now_ms, uint8_t* out, size_t cap, uint8_t* prio = nullptr);
  size_t pending() const;
  bool seen(uint16_t src, uint16_t id) const;
 private:
  struct Tx { bool used = false; uint8_t prio = 0; uint32_t due = 0; uint8_t n = 0; uint8_t raw[MAX_PACKET]; };
  static const int NTX = 8, NSEEN = 64;
  bool enqueue(const Packet& p, uint8_t prio, uint32_t due);
  void remember(uint16_t src, uint16_t id);
  uint32_t rnd();
  uint16_t self_, next_id_ = 1; uint32_t rng_;
  Tx tx_[NTX];
  uint32_t seen_[NSEEN] = {0}; int seen_n_ = 0, seen_w_ = 0;
};
}}
