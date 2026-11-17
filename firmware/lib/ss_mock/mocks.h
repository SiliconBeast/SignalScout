// Mock implementation of every HAL interface. Deterministic (seeded) so the
// simulator screenshots and the unit tests are repeatable.
#pragma once
#include <map>
#include <string>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "hal.h"
#include "proto.h"
#include "mesh.h"
#include "ubx.h"
namespace ss { namespace mock {

struct Rng { uint32_t s; explicit Rng(uint32_t seed = 12345) : s(seed) {}
  uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
  float uni() { return (next() & 0xFFFFFF) / 16777216.0f; }           // 0..1
  float noise(float amp) { return (uni() + uni() + uni() - 1.5f) * amp; } };

struct Clock { uint32_t ms = 0; };

struct Buttons : IButtons { uint16_t mask = 0; uint16_t read() override { return mask; } };
struct Led : ILed { uint8_t r = 0, g = 0, b = 0; void set(uint8_t r_, uint8_t g_, uint8_t b_) override { r = r_; g = g_; b = b_; } };
struct Backlight : IBacklight { uint8_t pct = 0; void set(uint8_t p) override { pct = p; } };
struct FuelGauge : IFuelGauge {
  bool ok = true; float volts = 3.92f, soc = 82.0f, rate = -11.5f;
  bool read(float& v, float& s, float& r) override { v = volts; s = soc; r = rate; return ok; } };
struct Charger : ICharger { bool chg = false, usb = false; bool charging() override { return chg; } bool usb_present() override { return usb; } };
struct Haptics : IHaptics { int plays = 0; uint8_t last = 0; void play(uint8_t e) override { plays++; last = e; } };
struct Siren : ISiren { bool on = false; uint16_t hz = 0; int changes = 0; void set(bool o, uint16_t h) override { on = o; hz = h; changes++; } };
struct Imu : IImu { bool read(float& x, float& y, float& z) override { x = 0; y = 0; z = 1; return true; } };
struct EnvSensor : IEnv { bool read(float& t, float& h, float& p) override { t = 21.4f; h = 38; p = 934.2f; return true; } };

struct Wifi : IWifiScanner {
  Clock& clk; Rng rng{77}; uint32_t t0 = 0; bool running = false; std::vector<WifiAp> aps;
  explicit Wifi(Clock& c) : clk(c) {
    add("UofA-Eduroam", 1, -48, AUTH_ENT, 0x10); add("ETLC-Lab-5G", 36, -55, AUTH_WPA3, 0x11); add("Shaw-4F2A", 6, -61, AUTH_WPA2, 0x12);
    add("TELUS1837", 11, -66, AUTH_WPA2, 0x13); add("HP-Print-B3", 6, -70, AUTH_OPEN, 0x14); add("CompEng-IoT", 1, -73, AUTH_WPA2, 0x15);
    add("Guest_WiFi", 11, -77, AUTH_OPEN, 0x16); add("", 149, -80, AUTH_WPA2, 0x17); add("eduroam", 44, -83, AUTH_ENT, 0x18);
  }
  void add(const char* ssid, uint8_t ch, int8_t rssi, uint8_t auth, uint8_t id) {
    WifiAp a; memset(&a, 0, sizeof a); snprintf(a.ssid, sizeof a.ssid, "%s", ssid); a.channel = ch; a.rssi = rssi; a.auth = auth;
    uint8_t m[6] = {0x3C, 0x84, 0x6A, 0x20, id, (uint8_t)(id * 7)}; memcpy(a.bssid, m, 6); aps.push_back(a);
  }
  void start() override { running = true; t0 = clk.ms; }
  int poll(WifiAp* out, int max) override {
    if (!running || clk.ms - t0 < 900) return -1;
    running = false; int n = 0;
    for (auto a : aps) { if (n >= max) break; a.rssi = (int8_t)(a.rssi + lroundf(rng.noise(5))); out[n++] = a; }
    return n;
  }
};
struct Ble : IBleScanner {
  Clock& clk; Rng rng{91}; uint32_t t0 = 0, dur = 0; bool running = false;
  explicit Ble(Clock& c) : clk(c) {}
  void start(uint16_t ms) override { running = true; t0 = clk.ms; dur = ms; }
  int poll(BleDev* out, int max) override {
    if (!running || clk.ms - t0 < dur) return -1;
    running = false;
    static const char* names[] = {"Suvir's AirPods", "Galaxy Watch6", "", "Tile Mate", "LE-Bose QC45", "", "Govee_H6159"};
    static const int8_t rssi[] = {-52, -64, -71, -76, -79, -84, -88};
    int n = 0;
    for (int i = 0; i < 7 && n < max; i++) {
      BleDev d; memset(&d, 0, sizeof d); snprintf(d.name, sizeof d.name, "%s", names[i]);
      uint8_t a[6] = {(uint8_t)(0xD0 + i), 0x5F, (uint8_t)(i * 37), 0x9A, (uint8_t)(0x11 * i), 0xC2}; memcpy(d.addr, a, 6);
      d.rssi = (int8_t)(rssi[i] + lroundf(rng.noise(6))); out[n++] = d;
    }
    return n;
  }
};
// Emits UBX NAV-PVT at 1 Hz once it has seen a configuration frame.
struct Gnss : IGnss {
  Clock& clk; std::vector<uint8_t> out; uint32_t t_last = 0, t_cfg = 0; bool configured = false; int cfg_frames = 0;
  double lat = 53.52687, lon = -113.52921; uint32_t ttff_ms = 4000; bool silent = false;
  explicit Gnss(Clock& c) : clk(c) {}
  void reset() override { configured = false; }
  void write(const uint8_t* b, int n) override { if (n > 4 && b[0] == 0xB5 && b[2] == ubx::CLS_CFG) { if (!configured) t_cfg = clk.ms; configured = true; cfg_frames++; } }
  int read(uint8_t* buf, int max) override {
    if (configured && !silent && clk.ms - t_last >= 1000) {
      t_last = clk.ms; uint8_t p[92]; memset(p, 0, sizeof p);
      bool fix = clk.ms - t_cfg >= ttff_ms; uint32_t sec = clk.ms / 1000;
      p[4] = 2026 & 0xFF; p[5] = 2026 >> 8; p[6] = 11; p[7] = 16; p[8] = 21; p[9] = (uint8_t)(14 + sec / 60 % 40); p[10] = (uint8_t)(sec % 60); p[11] = 0x07;
      p[20] = fix ? 3 : 0; p[21] = fix ? 1 : 0; p[23] = fix ? 14 : 4;
      lat += 0.0000012; lon += 0.0000021;
      put32(p + 24, (int32_t)llround(lon * 1e7)); put32(p + 28, (int32_t)llround(lat * 1e7));
      put32(p + 36, 668400); put32(p + 40, fix ? 2300 : 250000); put32(p + 60, 1250); put32(p + 64, 4850000);
      uint8_t f[100]; size_t n = ubx::frame(ubx::CLS_NAV, ubx::ID_NAV_PVT, p, 92, f, sizeof f); out.insert(out.end(), f, f + n);
    }
    int n = (int)out.size() < max ? (int)out.size() : max;
    if (n) { memcpy(buf, out.data(), (size_t)n); out.erase(out.begin(), out.begin() + n); }
    return n;
  }
  static void put32(uint8_t* p, int32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)((uint32_t)v >> (8 * i)); }
};
// Radio with a scripted peer two hops away.
struct LoRa : ILoRa {
  Clock& clk; bool fail = false, begun = false; LoraConfig cfg{}; std::vector<std::vector<uint8_t>> sent, rxq; uint32_t t_peer = 0; int peer_i = 0;
  bool peer = true; mesh::Node peer_node{0xB7C4, 99};
  explicit LoRa(Clock& c) : clk(c) {}
  bool begin(const LoraConfig& c) override { cfg = c; begun = !fail; return begun; }
  bool send(const uint8_t* b, size_t n) override { sent.emplace_back(b, b + n); return true; }
  void inject(const uint8_t* b, size_t n) { rxq.emplace_back(b, b + n); }
  int receive(uint8_t* buf, size_t max, float& rssi, float& snr) override {
    if (peer && clk.ms - t_peer >= 9000) {
      t_peer = clk.ms; static const char* msgs[] = {"At the trailhead, heading up", "Copy. Camp is at WP 2", "Weather turning, back by 5"};
      const char* m = msgs[peer_i++ % 3]; peer_node.originate(mesh::TEXT, (const uint8_t*)m, (uint8_t)strlen(m), clk.ms);
      uint8_t raw[mesh::MAX_PACKET]; size_t n = peer_node.next_tx(clk.ms, raw, sizeof raw); if (n) inject(raw, n);
    }
    if (rxq.empty()) return 0;
    std::vector<uint8_t> p = rxq.front(); rxq.erase(rxq.begin());
    size_t n = p.size() < max ? p.size() : max; memcpy(buf, p.data(), n); rssi = -97; snr = 6.5f; return (int)n;
  }
};
// Sled emulator: speaks the real protocol. Mostly ambient level with periodic
// strong bursts that the visible WiFi/BLE emitters cannot account for.
struct Sled : ISledLink {
  Clock& clk; Rng rng{5}; bool attached = true, streaming = false, send_verdicts = false, woke = false; uint8_t seq = 0;
  std::vector<uint8_t> out; proto::Parser parser; uint32_t t_s = 0, t_hello = 0; float floor = -64.0f; int frames_rx = 0; proto::ScanSummary summary{};
  explicit Sled(Clock& c) : clk(c) {}
  void wake(bool on) override { woke = on; }
  void write(const uint8_t* b, int n) override {
    if (!attached) return;
    for (int i = 0; i < n; i++) if (parser.feed(b[i])) {
      frames_rx++; const proto::Frame& f = parser.frame;
      if (f.type == proto::T_WAKE) streaming = true; else if (f.type == proto::T_SLEEP) streaming = false;
      else if (f.type == proto::T_PING) emit(proto::T_PONG, nullptr, 0);
      else if (f.type == proto::T_HELLO) { uint8_t p[4]; proto::Hello h{proto::ROLE_SLED, proto::VERSION, 0x0100}; emit(proto::T_HELLO, p, (uint8_t)proto::pack(h, p)); }
      else if (f.type == proto::T_SCAN_SUMMARY) proto::unpack(f.payload, f.len, summary);
    }
  }
  float level(uint32_t t) {
    float ambient = -52.0f + 4.0f * sinf(t / 2300.0f) + rng.noise(2.5f);
    uint32_t ph = t % 16000;
    if (ph > 9000 && ph < 13500) return -31.0f + rng.noise(2.0f); // unexplained burst
    return ambient;
  }
  int read(uint8_t* buf, int max) override {
    if (attached && streaming && clk.ms - t_s >= 50) {
      t_s = clk.ms; float v = level(clk.ms);
      proto::RfSample s; s.t_ms = clk.ms; s.avg_cdbm = (int16_t)(v * 100); s.min_cdbm = (int16_t)((v - 2.5f) * 100);
      s.max_cdbm = (int16_t)((v + 3.0f) * 100); s.floor_cdbm = (int16_t)(floor * 100);
      uint8_t p[12]; emit(proto::T_RF_SAMPLE, p, (uint8_t)proto::pack(s, p));
    }
    int n = (int)out.size() < max ? (int)out.size() : max;
    if (n) { memcpy(buf, out.data(), (size_t)n); out.erase(out.begin(), out.begin() + n); }
    return n;
  }
  void emit(uint8_t type, const uint8_t* p, uint8_t n) { uint8_t f[80]; size_t len = proto::encode(type, seq++, p, n, f, sizeof f); out.insert(out.end(), f, f + len); }
};
struct Storage : IStorage {
  bool present = true; std::map<std::string, std::string> files;
  bool mounted() override { return present; }
  bool append(const char* path, const char* line) override { if (!present) return false; files[path] += line; files[path] += "\n"; return true; }
  int read(const char* path, char* buf, int max) override {
    auto it = files.find(path); if (!present || it == files.end()) return -1;
    int n = (int)it->second.size() < max ? (int)it->second.size() : max; memcpy(buf, it->second.data(), (size_t)n); return n;
  }
  bool write(const char* path, const char* data) override { if (!present) return false; files[path] = data; return true; }
};
struct System : ISystem { bool slept = false; uint16_t node_id() override { return 0x51A7; } void deep_sleep() override { slept = true; } };

// Everything wired together.
struct Rig {
  Clock clk; Buttons buttons; Led led; Backlight backlight; FuelGauge gauge; Charger charger; Haptics haptics; Siren siren; Imu imu; EnvSensor env;
  Wifi wifi{clk}; Ble ble{clk}; Gnss gnss{clk}; LoRa lora{clk}; Sled sled{clk}; Storage storage; System sys;
  Hal hal{&buttons, &led, &backlight, &gauge, &charger, &haptics, &siren, &imu, &env, &wifi, &ble, &gnss, &lora, &sled, &storage, &sys};
};
}}
