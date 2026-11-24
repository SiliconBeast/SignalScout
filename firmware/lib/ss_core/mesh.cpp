// SPDX-License-Identifier: MIT
#include "mesh.h"
#include "proto.h"
#include <string.h>
#include <math.h>
namespace ss { namespace mesh {

size_t encode(const Packet& p, uint8_t* o, size_t cap) {
  if (p.len > MAX_PAYLOAD || cap < HEADER + p.len + CRC) return 0;
  o[0] = MAGIC; o[1] = (uint8_t)((VERSION << 4) | (p.hops & 0x0F)); o[2] = p.type; o[3] = p.ttl;
  o[4] = (uint8_t)p.src; o[5] = (uint8_t)(p.src >> 8); o[6] = (uint8_t)p.dst; o[7] = (uint8_t)(p.dst >> 8);
  o[8] = (uint8_t)p.id; o[9] = (uint8_t)(p.id >> 8);
  memcpy(o + HEADER, p.payload, p.len);
  // hops and ttl change at every relay, so they are excluded from the CRC input by zeroing.
  uint8_t tmp[MAX_PACKET]; memcpy(tmp, o, HEADER + p.len); tmp[1] &= 0xF0; tmp[3] = 0;
  uint16_t c = proto::crc16(tmp, HEADER + p.len);
  o[HEADER + p.len] = (uint8_t)c; o[HEADER + p.len + 1] = (uint8_t)(c >> 8);
  return HEADER + p.len + CRC;
}
bool decode(const uint8_t* in, size_t n, Packet& p) {
  if (n < HEADER + CRC || n > MAX_PACKET || in[0] != MAGIC || (in[1] >> 4) != VERSION) return false;
  size_t len = n - HEADER - CRC;
  uint8_t tmp[MAX_PACKET]; memcpy(tmp, in, HEADER + len); tmp[1] &= 0xF0; tmp[3] = 0;
  uint16_t c = proto::crc16(tmp, HEADER + len);
  if (in[n - 2] != (uint8_t)c || in[n - 1] != (uint8_t)(c >> 8)) return false;
  p.hops = in[1] & 0x0F; p.type = in[2]; p.ttl = in[3];
  p.src = (uint16_t)(in[4] | (in[5] << 8)); p.dst = (uint16_t)(in[6] | (in[7] << 8)); p.id = (uint16_t)(in[8] | (in[9] << 8));
  p.len = (uint8_t)len; memcpy(p.payload, in + HEADER, len);
  return true;
}
size_t pack(const Position& p, uint8_t* o) {
  uint32_t a = (uint32_t)p.lat_e7, b = (uint32_t)p.lon_e7;
  for (int i = 0; i < 4; i++) { o[i] = (uint8_t)(a >> (8 * i)); o[4 + i] = (uint8_t)(b >> (8 * i)); }
  o[8] = p.batt_pct; o[9] = p.fix; return 10;
}
bool unpack(const uint8_t* in, size_t n, Position& p) {
  if (n < 10) return false;
  uint32_t a = 0, b = 0;
  for (int i = 0; i < 4; i++) { a |= (uint32_t)in[i] << (8 * i); b |= (uint32_t)in[4 + i] << (8 * i); }
  p.lat_e7 = (int32_t)a; p.lon_e7 = (int32_t)b; p.batt_pct = in[8]; p.fix = in[9]; return true;
}

float airtime_ms(size_t bytes, int sf, float bw_khz, int cr, int preamble) {
  float ts = (float)(1u << sf) / bw_khz; // symbol time, ms
  int de = ts > 16.0f ? 1 : 0;
  float num = 8.0f * bytes - 4.0f * sf + 28.0f + 16.0f; // CRC on, explicit header
  float n = ceilf(num / (4.0f * (sf - 2 * de)));
  if (n < 0) n = 0;
  float payload_sym = 8.0f + n * cr;
  return (preamble + 4.25f) * ts + payload_sym * ts;
}

bool DutyLimiter::allow(uint32_t now, float cost, bool force) {
  if (!init_) { init_ = true; last_ = now; }
  tokens_ += (float)(uint32_t)(now - last_) * duty_; last_ = now;
  if (tokens_ > (float)burst_) tokens_ = (float)burst_;
  if (tokens_ < cost && !force) return false;
  tokens_ -= cost;
  return true;
}

uint32_t Node::rnd() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return rng_; }
void Node::remember(uint16_t src, uint16_t id) {
  seen_[seen_w_] = ((uint32_t)src << 16) | id | 0; seen_w_ = (seen_w_ + 1) % NSEEN; if (seen_n_ < NSEEN) seen_n_++;
}
bool Node::seen(uint16_t src, uint16_t id) const {
  uint32_t k = ((uint32_t)src << 16) | id;
  for (int i = 0; i < seen_n_; i++) if (seen_[i] == k) return true;
  return false;
}
bool Node::enqueue(const Packet& p, uint8_t prio, uint32_t due) {
  for (int i = 0; i < NTX; i++) if (!tx_[i].used) {
    size_t n = encode(p, tx_[i].raw, sizeof tx_[i].raw);
    if (!n) return false;
    tx_[i].used = true; tx_[i].prio = prio; tx_[i].due = due; tx_[i].n = (uint8_t)n; return true;
  }
  return false;
}
uint16_t Node::originate(Type t, const uint8_t* payload, uint8_t len, uint32_t now, uint16_t dst, uint8_t ttl) {
  if (len > MAX_PAYLOAD) return 0;
  Packet p; p.type = t; p.ttl = ttl; p.hops = 0; p.src = self_; p.dst = dst; p.id = next_id_; p.len = len;
  if (len) memcpy(p.payload, payload, len);
  uint8_t prio = (t == SOS || t == SOS_CLEAR) ? P_SOS : P_NORMAL;
  if (!enqueue(p, prio, now)) return 0;
  remember(self_, p.id);
  uint16_t id = next_id_; next_id_ = (uint16_t)(next_id_ == 0xFFFF ? 1 : next_id_ + 1);
  return id;
}
RxResult Node::on_receive(const uint8_t* raw, size_t n, uint32_t now) {
  RxResult r;
  if (!decode(raw, n, r.pkt)) return r;
  r.valid = true;
  if (r.pkt.src == self_ || seen(r.pkt.src, r.pkt.id)) { r.duplicate = true; return r; }
  remember(r.pkt.src, r.pkt.id);
  r.deliver = (r.pkt.dst == BROADCAST || r.pkt.dst == self_);
  if (r.pkt.dst != self_ && r.pkt.ttl > 1) {
    Packet f = r.pkt; f.ttl--; f.hops = (uint8_t)((f.hops + 1) & 0x0F);
    bool sos = (f.type == SOS || f.type == SOS_CLEAR);
    uint32_t jitter = sos ? 50 + rnd() % 250 : 200 + rnd() % 1300; // spread relays to avoid collisions
    r.forwarded = enqueue(f, sos ? P_SOS : P_NORMAL, now + jitter);
  }
  return r;
}
size_t Node::next_tx(uint32_t now, uint8_t* out, size_t cap, uint8_t* prio) {
  int best = -1;
  for (int i = 0; i < NTX; i++) {
    if (!tx_[i].used || (int32_t)(now - tx_[i].due) < 0) continue;
    if (best < 0 || tx_[i].prio > tx_[best].prio || (tx_[i].prio == tx_[best].prio && (int32_t)(tx_[i].due - tx_[best].due) < 0)) best = i;
  }
  if (best < 0 || tx_[best].n > cap) return 0;
  memcpy(out, tx_[best].raw, tx_[best].n);
  if (prio) *prio = tx_[best].prio;
  tx_[best].used = false;
  return tx_[best].n;
}
size_t Node::pending() const { size_t c = 0; for (int i = 0; i < NTX; i++) c += tx_[i].used; return c; }
}}
