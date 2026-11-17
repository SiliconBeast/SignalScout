// Unit tests (host). Run with `pio test -e native`.
#include <unity.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "proto.h"
#include "mesh.h"
#include "correlate.h"
#include "battery.h"
#include "geo.h"
#include "ubx.h"
#include "services.h"
#include "mocks.h"
using namespace ss;

void setUp() {}
void tearDown() {}

static FILE* open_vector(const char* name) {
  const char* roots[] = {"../tests/vectors/", "tests/vectors/", "../../tests/vectors/", "../../../tests/vectors/"};
  for (const char* r : roots) { std::string p = std::string(r) + name; FILE* f = fopen(p.c_str(), "r"); if (f) return f; }
  return nullptr;
}
static std::vector<uint8_t> unhex(const char* s) {
  std::vector<uint8_t> v; unsigned b;
  for (; s[0] && s[1] && sscanf(s, "%2x", &b) == 1; s += 2) v.push_back((uint8_t)b);
  return v;
}

// ------------------------------------------------------------ protocol framing
static void test_proto_crc_known() { TEST_ASSERT_EQUAL_HEX16(0x29B1, proto::crc16((const uint8_t*)"123456789", 9)); }
static void test_proto_roundtrip() {
  proto::RfSample s{123456, -5234, -5500, -4900, -6400}, o{};
  uint8_t p[16], f[80]; size_t n = proto::pack(s, p); size_t len = proto::encode(proto::T_RF_SAMPLE, 7, p, (uint8_t)n, f, sizeof f);
  TEST_ASSERT_EQUAL(n + proto::OVERHEAD, len);
  proto::Parser ps; bool got = false;
  for (size_t i = 0; i < len; i++) got = ps.feed(f[i]);
  TEST_ASSERT_TRUE(got);
  TEST_ASSERT_EQUAL(proto::T_RF_SAMPLE, ps.frame.type); TEST_ASSERT_EQUAL(7, ps.frame.seq);
  TEST_ASSERT_TRUE(proto::unpack(ps.frame.payload, ps.frame.len, o));
  TEST_ASSERT_EQUAL(123456, o.t_ms); TEST_ASSERT_EQUAL(-5234, o.avg_cdbm); TEST_ASSERT_EQUAL(-6400, o.floor_cdbm);
}
static void test_proto_bad_crc_and_resync() {
  uint8_t f[80]; size_t len = proto::encode(proto::T_PING, 1, nullptr, 0, f, sizeof f);
  proto::Parser ps; uint8_t bad[80]; memcpy(bad, f, len); bad[len - 1] ^= 0x55;
  bool got = false;
  for (size_t i = 0; i < len; i++) got |= ps.feed(bad[i]);
  TEST_ASSERT_FALSE(got); TEST_ASSERT_EQUAL(1, ps.crc_errors);
  const uint8_t junk[] = {0x00, 0xA5, 0x11, 0xA5, 0xA5}; // noise, including false starts
  for (uint8_t b : junk) TEST_ASSERT_FALSE(ps.feed(b));
  for (size_t i = 0; i < len; i++) got = ps.feed(f[i]);
  TEST_ASSERT_TRUE(got); TEST_ASSERT_EQUAL(proto::T_PING, ps.frame.type);
}
static void test_proto_rejects_oversize_and_version() {
  uint8_t f[200]; uint8_t big[100] = {0};
  TEST_ASSERT_EQUAL(0, proto::encode(1, 0, big, 65, f, sizeof f));
  TEST_ASSERT_EQUAL(0, proto::encode(1, 0, big, 10, f, 12));
  size_t len = proto::encode(proto::T_PONG, 3, nullptr, 0, f, sizeof f);
  f[2] = 9; uint16_t c = proto::crc16(f + 2, 4); f[6] = (uint8_t)c; f[7] = (uint8_t)(c >> 8); // valid CRC, wrong version
  proto::Parser ps; bool got = false;
  for (size_t i = 0; i < len; i++) got |= ps.feed(f[i]);
  TEST_ASSERT_FALSE(got); TEST_ASSERT_EQUAL(1, ps.ver_errors);
  const uint8_t over[] = {0xA5, 0x5A, 1, 1, 0, 200};
  for (uint8_t b : over) ps.feed(b);
  TEST_ASSERT_EQUAL(1, ps.len_errors);
}
static void test_proto_matches_python_vectors() {
  FILE* f = open_vector("proto.csv"); TEST_ASSERT_NOT_NULL_MESSAGE(f, "tests/vectors/proto.csv not found");
  char line[512]; int n = 0;
  while (fgets(line, sizeof line, f)) {
    if (line[0] == '#') continue;
    unsigned type, seq; char a[200] = "", b[300] = "";
    char* p1 = strchr(line, ','); char* p2 = p1 ? strchr(p1 + 1, ',') : nullptr; char* p3 = p2 ? strchr(p2 + 1, ',') : nullptr;
    TEST_ASSERT_NOT_NULL(p3);
    sscanf(line, "%u,%u", &type, &seq); *p3 = 0; snprintf(a, sizeof a, "%s", p2 + 1); sscanf(p3 + 1, "%299s", b);
    std::vector<uint8_t> payload = unhex(a), want = unhex(b); uint8_t out[100];
    size_t len = proto::encode((uint8_t)type, (uint8_t)seq, payload.data(), (uint8_t)payload.size(), out, sizeof out);
    TEST_ASSERT_EQUAL(want.size(), len); TEST_ASSERT_EQUAL_HEX8_ARRAY(want.data(), out, len); n++;
  }
  fclose(f); TEST_ASSERT_GREATER_THAN(3, n);
}

// ------------------------------------------------------------ mesh
static size_t make_text(mesh::Node& n, const char* t, uint8_t* raw, uint32_t now = 0, uint8_t ttl = mesh::DEFAULT_TTL) {
  n.originate(mesh::TEXT, (const uint8_t*)t, (uint8_t)strlen(t), now, mesh::BROADCAST, ttl);
  return n.next_tx(now, raw, mesh::MAX_PACKET);
}
static void test_mesh_encode_decode() {
  mesh::Packet p, q; p.type = mesh::TEXT; p.ttl = 3; p.src = 0x1234; p.id = 77; p.len = 5; memcpy(p.payload, "hello", 5);
  uint8_t raw[mesh::MAX_PACKET]; size_t n = mesh::encode(p, raw, sizeof raw);
  TEST_ASSERT_EQUAL(mesh::HEADER + 5 + mesh::CRC, n); TEST_ASSERT_TRUE(mesh::decode(raw, n, q));
  TEST_ASSERT_EQUAL_HEX16(0x1234, q.src); TEST_ASSERT_EQUAL(77, q.id); TEST_ASSERT_EQUAL(5, q.len);
  raw[mesh::HEADER] ^= 1; TEST_ASSERT_FALSE(mesh::decode(raw, n, q)); // payload corruption
  raw[mesh::HEADER] ^= 1; raw[0] = 0x54; TEST_ASSERT_FALSE(mesh::decode(raw, n, q)); // wrong magic
}
static void test_mesh_dedupe() {
  mesh::Node a(1, 5), b(2, 6); uint8_t raw[mesh::MAX_PACKET]; size_t n = make_text(a, "hi", raw);
  mesh::RxResult r1 = b.on_receive(raw, n, 100); TEST_ASSERT_TRUE(r1.valid && r1.deliver && !r1.duplicate);
  mesh::RxResult r2 = b.on_receive(raw, n, 200); TEST_ASSERT_TRUE(r2.duplicate); TEST_ASSERT_FALSE(r2.deliver);
  TEST_ASSERT_EQUAL(1, b.pending()); // forwarded once only
  mesh::RxResult r3 = a.on_receive(raw, n, 300); TEST_ASSERT_TRUE(r3.duplicate); // own packet echoed back
}
static void test_mesh_ttl_hops() {
  mesh::Node a(1, 5), b(2, 6), c(3, 7), d(4, 8); uint8_t raw[mesh::MAX_PACKET], fwd[mesh::MAX_PACKET];
  size_t n = make_text(a, "x", raw, 0, 2);
  mesh::RxResult rb = b.on_receive(raw, n, 0); TEST_ASSERT_TRUE(rb.forwarded);
  TEST_ASSERT_EQUAL(0, b.next_tx(0, fwd, sizeof fwd)); // relay waits for its jitter
  size_t fn = b.next_tx(5000, fwd, sizeof fwd); TEST_ASSERT_GREATER_THAN(0, fn);
  mesh::RxResult rc = c.on_receive(fwd, fn, 5000);
  TEST_ASSERT_TRUE(rc.deliver); TEST_ASSERT_EQUAL(1, rc.pkt.ttl); TEST_ASSERT_EQUAL(1, rc.pkt.hops);
  TEST_ASSERT_FALSE(rc.forwarded); TEST_ASSERT_EQUAL(0, c.pending()); // TTL exhausted: delivered, not relayed
  (void)d;
}
static void test_mesh_unicast_not_delivered_to_others() {
  mesh::Node a(1, 5), b(2, 6), c(3, 7); uint8_t raw[mesh::MAX_PACKET];
  a.originate(mesh::TEXT, (const uint8_t*)"p", 1, 0, 3); size_t n = a.next_tx(0, raw, sizeof raw);
  mesh::RxResult rb = b.on_receive(raw, n, 0); TEST_ASSERT_FALSE(rb.deliver); TEST_ASSERT_TRUE(rb.forwarded);
  mesh::RxResult rc = c.on_receive(raw, n, 0); TEST_ASSERT_TRUE(rc.deliver); TEST_ASSERT_FALSE(rc.forwarded);
}
static void test_mesh_sos_priority() {
  mesh::Node a(1, 5); uint8_t raw[mesh::MAX_PACKET]; uint8_t prio = 0; mesh::Packet p;
  a.originate(mesh::TEXT, (const uint8_t*)"first", 5, 0);
  mesh::Position pos{535268700, -1135292100, 80, 1}; uint8_t pb[10]; mesh::pack(pos, pb);
  a.originate(mesh::SOS, pb, 10, 10);
  size_t n = a.next_tx(20, raw, sizeof raw, &prio); TEST_ASSERT_TRUE(mesh::decode(raw, n, p));
  TEST_ASSERT_EQUAL(mesh::SOS, p.type); TEST_ASSERT_EQUAL(mesh::P_SOS, prio);
  mesh::Position q; TEST_ASSERT_TRUE(mesh::unpack(p.payload, p.len, q)); TEST_ASSERT_EQUAL(-1135292100, q.lon_e7);
  n = a.next_tx(20, raw, sizeof raw, &prio); TEST_ASSERT_TRUE(mesh::decode(raw, n, p)); TEST_ASSERT_EQUAL(mesh::TEXT, p.type);
}
static void test_mesh_queue_full() {
  mesh::Node a(1, 5); int ok = 0;
  for (int i = 0; i < 12; i++) ok += a.originate(mesh::TEXT, (const uint8_t*)"q", 1, 0) != 0;
  TEST_ASSERT_EQUAL(8, ok);
}
static void test_mesh_airtime_and_duty() {
  // SF9, 500 kHz, CR 4/5, 8 preamble, 30 bytes, explicit header, CRC on (Semtech AN1200.13):
  // Ts = 1.024 ms, payload = 8 + ceil(248 / 36) * 5 = 43 symbols, total 55.25 symbols = 56.58 ms.
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 56.58f, mesh::airtime_ms(30, 9, 500, 5, 8));
  // SF12 / 125 kHz / 20 bytes with low-data-rate optimisation = 1318.9 ms.
  TEST_ASSERT_FLOAT_WITHIN(2.0f, 1318.9f, mesh::airtime_ms(20, 12, 125, 5, 8));
  mesh::DutyLimiter d(0.10f, 1000);
  TEST_ASSERT_TRUE(d.allow(0, 600)); TEST_ASSERT_FALSE(d.allow(0, 600)); // bucket drained
  TEST_ASSERT_TRUE(d.allow(0, 600, true));                               // SOS may overdraw
  TEST_ASSERT_FALSE(d.allow(1000, 100));                                 // still in debt (-200 + 100)
  TEST_ASSERT_TRUE(d.allow(4000, 100));                                  // refilled at 10 %
}

// ------------------------------------------------------------ correlation
static void test_corr_sum() {
  float two[] = {-50, -50}; TEST_ASSERT_FLOAT_WITHIN(0.01f, -46.99f, corr::sum_dbm(two, 2));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -200.0f, corr::sum_dbm(nullptr, 0));
}
static void test_corr_states() {
  TEST_ASSERT_EQUAL(corr::QUIET, corr::classify(-63, -64, -200).state);
  TEST_ASSERT_EQUAL(corr::EXPLAINED, corr::classify(-50, -64, -48).state);
  corr::Result u = corr::classify(-31, -64, -48);
  TEST_ASSERT_EQUAL(corr::UNEXPLAINED, u.state); TEST_ASSERT_FLOAT_WITHIN(0.2f, 16.9f, u.excess_db); TEST_ASSERT_GREATER_THAN(50, u.confidence);
  TEST_ASSERT_EQUAL(corr::UNEXPLAINED, corr::classify(-40, -64, -200).state); // strong, nothing visible
}
static void test_corr_matches_python_vectors() {
  FILE* f = open_vector("correlate.csv"); TEST_ASSERT_NOT_NULL_MESSAGE(f, "tests/vectors/correlate.csv not found");
  char line[256]; int n = 0;
  while (fgets(line, sizeof line, f)) {
    if (line[0] == '#') continue;
    float m, fl, k, ex, exp_; int st, conf;
    TEST_ASSERT_EQUAL(7, sscanf(line, "%f,%f,%f,%d,%f,%f,%d", &m, &fl, &k, &st, &ex, &exp_, &conf));
    corr::Result r = corr::classify(m, fl, k);
    TEST_ASSERT_EQUAL_MESSAGE(st, r.state, line); TEST_ASSERT_FLOAT_WITHIN(0.01f, ex, r.excess_db);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, exp_, r.expected_dbm); TEST_ASSERT_INT_WITHIN(1, conf, r.confidence); n++;
  }
  fclose(f); TEST_ASSERT_EQUAL(90, n);
}
static void test_corr_floor_tracker() {
  corr::FloorTracker t(0.5f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -60, t.update(-60, 0.05f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -66, t.update(-66, 0.05f)); // drops at once
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -65.5f, t.update(-30, 1.0f)); // rises slowly through a burst
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -65.4f, t.update(-65.4f, 1.0f)); // never above the reading
}

// ------------------------------------------------------------ battery
static void test_battery_registers() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.8f, battery::vcell_volts(48640));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 82.5f, battery::soc_percent(0x5280));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, battery::soc_percent(0x6800)); // gauge can report > 100
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.4f, battery::crate_pct_per_hr((uint16_t)-50));
}
static void test_battery_curve_and_time() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0, battery::percent_from_voltage(3.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 100, battery::percent_from_voltage(4.25f));
  TEST_ASSERT_FLOAT_WITHIN(0.6f, 50, battery::percent_from_voltage(3.84f));
  float prev = -1; for (float v = 3.2f; v < 4.25f; v += 0.01f) { float p = battery::percent_from_voltage(v); TEST_ASSERT_TRUE(p >= prev); prev = p; }
  TEST_ASSERT_EQUAL(240, battery::minutes_remaining(80, -20));  // 80 % at 20 %/h
  TEST_ASSERT_EQUAL(30, battery::minutes_remaining(75, 50));    // charging: 25 % to go
  TEST_ASSERT_EQUAL(-1, battery::minutes_remaining(50, 0.1f));
}

// ------------------------------------------------------------ waypoint maths
static void test_geo_distance() {
  // Edmonton (53.5461, -113.4938) to Calgary (51.0447, -114.0719): 280.9 km great circle.
  TEST_ASSERT_FLOAT_WITHIN(800.0f, 280900.0f, (float)geo::distance_m(53.5461, -113.4938, 51.0447, -114.0719));
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 111.2f, (float)geo::distance_m(53.0, -113.0, 53.001, -113.0)); // 0.001 deg lat
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0, (float)geo::distance_m(10, 20, 10, 20));
}
static void test_geo_bearing() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0, (float)geo::bearing_deg(53, -113, 54, -113));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 180, (float)geo::bearing_deg(53, -113, 52, -113));
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 90, (float)geo::bearing_deg(0, 0, 0, 1));
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 270, (float)geo::bearing_deg(0, 0, 0, -1));
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 188.0f, (float)geo::bearing_deg(53.5461, -113.4938, 51.0447, -114.0719));
  TEST_ASSERT_EQUAL_STRING("N", geo::cardinal(359)); TEST_ASSERT_EQUAL_STRING("NE", geo::cardinal(45)); TEST_ASSERT_EQUAL_STRING("SW", geo::cardinal(225));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -20, (float)geo::relative_bearing(350, 10)); TEST_ASSERT_FLOAT_WITHIN(0.01f, 20, (float)geo::relative_bearing(10, 350));
}

// ------------------------------------------------------------ UBX
static void test_ubx_frame_checksum() {
  // UBX-CFG-RATE poll: B5 62 06 08 00 00 0E 30 (u-blox documentation example).
  uint8_t o[16]; size_t n = ubx::frame(0x06, 0x08, nullptr, 0, o, sizeof o);
  const uint8_t want[] = {0xB5, 0x62, 0x06, 0x08, 0x00, 0x00, 0x0E, 0x30};
  TEST_ASSERT_EQUAL(8, n); TEST_ASSERT_EQUAL_HEX8_ARRAY(want, o, 8);
}
static void test_ubx_valset_layout() {
  ubx::ValSet v; v.add(ubx::K_RATE_MEAS, 1000); v.add(ubx::K_SIG_GAL, 1);
  uint8_t o[64]; size_t n = v.finish(o, sizeof o);
  TEST_ASSERT_EQUAL(8 + 4 + (4 + 2) + (4 + 1), n); TEST_ASSERT_EQUAL_HEX8(0x8A, o[3]); TEST_ASSERT_EQUAL_HEX8(0x01, o[7]); // RAM layer
  const uint8_t key[] = {0x01, 0x00, 0x21, 0x30, 0xE8, 0x03}; TEST_ASSERT_EQUAL_HEX8_ARRAY(key, o + 10, 6);
}
static void test_ubx_nav_pvt() {
  mock::Clock clk; mock::Gnss g(clk); uint8_t cfg[128]; size_t cn = ubx::boot_config(false, cfg, sizeof cfg); g.write(cfg, (int)cn);
  clk.ms = 6000; uint8_t buf[128]; int n = g.read(buf, sizeof buf); TEST_ASSERT_EQUAL(100, n);
  ubx::Parser p; bool got = false; for (int i = 0; i < n; i++) got |= p.feed(buf[i]);
  TEST_ASSERT_TRUE(got); GnssFix f; TEST_ASSERT_TRUE(ubx::decode_nav_pvt(p.msg, f));
  TEST_ASSERT_TRUE(f.valid); TEST_ASSERT_EQUAL(14, f.sats); TEST_ASSERT_EQUAL(2026, f.year);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 53.5269f, (float)f.lat); TEST_ASSERT_FLOAT_WITHIN(0.001f, -113.5292f, (float)f.lon);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 668.4f, f.alt_m); TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.3f, f.hacc_m);
  buf[20] ^= 0xFF; ubx::Parser p2; got = false; for (int i = 0; i < n; i++) got |= p2.feed(buf[i]);
  TEST_ASSERT_FALSE(got); TEST_ASSERT_EQUAL(1, p2.ck_errors);
}

// ------------------------------------------------------------ services on mocks
struct Sys { mock::Rig rig; Bus bus; Services svc{bus, rig.hal};
  std::vector<Event> seen;
  void run(uint32_t ms) { for (uint32_t i = 0; i < ms; i += 10) { rig.clk.ms += 10; svc.tick_all(rig.clk.ms); Event e; while (bus.events.pop(e)) seen.push_back(e); } }
  std::vector<Event> drain() { std::vector<Event> v; v.swap(seen); return v; }
  int count(const std::vector<Event>& v, EvType t) { int n = 0; for (auto& e : v) n += e.type == t; return n; } };

static void test_svc_settings_text() {
  Settings s; s.brightness = 35; s.metric = false; s.lora_channel = 5; char buf[256]; settings_to_text(s, buf, sizeof buf);
  Settings o; settings_from_text(buf, o);
  TEST_ASSERT_EQUAL(35, o.brightness); TEST_ASSERT_FALSE(o.metric); TEST_ASSERT_EQUAL(5, o.lora_channel);
  settings_from_text("brightness=900\nlora_power=99\nbogus=1\n", o); TEST_ASSERT_EQUAL(100, o.brightness); TEST_ASSERT_EQUAL(22, o.lora_power);
}
static void test_svc_keys_and_sos_hold() {
  Sys s; s.run(100); s.drain();
  s.rig.buttons.mask = KEY_A; s.run(50); auto ev = s.drain();
  bool pressed = false; for (auto& e : ev) if (e.type == EV_KEY && (e.u.key.pressed & KEY_A)) pressed = true;
  TEST_ASSERT_TRUE(pressed);
  s.rig.buttons.mask = KEY_SOS; s.run(1500); TEST_ASSERT_FALSE(s.svc.shared.sos); // not yet
  s.run(700); TEST_ASSERT_TRUE(s.svc.shared.sos);
  ev = s.drain(); TEST_ASSERT_EQUAL(1, s.count(ev, EV_SOS));
  s.run(1000); TEST_ASSERT_TRUE(s.rig.siren.on);
  TEST_ASSERT_GREATER_THAN(0, (int)s.rig.lora.sent.size());
  mesh::Packet p; TEST_ASSERT_TRUE(mesh::decode(s.rig.lora.sent[0].data(), s.rig.lora.sent[0].size(), p)); TEST_ASSERT_EQUAL(mesh::SOS, p.type);
  s.rig.buttons.mask = 0; s.run(100); s.rig.buttons.mask = KEY_SOS; s.run(2200); // hold again to cancel
  TEST_ASSERT_FALSE(s.svc.shared.sos); TEST_ASSERT_FALSE(s.rig.siren.on);
}
static void test_svc_sweep_flow() {
  Sys s; s.bus.cmds.push(Cmd(CMD_SET_MODE, MODE_SWEEP)); s.run(14000); auto ev = s.drain();
  TEST_ASSERT_TRUE(s.rig.sled.woke); TEST_ASSERT_GREATER_THAN(100, s.count(ev, EV_RF));
  TEST_ASSERT_GREATER_THAN(0, s.count(ev, EV_WIFI_AP)); TEST_ASSERT_EQUAL(9, s.rig.sled.summary.n_wifi);
  int unexplained = 0, explained = 0; for (auto& e : ev) if (e.type == EV_RF) { unexplained += e.u.rf.state == corr::UNEXPLAINED; explained += e.u.rf.state == corr::EXPLAINED; }
  TEST_ASSERT_GREATER_THAN(20, unexplained); TEST_ASSERT_GREATER_THAN(20, explained);
  s.rig.sled.attached = false; s.rig.sled.out.clear(); s.run(3500); ev = s.drain();
  bool unlinked = false; for (auto& e : ev) if (e.type == EV_SLED && !e.u.sled.linked) unlinked = true;
  TEST_ASSERT_TRUE(unlinked);
}
static void test_svc_gnss_waypoint_storage() {
  Sys s; s.bus.cmds.push(Cmd(CMD_WP_SAVE)); s.run(500); auto ev = s.drain();
  bool nofix = false; for (auto& e : ev) if (e.type == EV_TOAST && strstr(e.u.toast, "No GPS")) nofix = true;
  TEST_ASSERT_TRUE(nofix);
  s.run(9000); TEST_ASSERT_EQUAL(1, s.svc.shared.fix);
  s.bus.cmds.push(Cmd(CMD_WP_SAVE)); s.run(200);
  TEST_ASSERT_TRUE(s.rig.storage.files.count("/waypoints.csv")); TEST_ASSERT_NOT_NULL(strstr(s.rig.storage.files["/waypoints.csv"].c_str(), "WP 1,53.52"));
  Cmd c(CMD_SET_SETTINGS); c.settings = s.svc.shared.settings; c.settings.brightness = 20; c.settings.lora_channel = 3; s.bus.cmds.push(c); s.run(200);
  TEST_ASSERT_EQUAL(20, s.rig.backlight.pct); TEST_ASSERT_FLOAT_WITHIN(0.01f, 912.0f, s.rig.lora.cfg.freq_mhz);
  TEST_ASSERT_NOT_NULL(strstr(s.rig.storage.files["/settings.txt"].c_str(), "brightness=20"));
  Sys s2; s2.rig.storage.files = s.rig.storage.files; s2.run(200); // "reboot": state comes back from the card
  TEST_ASSERT_EQUAL(20, s2.svc.shared.settings.brightness); ev = s2.drain(); TEST_ASSERT_EQUAL(1, s2.count(ev, EV_WP_ITEM));
}
static void test_svc_mesh_text_rx_tx() {
  Sys s; s.run(10000); auto ev = s.drain(); int rx = 0; for (auto& e : ev) if (e.type == EV_MSG && !e.u.msg.mine) rx++;
  TEST_ASSERT_EQUAL(1, rx); // scripted peer message
  size_t before = s.rig.lora.sent.size();
  Cmd c(CMD_SEND_TEXT); snprintf(c.text, sizeof c.text, "On my way"); s.bus.cmds.push(c); s.run(3000);
  TEST_ASSERT_GREATER_THAN(before, s.rig.lora.sent.size()); // own text + relay of the peer's
  bool found = false; for (auto& raw : s.rig.lora.sent) { mesh::Packet p; if (mesh::decode(raw.data(), raw.size(), p) && p.src == 0x51A7 && p.type == mesh::TEXT) found = true; }
  TEST_ASSERT_TRUE(found);
}
static void test_svc_sleep() {
  Sys s; s.run(200); s.bus.cmds.push(Cmd(CMD_SLEEP)); s.run(50);
  TEST_ASSERT_TRUE(s.rig.sys.slept); TEST_ASSERT_EQUAL(0, s.rig.backlight.pct);
  Sys t; t.run(200); t.bus.cmds.push(Cmd(CMD_SOS, 1)); t.run(50); t.bus.cmds.push(Cmd(CMD_SLEEP)); t.run(50);
  TEST_ASSERT_FALSE(t.rig.sys.slept); // SOS keeps the unit awake
}
static void test_svc_power_event() {
  Sys s; s.run(300); auto ev = s.drain(); bool ok = false;
  for (auto& e : ev) if (e.type == EV_POWER) { ok = true; TEST_ASSERT_FLOAT_WITHIN(0.1f, 82, e.u.power.pct); TEST_ASSERT_EQUAL(428, e.u.power.minutes); }
  TEST_ASSERT_TRUE(ok);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_proto_crc_known); RUN_TEST(test_proto_roundtrip); RUN_TEST(test_proto_bad_crc_and_resync);
  RUN_TEST(test_proto_rejects_oversize_and_version); RUN_TEST(test_proto_matches_python_vectors);
  RUN_TEST(test_mesh_encode_decode); RUN_TEST(test_mesh_dedupe); RUN_TEST(test_mesh_ttl_hops); RUN_TEST(test_mesh_unicast_not_delivered_to_others);
  RUN_TEST(test_mesh_sos_priority); RUN_TEST(test_mesh_queue_full); RUN_TEST(test_mesh_airtime_and_duty);
  RUN_TEST(test_corr_sum); RUN_TEST(test_corr_states); RUN_TEST(test_corr_matches_python_vectors); RUN_TEST(test_corr_floor_tracker);
  RUN_TEST(test_battery_registers); RUN_TEST(test_battery_curve_and_time);
  RUN_TEST(test_geo_distance); RUN_TEST(test_geo_bearing);
  RUN_TEST(test_ubx_frame_checksum); RUN_TEST(test_ubx_valset_layout); RUN_TEST(test_ubx_nav_pvt);
  RUN_TEST(test_svc_settings_text); RUN_TEST(test_svc_keys_and_sos_hold); RUN_TEST(test_svc_sweep_flow);
  RUN_TEST(test_svc_gnss_waypoint_storage); RUN_TEST(test_svc_mesh_text_rx_tx); RUN_TEST(test_svc_power_event); RUN_TEST(test_svc_sleep);
  return UNITY_END();
}
