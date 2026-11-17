#include "services.h"
#include "battery.h"
#include "board.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
namespace ss {

static inline bool due(uint32_t now, uint32_t& t, uint32_t period) {
  if ((uint32_t)(now - t) < period && t != 0) return false;
  t = now ? now : 1; return true;
}

// ---------------------------------------------------------------- input
void InputSvc::tick(uint32_t now) {
  uint16_t raw = hal_.buttons->read();
  bool settled = (raw == last_raw_); last_raw_ = raw;
  if (!settled) return; // two equal reads 10 ms apart = debounced
  if (first_) { // report the lock switch position at boot
    first_ = false; stable_ = raw & KEY_LOCK;
    Event e(EV_KEY, now); e.u.key.keys = stable_; bus_.events.push(e);
  }
  if (raw != stable_) {
    Event e(EV_KEY, now);
    e.u.key.keys = raw; e.u.key.pressed = (uint16_t)(raw & ~stable_); e.u.key.released = (uint16_t)(stable_ & ~raw);
    stable_ = raw; bus_.events.push(e);
    if (e.u.key.pressed & KEY_SOS) { sos_since_ = now; sos_fired_ = false; }
  }
  // SOS works on any screen and with the lock switch on: hold 2 s to toggle.
  if ((stable_ & KEY_SOS) && !sos_fired_ && (uint32_t)(now - sos_since_) >= SOS_HOLD_MS) {
    sos_fired_ = true; bus_.cmds.push(Cmd(CMD_SOS, 2));
  }
}

// ---------------------------------------------------------------- housekeeping
void HouseSvc::tick(uint32_t now) {
  int16_t want_bl = sh_.brightness_override >= 0 ? sh_.brightness_override : (int16_t)sh_.settings.brightness;
  if (rev_ != sh_.settings_rev || !started_ || want_bl != bl_) { started_ = true; rev_ = sh_.settings_rev; bl_ = want_bl; hal_.backlight->set((uint8_t)want_bl); }
  if (due(now, t_power_, 2000)) {
    Event e(EV_POWER, now); Power& p = e.u.power; float rate = 0;
    p.gauge_ok = hal_.gauge->read(p.volts, p.pct, rate);
    p.charging = hal_.charger->charging(); p.usb = hal_.charger->usb_present();
    if (!p.gauge_ok) { p.pct = 0; p.minutes = -1; } else p.minutes = battery::minutes_remaining(p.pct, rate);
    sh_.batt_pct = (uint8_t)lroundf(p.pct);
    bus_.events.push(e);
  }
  if (due(now, t_env_, 5000)) {
    Event e(EV_ENV, now); e.u.env.ok = hal_.env->read(e.u.env.temp_c, e.u.env.rh, e.u.env.hpa); bus_.events.push(e);
    if (e.u.env.ok) { env_ = e.u.env; plog.add(now / 1000, wx::sea_level_hpa(env_.hpa, sh_.alt_m, env_.temp_c)); }
  }
  if (env_.ok && due(now, t_wx_, 60000)) publish_local_weather(now);
  if (sh_.mode == MODE_LEVEL && due(now, t_tilt_, 50)) { // bubble level, 20 Hz, low-passed
    float ax, ay, az;
    if (hal_.imu->read(ax, ay, az)) {
      float p = atan2f(ay, sqrtf(ax * ax + az * az)) * 57.29578f, r = atan2f(-ax, az) * 57.29578f;
      pitch_ += 0.35f * (p - pitch_); roll_ += 0.35f * (r - roll_);
      Event e(EV_TILT, now); e.u.tilt.pitch = pitch_; e.u.tilt.roll = roll_; bus_.events.push(e);
    }
  }
  if (due(now, t_led_, 250)) {
    bool blink = (now / 250) & 1;
    if (sh_.sos) hal_.led->set(blink ? 255 : 0, 0, 0);
    else if (sh_.mode == MODE_SWEEP && sh_.rf_state == corr::UNEXPLAINED) hal_.led->set(160, 0, 200);
    else if (hal_.charger->charging()) hal_.led->set(40, 16, 0);
    else hal_.led->set(0, 0, 0);
  }
}

void HouseSvc::publish_local_weather(uint32_t now) {
  Event e(EV_WEATHER, now); Weather& w = e.u.wx; memset(&w, 0, sizeof w);
  w.online = false; w.temp_c = env_.temp_c; w.rh = (uint8_t)env_.rh; w.hpa = wx::sea_level_hpa(env_.hpa, sh_.alt_m, env_.temp_c);
  float dp = 0; bool have = plog.trend(dp); w.dp3h = have ? dp : 0;
  wx::Local l = wx::local_forecast(w.hpa, w.dp3h, 11, true);
  w.cond = have ? l.cond : wx::UNKNOWN; w.trend = have ? l.trend : 0;
  snprintf(w.text, sizeof w.text, "%s", have ? l.text : "Learning the pressure trend");
  bus_.events.push(e);
}

// ---------------------------------------------------------------- network
int NetSvc::load_networks(const char* t, char ssid[][33], char pass[][64], int max) {
  int n = 0;
  while (t && *t && n < max) {
    const char* nl = strchr(t, '\n'); size_t len = nl ? (size_t)(nl - t) : strlen(t);
    const char* tab = (const char*)memchr(t, '\t', len);
    if (tab && tab > t && *t != '#') {
      size_t a = (size_t)(tab - t), b = len - a - 1; if (b && t[a + b] == '\r') b--;
      if (a < 33 && b < 64) { memcpy(ssid[n], t, a); ssid[n][a] = 0; memcpy(pass[n], tab + 1, b); pass[n][b] = 0; n++; }
    }
    t = nl ? nl + 1 : nullptr;
  }
  return n;
}
void NetSvc::publish(uint32_t now) {
  Event e(EV_NET, now); NetStatus& s = e.u.net;
  s.state = state_; s.ap = ap_; s.ap_clients = ap_ ? hal_.net->ap_clients() : 0;
  s.rssi = state_ == NET_ONLINE ? hal_.net->sta_rssi() : 0;
  if (state_ == NET_ONLINE || state_ == NET_CONNECTING) snprintf(s.ssid, sizeof s.ssid, "%s", ssid_[try_ % (n_nets_ ? n_nets_ : 1)]);
  if (state_ == NET_ONLINE) hal_.net->ip(s.ip, sizeof s.ip);
  snprintf(s.ap_ssid, sizeof s.ap_ssid, "%s", ap_ssid_); snprintf(s.ap_pass, sizeof s.ap_pass, "%s", ap_pass_);
  bus_.events.push(e); last_pub_ = state_;
}
void NetSvc::ota_status(uint32_t now, uint8_t st, uint8_t prog) {
  ota_.state = st; ota_.progress = prog;
  Event e(EV_OTA, now); e.u.ota = ota_; bus_.events.push(e);
}
void NetSvc::fetch_weather(uint32_t now) {
  if (!sh_.have_pos) return; // retried on the next period; the UI says a fix is needed
  char url[384]; wx::open_meteo_url(url, sizeof url, sh_.last_lat_e7 / 1e7, sh_.last_lon_e7 / 1e7);
  int n = hal_.net->http_get(url, buf_, sizeof buf_ - 1); wx_due_ = false; t_wx_ = now;
  if (n <= 0) return;
  buf_[n] = 0; Event e(EV_WEATHER, now); int32_t off = 0;
  if (!wx::parse_open_meteo(buf_, e.u.wx, off)) return;
  bus_.events.push(e);
  if (off / 60 != sh_.settings.tz_min) { Event t(EV_TIME, now); t.u.time.utc = 0; t.u.time.tz_min = (int16_t)(off / 60); t.u.time.source = 0; bus_.events.push(t); }
}
void NetSvc::check_update(uint32_t now) {
  ota_due_ = false; t_ota_ = now; ota_status(now, OTA_CHECKING);
  int n = hal_.net->http_get(ota::MANIFEST_URL, buf_, sizeof buf_ - 1);
  if (n <= 0) { ota_status(now, OTA_ERROR); return; }
  buf_[n] = 0;
  if (!ota::parse_manifest(buf_, man_)) { ota_status(now, OTA_ERROR); return; }
  snprintf(ota_.version, sizeof ota_.version, "%s", man_.version); snprintf(ota_.notes, sizeof ota_.notes, "%.59s", man_.notes); ota_.size = man_.size;
  ota_status(now, ota::should_offer(man_, SS_FW_VERSION) ? OTA_AVAILABLE : OTA_UP_TO_DATE);
}
void NetSvc::tick(uint32_t now) {
  Cmd c;
  while (inbox.pop(c)) {
    if (c.type == CMD_NET) {
      want_ = c.a != 0;
      if (want_) {
        char txt[1024]; int n = hal_.storage->read("/wifi.txt", txt, sizeof txt - 1); txt[n > 0 ? n : 0] = 0;
        n_nets_ = load_networks(txt, ssid_, pass_, 4); try_ = 0; state_ = n_nets_ ? NET_SCANNING : NET_NO_NETWORK; t_retry_ = now;
      } else { hal_.net->sta_end(); state_ = NET_OFF; synced_ = false; }
      publish(now);
    } else if (c.type == CMD_HOTSPOT) {
      if (c.a && !ap_) {
        uint16_t id = hal_.sys->node_id(); snprintf(ap_ssid_, sizeof ap_ssid_, "SignalScout-%04X", id);
        uint32_t k = (uint32_t)id * 2654435761u; snprintf(ap_pass_, sizeof ap_pass_, "ss%08lx", (unsigned long)(k ^ 0x5C5C5C5Cu));
        ap_ = hal_.net->ap_begin(ap_ssid_, ap_pass_);
        if (!ap_) { Event t(EV_TOAST, now); snprintf(t.u.toast, sizeof t.u.toast, "Hotspot failed to start"); bus_.events.push(t); }
      } else if (!c.a && ap_) { hal_.net->ap_end(); ap_ = false; }
      publish(now);
    } else if (c.type == CMD_OTA) {
      if (c.a == 0) { if (state_ == NET_ONLINE) check_update(now); else ota_status(now, OTA_ERROR); }
      else if (c.a == 1 && ota_.state == OTA_AVAILABLE) {
        if (sh_.sos) continue;
        installing_ = hal_.net->ota_begin(man_.url, man_.size, man_.sha256); ota_status(now, installing_ ? OTA_DOWNLOADING : OTA_ERROR);
      } else if (c.a == 2 && (ota_.state == OTA_READY || hal_.net->ota_poll() > 100) && !sh_.sos) hal_.net->restart(); // also after a web-console upload
    } else if (c.type == CMD_WX_REFRESH) { wx_due_ = true; }
    else if (c.type == CMD_WIFI_ADD) {
      char line[100]; snprintf(line, sizeof line, "%s", c.text); hal_.storage->append("/wifi.txt", line);
    }
  }
  // station state machine: try each saved network in turn
  if (want_ && n_nets_) {
    uint8_t hw = hal_.net->sta_state();
    if (state_ == NET_SCANNING || ((state_ == NET_NO_NETWORK || state_ == NET_FAILED) && (int32_t)(now - t_retry_) >= 0)) {
      hal_.net->sta_begin(ssid_[try_ % n_nets_], pass_[try_ % n_nets_]); state_ = NET_CONNECTING; t_join_ = now;
    } else if (state_ == NET_CONNECTING) {
      if (hw == NET_ONLINE) { state_ = NET_ONLINE; wx_due_ = true; ota_due_ = sh_.settings.auto_update; }
      else if (hw == NET_FAILED || (uint32_t)(now - t_join_) > JOIN_TIMEOUT_MS) {
        hal_.net->sta_end(); try_++;
        if (try_ % n_nets_ == 0) { state_ = NET_NO_NETWORK; t_retry_ = now + RETRY_MS; } else state_ = NET_SCANNING;
      }
    } else if (state_ == NET_ONLINE && hw != NET_ONLINE) { state_ = NET_SCANNING; synced_ = false; }
  }
  if (state_ == NET_ONLINE) {
    uint32_t utc;
    if (!synced_ && hal_.net->ntp(utc)) { synced_ = true; Event t(EV_TIME, now); t.u.time.utc = utc; t.u.time.tz_min = sh_.settings.tz_min; t.u.time.source = 2; bus_.events.push(t); }
    if (wx_due_ || (uint32_t)(now - t_wx_) >= WX_PERIOD_MS) fetch_weather(now);
    if (sh_.settings.auto_update && (ota_due_ || (uint32_t)(now - t_ota_) >= OTA_PERIOD_MS) && !installing_) check_update(now);
  }
  if (installing_) {
    int p = hal_.net->ota_poll();
    if (p < 0) { installing_ = false; ota_status(now, OTA_ERROR); }
    else if (p > 100) { installing_ = false; ota_status(now, OTA_READY, 100); }
    else if (p != ota_.progress) ota_status(now, p >= 100 ? OTA_VERIFYING : OTA_DOWNLOADING, (uint8_t)p);
  }
  if (state_ != last_pub_ || due(now, t_pub_, 5000)) publish(now);
}

// ---------------------------------------------------------------- wifi / ble scan
void ScanSvc::tick(uint32_t now) {
  uint8_t mode = sh_.mode;
  bool want = (mode == MODE_FIELD || mode == MODE_SWEEP);
  switch (st_) {
    case IDLE:
      if (want && (int32_t)(now - next_) >= 0) { hal_.wifi->start(); st_ = WIFI; }
      break;
    case WIFI: {
      int n = hal_.wifi->poll(aps_, MAX_AP);
      if (n < 0) break;
      bus_.events.push(Event(EV_WIFI_BEGIN, now));
      double sum = 0; float best = -200;
      for (int i = 0; i < n; i++) {
        Event e(EV_WIFI_AP, now); e.u.ap = aps_[i]; bus_.events.push(e);
        sum += corr::dbm_to_mw(aps_[i].rssi); if (aps_[i].rssi > best) best = aps_[i].rssi;
      }
      bus_.events.push(Event(EV_WIFI_END, now));
      wifi_sum_mw_ = (float)sum; sh_.n_wifi = (uint8_t)n; sh_.strongest_wifi = best;
      hal_.ble->start(2500); st_ = BLE;
      break;
    }
    case BLE: {
      int n = hal_.ble->poll(bles_, MAX_BLE);
      if (n < 0) break;
      bus_.events.push(Event(EV_BLE_BEGIN, now));
      double sum = wifi_sum_mw_; float best = -200;
      for (int i = 0; i < n; i++) {
        Event e(EV_BLE_DEV, now); e.u.ble = bles_[i]; bus_.events.push(e);
        sum += corr::dbm_to_mw(bles_[i].rssi); if (bles_[i].rssi > best) best = bles_[i].rssi;
      }
      bus_.events.push(Event(EV_BLE_END, now));
      sh_.n_ble = (uint8_t)n; sh_.strongest_ble = best; sh_.known_sum_dbm = (float)corr::mw_to_dbm(sum);
      st_ = IDLE; next_ = now + (mode == MODE_FIELD ? 1500 : 6000);
      break;
    }
  }
}

// ---------------------------------------------------------------- sweep (sled link)
void SweepSvc::send(uint8_t type, const uint8_t* p, uint8_t n) {
  uint8_t buf[proto::MAX_PAYLOAD + proto::OVERHEAD];
  size_t len = proto::encode(type, seq_++, p, n, buf, sizeof buf);
  if (len) hal_.sled->write(buf, (int)len);
}
void SweepSvc::tick(uint32_t now) {
  bool want = (sh_.mode == MODE_SWEEP);
  if (want != streaming_) {
    streaming_ = want; hal_.sled->wake(want);
    send(want ? proto::T_WAKE : proto::T_SLEEP, nullptr, 0);
    if (want) { uint8_t p[4]; proto::Hello h{proto::ROLE_BASE, proto::VERSION, 0x0100}; send(proto::T_HELLO, p, (uint8_t)proto::pack(h, p)); }
  }
  uint8_t buf[128]; int n;
  while ((n = hal_.sled->read(buf, sizeof buf)) > 0) {
    for (int i = 0; i < n; i++) {
      if (!parser_.feed(buf[i])) continue;
      const proto::Frame& f = parser_.frame; last_rx_ = now;
      if (!linked_) { linked_ = true; Event e(EV_SLED, now); e.u.sled.linked = true; e.u.sled.fw = sled_fw_; bus_.events.push(e); }
      if (f.type == proto::T_HELLO) {
        proto::Hello h; if (proto::unpack(f.payload, f.len, h)) { sled_fw_ = h.fw_ver; Event e(EV_SLED, now); e.u.sled.linked = true; e.u.sled.fw = sled_fw_; bus_.events.push(e); }
      } else if (f.type == proto::T_VERDICT) {
        if (proto::unpack(f.payload, f.len, verdict_)) { have_verdict_ = true; last_verdict_ = now; }
      } else if (f.type == proto::T_RF_SAMPLE) {
        proto::RfSample s; if (!proto::unpack(f.payload, f.len, s)) continue;
        Event e(EV_RF, now); RfReading& r = e.u.rf;
        r.dbm = s.avg_cdbm / 100.0f; r.min_dbm = s.min_cdbm / 100.0f; r.max_dbm = s.max_cdbm / 100.0f; r.floor_dbm = s.floor_cdbm / 100.0f;
        if (have_verdict_ && (uint32_t)(now - last_verdict_) < 3000) { // Linux side is alive: use its verdict
          r.state = verdict_.state; r.confidence = verdict_.confidence; r.excess_db = verdict_.excess_cdb / 100.0f;
          r.expected_dbm = verdict_.expected_cdbm / 100.0f; r.from_sled_verdict = true;
        } else { // fallback: same algorithm, run locally on the base's own scan
          corr::Result c = corr::classify(r.dbm, r.floor_dbm, sh_.known_sum_dbm);
          r.state = c.state; r.confidence = c.confidence; r.excess_db = c.excess_db; r.expected_dbm = c.expected_dbm;
        }
        sh_.rf_state = r.state;
        if (streaming_) bus_.events.push(e);
        if (sh_.settings.logging && due(now, t_log_, 1000)) {
          LogLine l; snprintf(l.path, sizeof l.path, "/sweep.csv");
          snprintf(l.line, sizeof l.line, "%lu,%.1f,%.1f,%.1f,%.1f,%u,%.1f,%u,%u", (unsigned long)now, r.dbm, r.min_dbm, r.max_dbm, r.floor_dbm,
                   r.state, r.excess_db, (unsigned)sh_.n_wifi, (unsigned)sh_.n_ble);
          log_.push(l);
        }
      }
    }
  }
  if (linked_ && (uint32_t)(now - last_rx_) > 2500) {
    linked_ = false; have_verdict_ = false; sh_.rf_state = 0;
    Event e(EV_SLED, now); e.u.sled.linked = false; bus_.events.push(e);
  }
  if (due(now, t_ping_, 1000) && !streaming_) send(proto::T_PING, nullptr, 0);
  if (streaming_ && due(now, t_summary_, 2000)) {
    proto::ScanSummary s; s.n_wifi = sh_.n_wifi; s.n_ble = sh_.n_ble;
    s.strongest_wifi_cdbm = (int16_t)(sh_.strongest_wifi * 100); s.strongest_ble_cdbm = (int16_t)(sh_.strongest_ble * 100);
    s.sum_cdbm = (int16_t)(sh_.known_sum_dbm * 100);
    uint8_t p[8]; send(proto::T_SCAN_SUMMARY, p, (uint8_t)proto::pack(s, p));
  }
}

// ---------------------------------------------------------------- gnss
void GnssSvc::configure() {
  uint8_t buf[128]; psm_ = sh_.settings.gnss_psm;
  size_t n = ubx::boot_config(psm_, buf, sizeof buf);
  hal_.gnss->write(buf, (int)n); cfg_sent_ = true;
}
void GnssSvc::tick(uint32_t now) {
  // (Re)send the configuration until NAV-PVT shows up, and whenever power-save changes.
  if ((!got_pvt_ && due(now, t_cfg_, 2000)) || (cfg_sent_ && psm_ != sh_.settings.gnss_psm)) configure();
  uint8_t buf[128]; int n;
  while ((n = hal_.gnss->read(buf, sizeof buf)) > 0)
    for (int i = 0; i < n; i++) {
      if (!parser_.feed(buf[i])) continue;
      Event e(EV_GNSS, now);
      if (!ubx::decode_nav_pvt(parser_.msg, e.u.gnss)) continue;
      got_pvt_ = true; last_pvt_ = now;
      const GnssFix& f = e.u.gnss;
      sh_.fix = f.valid ? 1 : 0;
      if (f.valid) { sh_.lat_e7 = (int32_t)llround(f.lat * 1e7); sh_.lon_e7 = (int32_t)llround(f.lon * 1e7); sh_.last_lat_e7 = sh_.lat_e7; sh_.last_lon_e7 = sh_.lon_e7; sh_.alt_m = f.alt_m; sh_.have_pos = true; }
      bus_.events.push(e);
      if (sh_.settings.logging && f.valid && due(now, t_log_, 5000)) {
        LogLine l; snprintf(l.path, sizeof l.path, "/track.csv");
        snprintf(l.line, sizeof l.line, "%04u-%02u-%02uT%02u:%02u:%02uZ,%.7f,%.7f,%.1f,%u,%.1f", f.year, f.month, f.day, f.hour, f.minute, f.second,
                 f.lat, f.lon, f.alt_m, f.sats, f.hacc_m);
        log_.push(l);
      }
    }
  if (got_pvt_ && (uint32_t)(now - last_pvt_) > 5000) { // receiver went silent
    got_pvt_ = false; sh_.fix = 0; Event e(EV_GNSS, now); bus_.events.push(e);
  }
}

// ---------------------------------------------------------------- mesh
MeshSvc::MeshSvc(Bus& b, Hal& h, Shared& s, SsQueue<LogLine>& log)
    : bus_(b), hal_(h), sh_(s), log_(log), node_(h.sys->node_id(), (uint32_t)h.sys->node_id() * 2654435761u) {}
void MeshSvc::start_radio() {
  const Settings& st = sh_.settings; rev_ = sh_.settings_rev; started_ = true;
  LoraConfig c; c.freq_mhz = lora::CHANNELS[st.lora_channel % lora::N_CHANNELS]; c.bw_khz = lora::BW_KHZ;
  c.sf = (uint8_t)lora::SF; c.cr = (uint8_t)lora::CR; c.sync = lora::SYNC; c.preamble = (uint8_t)lora::PREAMBLE;
  c.power_dbm = (int8_t)(st.lora_power > 22 ? 22 : st.lora_power);
  radio_ok_ = hal_.lora->begin(c);
}
void MeshSvc::status(uint32_t now) {
  Event e(EV_LORA, now); LoraStatus& l = e.u.lora;
  l.ok = radio_ok_; l.channel = sh_.settings.lora_channel; l.node_id = node_.id(); l.tx_count = tx_; l.rx_count = rx_;
  float t = duty_.tokens() / 60.0f; l.budget_pct = (uint8_t)(t > 100 ? 100 : (t < 0 ? 0 : t));
  bus_.events.push(e);
}
void MeshSvc::sos_packet(uint32_t now, bool clear) {
  mesh::Position p; p.lat_e7 = sh_.lat_e7; p.lon_e7 = sh_.lon_e7; p.batt_pct = sh_.batt_pct; p.fix = sh_.fix;
  uint8_t buf[10]; size_t n = mesh::pack(p, buf);
  node_.originate(clear ? mesh::SOS_CLEAR : mesh::SOS, buf, (uint8_t)n, now, mesh::BROADCAST, 5);
}
void MeshSvc::deliver(const mesh::RxResult& r, float rssi, float snr, uint32_t now) {
  const mesh::Packet& p = r.pkt;
  Event e(EV_MSG, now); MeshText& m = e.u.msg;
  m.src = p.src; m.rssi = rssi; m.snr = snr; m.hops = p.hops; m.t_ms = now;
  if (p.type == mesh::TEXT) {
    size_t n = p.len < sizeof m.text - 1 ? p.len : sizeof m.text - 1; memcpy(m.text, p.payload, n); m.text[n] = 0;
  } else if (p.type == mesh::SOS || p.type == mesh::SOS_CLEAR || p.type == mesh::POSITION) {
    mesh::Position pos; if (!mesh::unpack(p.payload, p.len, pos)) return;
    m.sos = (p.type == mesh::SOS); m.has_pos = pos.fix != 0; m.lat = pos.lat_e7 * 1e-7; m.lon = pos.lon_e7 * 1e-7; m.batt = pos.batt_pct;
    snprintf(m.text, sizeof m.text, "%s", p.type == mesh::SOS ? "SOS" : (p.type == mesh::SOS_CLEAR ? "SOS cleared" : "Position"));
    if (p.type == mesh::SOS && sh_.settings.haptics) hal_.haptics->play(HAP_ALERT);
  } else return;
  bus_.events.push(e);
  LogLine l; snprintf(l.path, sizeof l.path, "/mesh.csv");
  snprintf(l.line, sizeof l.line, "%lu,rx,%04X,%u,%.0f,%.1f,%s", (unsigned long)now, p.src, p.hops, rssi, snr, m.text);
  if (sh_.settings.logging) log_.push(l);
}
void MeshSvc::tick(uint32_t now) {
  if (!started_ || rev_ != sh_.settings_rev) { start_radio(); status(now); }
  Cmd c;
  while (inbox.pop(c)) {
    if (c.type == CMD_SEND_TEXT) {
      uint8_t n = (uint8_t)strnlen(c.text, sizeof c.text - 1);
      if (node_.originate(mesh::TEXT, (const uint8_t*)c.text, n, now)) {
        Event e(EV_MSG, now); MeshText& m = e.u.msg; m.src = node_.id(); m.mine = true; m.t_ms = now;
        snprintf(m.text, sizeof m.text, "%s", c.text); bus_.events.push(e);
      } else { Event e(EV_TOAST, now); snprintf(e.u.toast, sizeof e.u.toast, "Send queue full"); bus_.events.push(e); }
    } else if (c.type == CMD_SOS) {
      bool on = c.a == 2 ? !sh_.sos : (c.a != 0);
      if (on != sh_.sos) {
        sh_.sos = on; t_sos_ = now;
        sos_packet(now, !on);
        if (sh_.settings.haptics) hal_.haptics->play(HAP_ALERT);
        if (!on) { hal_.siren->set(false, 0); siren_on_ = false; }
        Event e(EV_SOS, now); e.u.sos.active = on; bus_.events.push(e);
      }
    }
  }
  if (sh_.sos) {
    if ((uint32_t)(now - t_sos_) >= SOS_PERIOD_MS) { t_sos_ = now; sos_packet(now, false); }
    if (due(now, t_siren_, 400)) { // two-tone siren, only if enabled
      siren_on_ = !siren_on_;
      if (sh_.settings.siren) hal_.siren->set(true, siren_on_ ? 3100 : 2600); else hal_.siren->set(false, 0);
    }
  }
  if (radio_ok_) {
    uint8_t buf[mesh::MAX_PACKET]; float rssi = 0, snr = 0;
    int n = hal_.lora->receive(buf, sizeof buf, rssi, snr);
    if (n > 0) {
      mesh::RxResult r = node_.on_receive(buf, (size_t)n, now);
      if (r.valid && !r.duplicate) { rx_++; if (r.deliver) deliver(r, rssi, snr, now); }
    }
    uint8_t prio = 0; size_t tn = node_.next_tx(now, buf, sizeof buf, &prio);
    if (tn) {
      float cost = mesh::airtime_ms(tn, lora::SF, lora::BW_KHZ, lora::CR, lora::PREAMBLE);
      // SOS traffic may overdraw the airtime budget; everything else waits its turn.
      if (duty_.allow(now, cost, prio == mesh::P_SOS)) { if (hal_.lora->send(buf, tn)) tx_++; }
      else { Event e(EV_TOAST, now); snprintf(e.u.toast, sizeof e.u.toast, "Airtime limit - message dropped"); bus_.events.push(e); }
    }
  }
  if (due(now, t_status_, 3000)) status(now);
}

// ---------------------------------------------------------------- storage
int settings_to_text(const Settings& s, char* out, int cap) {
  return snprintf(out, (size_t)cap, "brightness=%u\nhaptics=%u\nsiren=%u\nmetric=%u\ngnss_psm=%u\nlogging=%u\nlora_channel=%u\nlora_power=%u\n"
                  "profile=%u\nsetup_done=%u\naod=%u\naod_cx0=%u\naod_cx1=%u\naod_cx2=%u\nscreen_off_s=%u\nwifi=%u\nauto_update=%u\ntz_min=%d\nwp_target=%u\n",
                  s.brightness, s.haptics, s.siren, s.metric, s.gnss_psm, s.logging, s.lora_channel, s.lora_power,
                  s.profile, s.setup_done, s.aod, s.aod_cx[0], s.aod_cx[1], s.aod_cx[2], s.screen_off_s, s.wifi, s.auto_update, s.tz_min, s.wp_target);
}
void settings_from_text(const char* t, Settings& s) {
  while (t && *t) {
    char key[20]; int val = 0;
    if (sscanf(t, "%19[^=]=%d", key, &val) == 2) {
      if (!strcmp(key, "brightness")) s.brightness = (uint8_t)(val < 5 ? 5 : (val > 100 ? 100 : val));
      else if (!strcmp(key, "haptics")) s.haptics = val != 0;
      else if (!strcmp(key, "siren")) s.siren = val != 0;
      else if (!strcmp(key, "metric")) s.metric = val != 0;
      else if (!strcmp(key, "gnss_psm")) s.gnss_psm = val != 0;
      else if (!strcmp(key, "logging")) s.logging = val != 0;
      else if (!strcmp(key, "lora_channel")) s.lora_channel = (uint8_t)(val < 0 ? 0 : val % lora::N_CHANNELS);
      else if (!strcmp(key, "lora_power")) s.lora_power = (uint8_t)(val < 2 ? 2 : (val > 22 ? 22 : val));
      else if (!strcmp(key, "profile")) s.profile = (uint8_t)(val ? PROFILE_ADVENTURE : PROFILE_HACKER);
      else if (!strcmp(key, "setup_done")) s.setup_done = val != 0;
      else if (!strcmp(key, "aod")) s.aod = val != 0;
      else if (!strncmp(key, "aod_cx", 6) && key[6] >= '0' && key[6] <= '2' && !key[7]) s.aod_cx[key[6] - '0'] = (uint8_t)(val >= 0 && val < CX_COUNT ? val : CX_NONE);
      else if (!strcmp(key, "screen_off_s")) s.screen_off_s = (uint8_t)(val < 10 ? 10 : val > 240 ? 240 : val);
      else if (!strcmp(key, "wifi")) s.wifi = val != 0;
      else if (!strcmp(key, "auto_update")) s.auto_update = val != 0;
      else if (!strcmp(key, "tz_min")) s.tz_min = (int16_t)(val < -720 ? -720 : val > 840 ? 840 : val);
      else if (!strcmp(key, "wp_target")) s.wp_target = (uint8_t)(val < 0 ? 0 : val > 15 ? 15 : val);
    }
    t = strchr(t, '\n'); if (t) t++;
  }
}
void StoreSvc::publish_settings(uint32_t now) { Event e(EV_SETTINGS, now); e.u.settings = sh_.settings; bus_.events.push(e); }
void StoreSvc::publish_waypoints(uint32_t now) {
  bus_.events.push(Event(EV_WP_BEGIN, now));
  for (int i = 0; i < n_wp_; i++) { Event e(EV_WP_ITEM, now); e.u.wp = wps_[i]; bus_.events.push(e); }
  bus_.events.push(Event(EV_WP_END, now));
}
void StoreSvc::load() {
  char buf[1024]; int n = hal_.storage->read("/settings.txt", buf, sizeof buf - 1);
  if (n > 0) { buf[n] = 0; settings_from_text(buf, sh_.settings); }
  n = hal_.storage->read("/waypoints.csv", buf, sizeof buf - 1); n_wp_ = 0;
  if (n > 0) {
    buf[n] = 0;
    for (char* line = strtok(buf, "\n"); line && n_wp_ < MAX_WP; line = strtok(nullptr, "\n")) {
      Waypoint w; memset(&w, 0, sizeof w);
      if (sscanf(line, "%15[^,],%lf,%lf", w.name, &w.lat, &w.lon) == 3) wps_[n_wp_++] = w;
    }
  }
}
void StoreSvc::save_settings() { char buf[512]; settings_to_text(sh_.settings, buf, sizeof buf); hal_.storage->write("/settings.txt", buf); }
void StoreSvc::save_waypoints() {
  char buf[1024]; int o = 0; buf[0] = 0;
  for (int i = 0; i < n_wp_; i++) o += snprintf(buf + o, sizeof buf - (size_t)o, "%s,%.7f,%.7f\n", wps_[i].name, wps_[i].lat, wps_[i].lon);
  hal_.storage->write("/waypoints.csv", buf);
}
void StoreSvc::tick(uint32_t now) {
  if (!loaded_ || due(now, t_mount_, 3000)) {
    bool m = hal_.storage->mounted();
    if (!loaded_ || m != mounted_) {
      mounted_ = m; if (m || !loaded_) load();
      loaded_ = true; sh_.settings_rev++;
      Event e(EV_STORAGE, now); e.u.storage.mounted = m; bus_.events.push(e);
      publish_settings(now); publish_waypoints(now);
    }
  }
  Cmd c;
  while (inbox.pop(c)) {
    if (c.type == CMD_SET_SETTINGS) {
      sh_.settings = c.settings; sh_.settings_rev++; save_settings(); publish_settings(now);
    } else if (c.type == CMD_WP_SAVE) {
      Event t(EV_TOAST, now);
      if (!sh_.fix) snprintf(t.u.toast, sizeof t.u.toast, "No GPS fix yet");
      else if (n_wp_ >= MAX_WP) snprintf(t.u.toast, sizeof t.u.toast, "Waypoint list full");
      else {
        Waypoint& w = wps_[n_wp_]; memset(&w, 0, sizeof w);
        if (c.text[0]) snprintf(w.name, sizeof w.name, "%.15s", c.text); else snprintf(w.name, sizeof w.name, "WP %d", n_wp_ + 1);
        w.lat = sh_.lat_e7 * 1e-7; w.lon = sh_.lon_e7 * 1e-7; n_wp_++;
        save_waypoints(); publish_waypoints(now);
        snprintf(t.u.toast, sizeof t.u.toast, "Saved %s", w.name);
      }
      bus_.events.push(t);
    } else if (c.type == CMD_WP_DELETE) {
      if (c.a >= 0 && c.a < n_wp_) {
        for (int i = c.a; i < n_wp_ - 1; i++) wps_[i] = wps_[i + 1];
        n_wp_--; save_waypoints(); publish_waypoints(now);
      }
    }
  }
  LogLine l; int budget = 4; // a few lines per tick keeps the SPI bus free for LoRa
  while (budget-- > 0 && log_.pop(l)) if (mounted_) hal_.storage->append(l.path, l.line);
}

// ---------------------------------------------------------------- wiring
Services::Services(Bus& b, Hal& h)
    : bus(b), hal(h), input(b, h, shared), house(b, h, shared), scan(b, h, shared), sweep(b, h, shared, log),
      gnss(b, h, shared, log), mesh(b, h, shared, log), store(b, h, shared, log), net(b, h, shared) {}
void Services::route(uint32_t now) {
  (void)now; Cmd c;
  while (bus.cmds.pop(c)) {
    switch (c.type) {
      case CMD_SET_MODE: shared.mode = (uint8_t)c.a; break;
      case CMD_SEND_TEXT: case CMD_SOS: mesh.inbox.push(c); break;
      case CMD_WP_SAVE: case CMD_WP_DELETE: case CMD_SET_SETTINGS: store.inbox.push(c); break;
      case CMD_HAPTIC: if (shared.settings.haptics) hal.haptics->play((uint8_t)c.a); break;
      case CMD_NET: case CMD_HOTSPOT: case CMD_OTA: case CMD_WX_REFRESH: case CMD_WIFI_ADD: net.inbox.push(c); break;
      case CMD_BRIGHTNESS: shared.brightness_override = (int16_t)c.a; break;
      case CMD_SLEEP: {
        if (shared.sos) break; // never sleep through an active SOS
        uint8_t f[32]; size_t n = ubx::backup_request(f, sizeof f); hal.gnss->write(f, (int)n); // GNSS has no power switch
        hal.backlight->set(0); hal.siren->set(false, 0); hal.led->set(0, 0, 0); hal.sys->deep_sleep();
        break;
      }
    }
  }
}
void Services::tick_all(uint32_t now) {
  route(now); store.tick(now); input.tick(now); house.tick(now); scan.tick(now); sweep.tick(now); gnss.tick(now); mesh.tick(now); net.tick(now);
}
}
