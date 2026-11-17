// System and utility apps: Settings, Weather, Console (web hotspot), Torch,
// Level, Updates, first-boot Setup.
#include "ui.h"
#include "geo.h"
#include "weather.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <initializer_list>

namespace ss { namespace ui {

// ------------------------------------------------------------------ settings
enum SetItem { S_HDR, S_PROFILE, S_BRIGHT, S_AOD, S_SLEEP_AFTER, S_CX0, S_CX1, S_CX2, S_WIFI, S_NETS, S_HOTSPOT, S_CHAN, S_POWER, S_PSM,
  S_HAPTICS, S_SIREN, S_UNITS, S_TZ, S_LOG, S_UPDATE, S_AUTOUPD, S_ABOUT, S_SLEEP };
struct SetRow { uint8_t item; const char* label; };
static const SetRow ROWS[] = {
  {S_HDR, "Profile"}, {S_PROFILE, "Profile"},
  {S_HDR, "Display"}, {S_BRIGHT, "Brightness"}, {S_AOD, "Always-on display"}, {S_SLEEP_AFTER, "Dim after"}, {S_CX0, "AOD left"}, {S_CX1, "AOD centre"}, {S_CX2, "AOD right"},
  {S_HDR, "Connectivity"}, {S_WIFI, "Wi-Fi"}, {S_NETS, "Saved networks"}, {S_HOTSPOT, "Web console"},
  {S_HDR, "Radio"}, {S_CHAN, "LoRa channel"}, {S_POWER, "LoRa power"}, {S_PSM, "GPS power save"},
  {S_HDR, "Sound and haptics"}, {S_HAPTICS, "Haptics"}, {S_SIREN, "SOS siren"},
  {S_HDR, "Units and time"}, {S_UNITS, "Units"}, {S_TZ, "Time zone"},
  {S_HDR, "Storage"}, {S_LOG, "Log to microSD"},
  {S_HDR, "System"}, {S_UPDATE, "Software update"}, {S_AUTOUPD, "Auto-check updates"}, {S_ABOUT, "About"}, {S_SLEEP, "Sleep now"},
};
static const int N_ROWS = (int)(sizeof ROWS / sizeof ROWS[0]);
static const char* cx_name(uint8_t c) { static const char* n[] = {"None", "Weather", "Waypoint", "Battery", "Mesh", "Satellites", "RF level", "Wi-Fi"}; return n[c < CX_COUNT ? c : 0]; }

struct SettingsPage : Page {
  using Page::Page; lv_obj_t* body = nullptr; lv_obj_t* rows[N_ROWS] = {nullptr};
  Mode mode() const override { return MODE_SETTINGS; }
  void build() override {
    scr = page_root(ui); header(scr, "Settings", GREY, nullptr, 0, 0);
    body = lv_obj_create(scr); lv_obj_remove_style_all(body); lv_obj_set_pos(body, 0, TOP - 8); lv_obj_set_size(body, W, H - TOP - 22);
    lv_obj_set_scroll_dir(body, LV_DIR_VER); lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF); lv_obj_add_flag(body, LV_OBJ_FLAG_EVENT_BUBBLE);
    if (ROWS[ui.settings_sel].item == S_HDR) ui.settings_sel++;
    draw(false);
  }
  void value(int item, char* b, size_t n, uint32_t& col) {
    const Vm& v = ui.vm; const Settings& s = v.settings; col = DIM;
    switch (item) {
      case S_PROFILE: snprintf(b, n, "%s", s.profile ? "Adventure" : "Hackerspace"); col = theme().acc; break;
      case S_BRIGHT: snprintf(b, n, "%u%%", s.brightness); break;
      case S_AOD: snprintf(b, n, "%s", s.aod ? "On" : "Off"); break;
      case S_SLEEP_AFTER: snprintf(b, n, "%u s", s.screen_off_s); break;
      case S_CX0: case S_CX1: case S_CX2: snprintf(b, n, "%s", cx_name(s.aod_cx[item - S_CX0])); break;
      case S_WIFI:
        snprintf(b, n, "%s", !s.wifi ? "Off" : v.net.state == NET_ONLINE ? v.net.ssid : v.net.state == NET_CONNECTING ? "Joining..." : v.net.state == NET_NO_NETWORK ? "No saved network" : "On");
        if (v.net.state == NET_ONLINE) col = GREEN;
        break;
      case S_NETS: snprintf(b, n, "Add in console"); break;
      case S_HOTSPOT: snprintf(b, n, "%s", v.net.ap ? "Running" : "Off"); if (v.net.ap) col = CYAN; break;
      case S_CHAN: snprintf(b, n, "%d   %.1f MHz", s.lora_channel + 1, lora::CHANNELS[s.lora_channel % lora::N_CHANNELS]); break;
      case S_POWER: snprintf(b, n, "%u dBm", s.lora_power); break;
      case S_PSM: snprintf(b, n, "%s", s.gnss_psm ? "On" : "Off"); break;
      case S_HAPTICS: snprintf(b, n, "%s", s.haptics ? "On" : "Off"); break;
      case S_SIREN: snprintf(b, n, "%s", s.siren ? "On" : "Off"); break;
      case S_UNITS: snprintf(b, n, "%s", s.metric ? "Metric" : "Imperial"); break;
      case S_TZ: snprintf(b, n, "UTC%c%d:%02d", s.tz_min < 0 ? '-' : '+', abs(s.tz_min) / 60, abs(s.tz_min) % 60); break;
      case S_LOG: snprintf(b, n, "%s", !v.sd ? "No card" : s.logging ? "On" : "Off"); break;
      case S_UPDATE: snprintf(b, n, "%s", v.ota.state == OTA_AVAILABLE ? v.ota.version : SS_FW_VERSION); if (v.ota.state == OTA_AVAILABLE) col = 0xFF3D7F; break;
      case S_AUTOUPD: snprintf(b, n, "%s", s.auto_update ? "On" : "Off"); break;
      case S_ABOUT: snprintf(b, n, "Node %04X", v.lora.node_id); break;
      default: b[0] = 0; break;
    }
  }
  void draw(bool anim = true) {
    int32_t sy = lv_obj_get_scroll_y(body); lv_obj_clean(body); char b[48]; int y = 0; uint32_t col;
    for (int i = 0; i < N_ROWS; i++) {
      if (ROWS[i].item == S_HDR) { rows[i] = section(body, y + 14, ROWS[i].label); y += 40; continue; }
      bool f = i == ui.settings_sel;
      lv_obj_t* r = row(body, y, 54, f, theme().acc, i); rows[i] = r;
      text(r, 18, 15, ROWS[i].label, F16, TXT); value(ROWS[i].item, b, sizeof b, col);
      text_w(r, 170, 16, W - 2 * M - 188, b, theme().mono_data ? M14 : F16, f && col == DIM ? TXT : col, LV_TEXT_ALIGN_RIGHT);
      y += 60;
    }
    box(body, 0, y, W, 20, BG, 0);
    lv_obj_update_layout(body); lv_obj_scroll_to_y(body, sy, LV_ANIM_OFF);
    lv_obj_scroll_to_view(rows[ui.settings_sel], anim ? LV_ANIM_ON : LV_ANIM_OFF);
  }
  void refresh(uint32_t what) override { if (what & (R_SET | R_LINK | R_NET | R_OTA)) draw(false); }
  void change(int d) {
    Settings s = ui.vm.settings; int item = ROWS[ui.settings_sel].item;
    switch (item) {
      case S_PROFILE: s.profile = s.profile ? PROFILE_HACKER : PROFILE_ADVENTURE; break;
      case S_BRIGHT: { int v = s.brightness + d * 10; s.brightness = (uint8_t)(v < 10 ? 10 : v > 100 ? 100 : v); break; }
      case S_AOD: s.aod = !s.aod; break;
      case S_SLEEP_AFTER: { static const uint8_t o[] = {15, 30, 60, 120, 240}; int k = 0; while (k < 4 && o[k] < s.screen_off_s) k++; k = (k + 5 + d) % 5; s.screen_off_s = o[k]; break; }
      case S_CX0: case S_CX1: case S_CX2: { uint8_t& c = s.aod_cx[item - S_CX0]; c = (uint8_t)((c + CX_COUNT + d) % CX_COUNT); break; }
      case S_WIFI: s.wifi = !s.wifi; ui.cmd(Cmd(CMD_NET, s.wifi)); break;
      case S_NETS: ui.toast("Join the web console and add networks there"); return;
      case S_HOTSPOT: ui.open_app(APP_CONSOLE); return;
      case S_CHAN: s.lora_channel = (uint8_t)((s.lora_channel + lora::N_CHANNELS + d) % lora::N_CHANNELS); break;
      case S_POWER: { int v = s.lora_power + d * 2; s.lora_power = (uint8_t)(v < 2 ? 2 : v > 22 ? 22 : v); break; }
      case S_PSM: s.gnss_psm = !s.gnss_psm; break;
      case S_HAPTICS: s.haptics = !s.haptics; break;
      case S_SIREN: s.siren = !s.siren; break;
      case S_UNITS: s.metric = !s.metric; break;
      case S_TZ: { int v = s.tz_min + d * 30; s.tz_min = (int16_t)(v < -720 ? 840 : v > 840 ? -720 : v); break; }
      case S_LOG: s.logging = !s.logging; break;
      case S_UPDATE: ui.open_app(APP_UPDATES); return;
      case S_AUTOUPD: s.auto_update = !s.auto_update; break;
      case S_ABOUT: { char b[48]; snprintf(b, sizeof b, "SignalOS %s  built %s", SS_FW_VERSION, __DATE__); ui.toast(b); return; }
      case S_SLEEP: ui.cmd(Cmd(CMD_SLEEP)); return;
    }
    Ui& u = ui; u.haptic(); u.apply(s); if (u.page() == this) draw(false); // apply() may rebuild (and delete) this page
  }
  bool key(uint16_t k) override {
    if (k & KEY_B) { ui.go(PG_HOME, -1); return true; }
    int s = ui.settings_sel;
    if (k & KEY_DOWN) { do s++; while (s < N_ROWS && ROWS[s].item == S_HDR); if (s >= N_ROWS) return false; ui.settings_sel = s; ui.haptic(); draw(); return true; }
    if (k & KEY_UP) { do s--; while (s >= 0 && ROWS[s].item == S_HDR); if (s < 0) return false; ui.settings_sel = s; ui.haptic(); draw(); return true; }
    int d = (k & KEY_RIGHT) ? 1 : (k & KEY_LEFT) ? -1 : (k & KEY_A) ? 1 : 0;
    if (d) { change(d); return true; }
    return false;
  }
  bool tap(int id) override { ui.settings_sel = id; change(1); return true; }
};

// ------------------------------------------------------------------ weather
struct WeatherPage : Page {
  using Page::Page; lv_obj_t* body = nullptr;
  void build() override {
    scr = page_root(ui); header(scr, "Weather", AMBER, nullptr, 0, 0);
    body = lv_obj_create(scr); lv_obj_remove_style_all(body); lv_obj_set_pos(body, 0, TOP - 4); lv_obj_set_size(body, W, H - TOP - 18);
    lv_obj_set_scroll_dir(body, LV_DIR_VER); lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);
    draw();
  }
  void draw() {
    int32_t sy = lv_obj_get_scroll_y(body); lv_obj_clean(body);
    const Vm& v = ui.vm; const Weather* wx = v.weather(); char b[64]; const Theme& t = theme(); bool met = v.settings.metric; clk::Civil c;
    int cw = W - 2 * M, y = 0;
    lv_obj_t* hero = card(body, M, y, cw, 150);
    if (wx) {
      glyph(hero, wx_icon(wx->cond, true), 12, 26, t.colour_tiles ? AMBER : t.acc);
      fmt_temp(b, sizeof b, wx->temp_c, met); text(hero, 124, 14, b, F48, TXT);
      text_w(hero, 124, 72, cw - 140, wx->online ? wx::cond_name((wx::Cond)wx->cond) : wx->text, F20, TXT);
      if (wx->online && wx->n_days) { char hi[8], lo[8]; fmt_temp(hi, 8, wx->days[0].hi, met); fmt_temp(lo, 8, wx->days[0].lo, met); snprintf(b, sizeof b, "H %s  L %s   wind %.0f %s   RH %u%%", hi, lo, met ? wx->wind_kmh : wx->wind_kmh * 0.621f, met ? "km/h" : "mph", wx->rh); }
      else snprintf(b, sizeof b, "RH %u%%   %.1f hPa sea level", wx->rh, wx->hpa);
      text_w(hero, 124, 104, cw - 136, b, theme().mono_data ? M12 : F14, DIM);
    } else { glyph(hero, &wx96_partly, 12, 26, FAINT); text(hero, 124, 40, "No data yet", F24, DIM); text_w(hero, 124, 76, cw - 136, "Barometer readings start at power-on.", F14, MUTE); }
    y += 160;
    if (v.have_wx_online) {
      const Weather& w = v.wx_online;
      lv_obj_t* hr = card(body, M, y, cw, 112); int colw = (cw - 16) / 6; bool tm = ui.local_time(c);
      for (int i = 0; i < 6; i++) {
        int x = 8 + i * colw;
        if (i == 0) snprintf(b, sizeof b, "Now"); else if (tm) snprintf(b, sizeof b, "%02u", (c.hour + i) % 24); else snprintf(b, sizeof b, "+%dh", i);
        text_w(hr, x, 10, colw, b, theme().mono_data ? M12 : F12, DIM, LV_TEXT_ALIGN_CENTER);
        glyph(hr, wx_icon(w.hourly_cond[i], false), x + (colw - 40) / 2, 32, t.colour_tiles ? (w.hourly_cond[i] == wx::CLEAR ? AMBER : TXT) : t.acc);
        if (w.hourly[i] != -128) { fmt_temp(b, sizeof b, w.hourly[i], met); text_w(hr, x, 80, colw, b, DATA(14), TXT, LV_TEXT_ALIGN_CENTER); }
      }
      y += 122;
      lv_obj_t* dy = card(body, M, y, cw, 20 + 44 * w.n_days);
      for (int i = 0; i < w.n_days; i++) {
        int ry = 12 + i * 44; char hi[8], lo[8];
        if (i == 0) snprintf(b, sizeof b, "Today"); else if (tm) snprintf(b, sizeof b, "%s", clk::weekday_short((uint8_t)((c.weekday + i) % 7))); else snprintf(b, sizeof b, "+%d d", i);
        text(dy, 16, ry + 8, b, F16, TXT);
        glyph(dy, wx_icon(w.days[i].cond, false), 104, ry, t.colour_tiles ? (w.days[i].cond == wx::CLEAR ? AMBER : TXT) : t.acc);
        if (w.days[i].pop >= 20) { snprintf(b, sizeof b, "%u%%", w.days[i].pop); text(dy, 150, ry + 10, b, M12, CYAN); }
        fmt_temp(lo, 8, w.days[i].lo, met); fmt_temp(hi, 8, w.days[i].hi, met);
        text_w(dy, 196, ry + 8, 40, lo, DATA(14), DIM, LV_TEXT_ALIGN_RIGHT);
        // range bar on a -30..+30 scale
        int x0 = 246, bw = cw - x0 - 64; auto px = [&](int d) { int p = (d + 30) * bw / 60; return p < 0 ? 0 : p > bw ? bw : p; };
        box(dy, x0, ry + 16, bw, 6, FAINT, 3);
        lv_obj_t* rg = box(dy, x0 + px(w.days[i].lo), ry + 16, px(w.days[i].hi) - px(w.days[i].lo) + 6, 6, t.acc, 3);
        lv_obj_set_style_bg_grad_color(rg, C(t.acc2), 0); lv_obj_set_style_bg_grad_dir(rg, LV_GRAD_DIR_HOR, 0);
        text_w(dy, cw - 56, ry + 8, 44, hi, DATA(14), TXT, LV_TEXT_ALIGN_RIGHT);
      }
      y += 30 + 44 * w.n_days;
    } else {
      lv_obj_t* n = card(body, M, y, cw, 84);
      text_w(n, 16, 12, cw - 32, v.settings.wifi ? (v.gnss.valid || v.net.state != NET_ONLINE ? "Waiting for Wi-Fi" : "Waiting for a GPS fix") : "Offline forecast", F16, TXT);
      lv_obj_t* l = text(n, 16, 38, v.settings.wifi ? "The hourly and 3-day forecast load once the device is online and has a position." : "Turn on Wi-Fi in Settings for hourly and 3-day forecasts. Offline, SignalOS reads the barometer.", F12, DIM);
      lv_obj_set_width(l, cw - 32); lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
      y += 94;
    }
    if (v.have_wx_local) { // barometer card, always
      const Weather& l = v.wx_local; lv_obj_t* bc = card(body, M, y, cw, 92);
      glyph(bc, &ic30_sweep, 16, 14, t.acc);
      snprintf(b, sizeof b, "%.1f hPa", l.hpa); text(bc, 58, 12, b, DATA(20), TXT);
      const char* tr = l.trend < 0 ? "falling" : l.trend > 0 ? "rising" : "steady";
      if (l.cond != wx::UNKNOWN) snprintf(b, sizeof b, "%s %+.1f hPa / 3 h", tr, l.dp3h); else snprintf(b, sizeof b, "trend after 1 h of readings");
      text_w(bc, 190, 16, cw - 206, b, theme().mono_data ? M12 : F14, l.trend < 0 ? AMBER : DIM, LV_TEXT_ALIGN_RIGHT);
      snprintf(b, sizeof b, "Next 12 h: %s", l.text); text_w(bc, 16, 54, cw - 32, b, F14, TXT);
      y += 102;
    }
    text_w(body, M, y + 4, cw, v.have_wx_online ? "Forecast: Open-Meteo.com (CC BY 4.0)" : "", F12, MUTE);
    box(body, 0, y + 24, W, 10, BG, 0);
    lv_obj_update_layout(body); lv_obj_scroll_to_y(body, sy, LV_ANIM_OFF);
  }
  void refresh(uint32_t what) override { if (what & (R_WX | R_SET | R_NET)) draw(); }
  bool key(uint16_t k) override {
    if (k & KEY_B) { ui.go(PG_HOME, -1); return true; }
    if (k & KEY_DOWN) { lv_obj_scroll_by_bounded(body, 0, -140, LV_ANIM_ON); return true; }
    if (k & KEY_UP) { lv_obj_scroll_by_bounded(body, 0, 140, LV_ANIM_ON); return true; }
    if (k & KEY_A) { ui.cmd(Cmd(CMD_WX_REFRESH)); ui.toast("Refreshing"); return true; }
    return false;
  }
};

// ------------------------------------------------------------------ console (web hotspot)
struct Console : Page {
  using Page::Page; lv_obj_t* body = nullptr; bool was_ap = false;
  void build() override {
    scr = page_root(ui); header(scr, "Console", theme().acc, nullptr, 0, 0);
    body = box(scr, 0, TOP, W, H - TOP - 30, BG, 0); lv_obj_add_flag(body, LV_OBJ_FLAG_EVENT_BUBBLE);
    hint(scr, "A start/stop hotspot   B back"); draw();
  }
  void draw() {
    lv_obj_clean(body); const Vm& v = ui.vm; const Theme& t = theme(); char b[96]; int cw = W - 2 * M;
    lv_obj_t* c = card(body, M, 0, cw, 200); tap_id(c, 0);
    if (v.net.ap) {
      lv_obj_t* q = lv_qrcode_create(c); lv_qrcode_set_size(q, 150); lv_qrcode_set_dark_color(q, C(0x000000)); lv_qrcode_set_light_color(q, C(0xFFFFFF));
      lv_obj_set_pos(q, 16, 16); lv_obj_set_style_border_width(q, 6, 0); lv_obj_set_style_border_color(q, C(0xFFFFFF), 0);
      snprintf(b, sizeof b, "WIFI:T:WPA;S:%s;P:%s;;", v.net.ap_ssid, v.net.ap_pass); lv_qrcode_update(q, b, (uint32_t)strlen(b));
      int x = 186; text(c, x, 16, "Scan to join", F14, DIM);
      text_w(c, x, 38, cw - x - 10, v.net.ap_ssid, M14, TXT); text_w(c, x, 60, cw - x - 10, v.net.ap_pass, M14, t.acc);
      text(c, x, 92, "then open", F14, DIM); text(c, x, 114, "192.168.4.1", M14, TXT);
      snprintf(b, sizeof b, "%u connected", v.net.ap_clients); pill(c, x, 146, b, v.net.ap_clients ? t.acc : CARD_HI, v.net.ap_clients ? t.acc_ink : DIM, M12);
      if (!was_ap) anim_scale(q, 200, 256, 380);
    } else {
      glyph(c, &ic30_web, 16, 16, t.acc); text(c, 58, 18, "Web console", F20, TXT);
      lv_obj_t* l = text(c, 16, 60, "Start a hotspot and open the console in any browser: download logs and waypoints, add Wi-Fi networks, change settings, upload a firmware file. No app needed.", F14, DIM);
      lv_obj_set_width(l, cw - 32); lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
      lv_obj_t* p = pill(c, 16, 148, "Start hotspot", t.acc, t.acc_ink, F16); lv_obj_set_style_pad_ver(p, 9, 0);
    }
    was_ap = v.net.ap;
    lv_obj_t* s = card(body, M, 210, cw, 104);
    section(s, 10, "station"); lv_obj_set_x(lv_obj_get_child(s, 0), 14);
    static const char* st[] = {"off", "searching", "joining", "online", "no saved network in range", "failed"};
    snprintf(b, sizeof b, "wifi    %s", st[v.net.state < 6 ? v.net.state : 0]); text(s, 14, 34, b, M14, v.net.state == NET_ONLINE ? t.acc : TXT);
    if (v.net.state == NET_ONLINE) { snprintf(b, sizeof b, "ssid    %s  %d dBm", v.net.ssid, v.net.rssi); text_w(s, 14, 56, cw - 28, b, M14, TXT); snprintf(b, sizeof b, "ip      %s", v.net.ip); text(s, 14, 78, b, M14, TXT); }
    else text_w(s, 14, 60, cw - 28, v.settings.wifi ? "Add networks in the console, or put ssid<TAB>password lines in /wifi.txt on the card." : "Wi-Fi joining is off. Settings > Wi-Fi.", F12, DIM);
    lv_obj_t* e = card(body, M, 324, cw, 52);
    text_w(e, 14, 8, cw - 28, "GET /  /logs  /waypoints.csv  /settings.txt", M12, DIM); text_w(e, 14, 28, cw - 28, "POST /wifi  /settings  /update (firmware.bin)", M12, DIM);
  }
  void refresh(uint32_t what) override { if (what & (R_NET | R_SET)) draw(); }
  bool key(uint16_t k) override {
    if (k & KEY_B) { ui.go(PG_HOME, -1); return true; }
    if (k & KEY_A) { ui.haptic(HAP_DOUBLE); ui.cmd(Cmd(CMD_HOTSPOT, !ui.vm.net.ap)); return true; }
    return false;
  }
  bool tap(int) override { return key(KEY_A); }
};

// ------------------------------------------------------------------ torch
struct Torch : Page {
  using Page::Page; int level = 100; lv_obj_t* l = nullptr;
  void build() override {
    scr = page_root(ui); paint();
    l = text_w(scr, 0, H - 44, W, "", F14, 0x5A1010, LV_TEXT_ALIGN_CENTER); label();
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE); tap_id(scr, 0);
    ui.cmd(Cmd(CMD_BRIGHTNESS, level));
  }
  void paint() { lv_obj_set_style_bg_color(scr, C(ui.torch_red ? 0xFF1A00 : 0xFFFFFF), 0); }
  void label() { char b[64]; snprintf(b, sizeof b, "%s  %d%%    A colour   Up/Down level   B off", ui.torch_red ? "Red" : "White", level); lv_label_set_text(l, b); lv_obj_set_style_text_color(l, C(ui.torch_red ? 0x5A0800 : 0x9A9A9A), 0); }
  bool key(uint16_t k) override {
    if (k & KEY_B) { ui.torch_red = false; ui.go(PG_HOME, -1); return true; }
    if (k & KEY_A) { ui.torch_red = !ui.torch_red; anim_bg(scr, ui.torch_red ? 0xFFFFFF : 0xFF1A00, ui.torch_red ? 0xFF1A00 : 0xFFFFFF, 250); label(); return true; }
    int d = (k & KEY_UP) ? 10 : (k & KEY_DOWN) ? -10 : 0;
    if (d) { level = level + d < 10 ? 10 : level + d > 100 ? 100 : level + d; ui.cmd(Cmd(CMD_BRIGHTNESS, level)); label(); return true; }
    return false;
  }
  bool tap(int) override { return key(KEY_A); }
};

// ------------------------------------------------------------------ level
struct Level : Page {
  using Page::Page; lv_obj_t *bubble = nullptr, *l_p = nullptr, *l_r = nullptr, *l_s = nullptr; float bx = 0, by = 0, zp = 0, zr = 0; bool was_level = false;
  static const int CX = W / 2, CY = TOP + 150, R = 150;
  Mode mode() const override { return MODE_LEVEL; }
  void build() override {
    scr = page_root(ui); header(scr, "Level", theme().acc, nullptr, 0, 0);
    lv_obj_t* d = box(scr, CX - R, CY - R, 2 * R, 2 * R, theme().card, R);
    lv_obj_set_style_border_width(d, 2, 0); lv_obj_set_style_border_color(d, C(LINE), 0);
    for (int r : {100, 40}) { lv_obj_t* ring = box(d, R - r, R - r, 2 * r, 2 * r, theme().card, r); lv_obj_set_style_border_width(ring, 2, 0); lv_obj_set_style_border_color(ring, C(r == 40 ? theme().acc : LINE), 0); lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0); }
    box(d, R - 1, 10, 2, 2 * R - 20, LINE, 1); box(d, 10, R - 1, 2 * R - 20, 2, LINE, 1);
    bubble = box(scr, CX - 24, CY - 24, 48, 48, theme().acc, 24); lv_obj_set_style_bg_opa(bubble, LV_OPA_80, 0);
    lv_obj_set_style_border_width(bubble, 2, 0); lv_obj_set_style_border_color(bubble, C(TXT), 0);
    int y = CY + R + 14;
    l_p = text_w(scr, M, y, W / 2 - M, "", DATA(24), TXT, LV_TEXT_ALIGN_CENTER); l_r = text_w(scr, W / 2, y, W / 2 - M, "", DATA(24), TXT, LV_TEXT_ALIGN_CENTER);
    text_w(scr, M, y + 32, W / 2 - M, "pitch", F12, DIM, LV_TEXT_ALIGN_CENTER); text_w(scr, W / 2, y + 32, W / 2 - M, "roll", F12, DIM, LV_TEXT_ALIGN_CENTER);
    hint(scr, "Lay the device flat or against an edge.   A zero   B back");
  }
  void tick() override {
    float p = ui.vm.pitch - zp, r = ui.vm.roll - zr;
    float tx = r * 12, ty = -p * 12, m = sqrtf(tx * tx + ty * ty); if (m > R - 28) { tx *= (R - 28) / m; ty *= (R - 28) / m; }
    bx += (tx - bx) * 0.2f; by += (ty - by) * 0.2f; // damped like a real bubble
    lv_obj_set_pos(bubble, CX - 24 + (int)bx, CY - 24 + (int)by);
    char b[16]; snprintf(b, sizeof b, "%+.1f\xC2\xB0", p); lv_label_set_text(l_p, b); snprintf(b, sizeof b, "%+.1f\xC2\xB0", r); lv_label_set_text(l_r, b);
    bool lvl = fabsf(p) < 0.5f && fabsf(r) < 0.5f;
    if (lvl != was_level) { was_level = lvl; lv_obj_set_style_bg_color(bubble, C(lvl ? GREEN : theme().acc), 0); if (lvl) ui.haptic(); }
  }
  bool key(uint16_t k) override {
    if (k & KEY_B) { ui.go(PG_HOME, -1); return true; }
    if (k & KEY_A) { zp = ui.vm.pitch; zr = ui.vm.roll; ui.haptic(HAP_DOUBLE); ui.toast("Zeroed on this surface"); return true; }
    return false;
  }
};

// ------------------------------------------------------------------ updates
struct Updates : Page {
  using Page::Page; lv_obj_t* body = nullptr; lv_obj_t* l_hint = nullptr; lv_obj_t* ring = nullptr; lv_obj_t* l_pct = nullptr; int shown = 0;
  void build() override {
    scr = page_root(ui); header(scr, "Updates", 0xFF3D7F, nullptr, 0, 0);
    body = box(scr, 0, TOP, W, H - TOP - 30, BG, 0); lv_obj_add_flag(body, LV_OBJ_FLAG_EVENT_BUBBLE);
    hint(scr, ""); l_hint = lv_obj_get_child(scr, -1);
    draw();
  }
  void draw() {
    lv_obj_clean(body); ring = nullptr; const Vm& v = ui.vm; char b[96]; int cw = W - 2 * M;
    lv_obj_t* h = card(body, M, 0, cw, 92);
    app_tile(h, 14, 14, 64, APP_UPDATES); lv_obj_set_pos(lv_obj_get_child(lv_obj_get_child(h, 0), 0), 6, 6);
    text(h, 94, 14, "SignalOS", F24, TXT); snprintf(b, sizeof b, "Version %s  stable", SS_FW_VERSION); text(h, 94, 48, b, M14, DIM);
    lv_obj_t* c = card(body, M, 102, cw, 210); tap_id(c, 0);
    const char* action = nullptr;
    switch (v.ota.state) {
      case OTA_CHECKING: text(c, 18, 18, "Checking...", F24, TXT); break;
      case OTA_AVAILABLE: {
        snprintf(b, sizeof b, "SignalOS %s", v.ota.version); text(c, 18, 16, b, F28, TXT);
        snprintf(b, sizeof b, "%.1f MB   verified with SHA-256", v.ota.size / 1048576.0); text(c, 18, 54, b, M12, DIM);
        lv_obj_t* l = text(c, 18, 82, v.ota.notes, F16, TXT); lv_obj_set_width(l, cw - 36); lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
        action = "Download and install"; break;
      }
      case OTA_DOWNLOADING: case OTA_VERIFYING: {
        ring = lv_arc_create(c); lv_obj_remove_style_all(ring); lv_obj_set_size(ring, 130, 130); lv_obj_set_pos(ring, 18, 40);
        lv_arc_set_bg_angles(ring, 0, 360); lv_arc_set_rotation(ring, 270); lv_arc_set_range(ring, 0, 100); lv_arc_set_value(ring, shown);
        lv_obj_set_style_arc_width(ring, 12, LV_PART_MAIN); lv_obj_set_style_arc_color(ring, C(FAINT), LV_PART_MAIN); lv_obj_set_style_arc_opa(ring, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_arc_width(ring, 12, LV_PART_INDICATOR); lv_obj_set_style_arc_color(ring, C(0xFF3D7F), LV_PART_INDICATOR); lv_obj_set_style_arc_opa(ring, LV_OPA_COVER, LV_PART_INDICATOR); lv_obj_set_style_arc_rounded(ring, true, LV_PART_INDICATOR);
        lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
        l_pct = text_w(c, 18, 90, 130, "", DATA(24), TXT, LV_TEXT_ALIGN_CENTER);
        text(c, 170, 50, v.ota.state == OTA_VERIFYING ? "Verifying" : "Downloading", F20, TXT);
        snprintf(b, sizeof b, "SignalOS %s", v.ota.version); text(c, 170, 80, b, M14, DIM);
        lv_obj_t* l = text(c, 170, 110, "Keep going: scanning, mesh and SOS keep working.", F12, DIM); lv_obj_set_width(l, cw - 186); lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
        break;
      }
      case OTA_READY: text(c, 18, 18, "Ready to install", F28, TXT); snprintf(b, sizeof b, "SignalOS %s is verified. Restart takes about 10 s.", v.ota.version);
        { lv_obj_t* l = text(c, 18, 60, b, F14, DIM); lv_obj_set_width(l, cw - 36); lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP); }
        action = v.sos ? nullptr : "Restart now"; break;
      case OTA_ERROR: text(c, 18, 18, "Couldn't check", F28, TXT);
        { lv_obj_t* l = text(c, 18, 60, v.net.state == NET_ONLINE ? "The update server didn't answer or the release was invalid. Try again later." : "Not online. Turn on Wi-Fi in Settings, or upload firmware.bin in the web console.", F14, DIM); lv_obj_set_width(l, cw - 36); lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP); }
        action = "Try again"; break;
      default: text(c, 18, 18, v.ota.state == OTA_UP_TO_DATE ? "Up to date" : "Updates", F28, TXT);
        { lv_obj_t* l = text(c, 18, 60, v.net.state == NET_ONLINE ? "SignalOS checks once a day while online." : "Connect to Wi-Fi to check, or upload firmware.bin in the web console.", F14, DIM); lv_obj_set_width(l, cw - 36); lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP); }
        action = "Check now"; break;
    }
    if (action) { lv_obj_t* p = pill(c, 18, 156, action, 0xFF3D7F, 0xFFFFFF, F16); lv_obj_set_style_pad_ver(p, 10, 0); lv_obj_set_style_pad_hor(p, 18, 0); }
    lv_obj_t* n = card(body, M, 322, cw, 72);
    lv_obj_t* l = text(n, 16, 12, "Installs to the spare slot. If the new version fails its self-test on first boot, the device rolls back by itself.", F12, DIM);
    lv_obj_set_width(l, cw - 32); lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l_hint, action ? "A action   B back" : "B back");
  }
  void tick() override {
    if (!ring) return;
    int target = ui.vm.ota.progress;
    if (shown != target) { shown += (target - shown + (target > shown ? 3 : -3)) / 4; if (abs(target - shown) < 2) shown = target; lv_arc_set_value(ring, shown); char b[8]; snprintf(b, sizeof b, "%d%%", shown); lv_label_set_text(l_pct, b); }
  }
  void refresh(uint32_t what) override {
    if (!(what & (R_OTA | R_NET))) return;
    static uint8_t last = 255; bool same = last == ui.vm.ota.state && ring; last = ui.vm.ota.state;
    if (!same) draw();
  }
  bool key(uint16_t k) override {
    if (k & KEY_B) { ui.go(PG_HOME, -1); return true; }
    if (k & KEY_A) {
      uint8_t s = ui.vm.ota.state; ui.haptic(HAP_DOUBLE);
      if (s == OTA_AVAILABLE) ui.cmd(Cmd(CMD_OTA, 1)); else if (s == OTA_READY) ui.cmd(Cmd(CMD_OTA, 2)); else if (s != OTA_DOWNLOADING && s != OTA_VERIFYING) ui.cmd(Cmd(CMD_OTA, 0));
      return true;
    }
    return false;
  }
  bool tap(int) override { return key(KEY_A); }
};

// ------------------------------------------------------------------ first boot
struct Setup : Page {
  using Page::Page; int step = 0, sel = 0;
  void build() override {
    scr = page_root(ui); set_profile((uint8_t)sel); const Theme& t = theme();
    if (step == 0) {
      lv_obj_t* logo = box(scr, W / 2 - 60, 110, 120, 120, 0x0A0F0C, 36);
      lv_obj_set_style_border_width(logo, 2, 0); lv_obj_set_style_border_color(logo, C(0x1D3A28), 0);
      glyph(logo, &ic52_sweep, 34, 34, 0x39FF88); anim_scale(logo, 160, 256, 700);
      text_w(scr, 0, 256, W, "SignalOS", F48, TXT, LV_TEXT_ALIGN_CENTER);
      text_w(scr, 0, 316, W, "for Signal Scout", F20, DIM, LV_TEXT_ALIGN_CENTER);
      text_w(scr, 0, 380, W, "Wi-Fi and Bluetooth diagnostics, RF sweep,", F14, MUTE, LV_TEXT_ALIGN_CENTER);
      text_w(scr, 0, 400, W, "GPS, LoRa mesh and SOS in one pocket.", F14, MUTE, LV_TEXT_ALIGN_CENTER);
      lv_obj_t* p = pill(scr, 0, H - 72, "Press A to begin", TXT, 0x000000, F16); lv_obj_set_style_pad_ver(p, 10, 0); lv_obj_update_layout(p); lv_obj_set_x(p, W / 2 - lv_obj_get_width(p) / 2);
      tap_id(p, 99);
      return;
    }
    text(scr, M + 4, 52, "Choose a profile", F28, TXT);
    text(scr, M + 4, 92, "Changes layout, colours and which apps come first.", F14, DIM);
    static const char* name[] = {"Hackerspace", "Adventure"};
    static const char* desc[] = {"Phosphor on black, monospaced readouts. Scanners, sweep and the web console up front.", "Big type and colour icons. Position, navigation, mesh and weather up front. Wi-Fi off to save power."};
    for (int i = 0; i < 2; i++) {
      bool f = i == sel; int y = 130 + i * 158;
      lv_obj_t* c = box(scr, M, y, W - 2 * M, 146, f ? (i ? 0x24211C : 0x132018) : 0x101113, 22); tap_id(c, i);
      lv_obj_set_style_border_width(c, 2, 0); lv_obj_set_style_border_color(c, C(f ? (i ? 0xFF9F1C : 0x39FF88) : LINE), 0);
      lv_obj_t* tile = box(c, 16, 16, 64, 64, i ? 0xFF9F1C : 0x0A0F0C, 18);
      if (!i) { lv_obj_set_style_border_width(tile, 2, 0); lv_obj_set_style_border_color(tile, C(0x1D3A28), 0); }
      glyph(tile, i ? &ic30_mountain : &ic30_terminal, 17, 17, i ? 0x1A0E00 : 0x39FF88);
      text(c, 96, 22, name[i], i ? F24 : M20, TXT);
      lv_obj_t* l = text(c, 96, 56, desc[i], F14, DIM); lv_obj_set_width(l, W - 2 * M - 112); lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
      if (f) anim_scale(c, 245, 256, 260);
    }
    text_w(scr, 0, H - 52, W, "Up/Down choose   A confirm.  Switch any time from the shade.", t.mono_data ? M12 : F12, MUTE, LV_TEXT_ALIGN_CENTER);
  }
  bool key(uint16_t k) override {
    if (step == 0) { if (k & KEY_A) { step = 1; ui.haptic(); lv_obj_t* old = scr; build(); anim_x(scr, W, 0, 300); anim_x(old, 0, -W / 3, 300, 0, true); } return true; }
    if ((k & (KEY_UP | KEY_DOWN)) && sel != ((k & KEY_DOWN) ? 1 : 0)) { sel = (k & KEY_DOWN) ? 1 : 0; ui.haptic(); lv_obj_delete(scr); build(); return true; }
    if (k & KEY_A) {
      set_profile(ui.vm.settings.profile); // let apply() see the change and restyle the shell
      Ui& u = ui; Settings s = u.vm.settings; s.profile = (uint8_t)sel; s.setup_done = true; if (sel) s.wifi = false;
      u.haptic(HAP_DOUBLE); u.home_page = 1; u.home_sel = 0; u.apply(s); u.go(PG_HOME, 1); return true; // this page is gone after apply()
    }
    return false;
  }
  bool tap(int id) override {
    if (id == 99 || step == 0) return key(KEY_A);
    if (id == sel) return key(KEY_A);
    return key(id ? KEY_DOWN : KEY_UP);
  }
};

Page* new_settings(Ui& u) { return new SettingsPage(u); }
Page* new_weather(Ui& u) { return new WeatherPage(u); }
Page* new_console(Ui& u) { return new Console(u); }
Page* new_torch(Ui& u) { return new Torch(u); }
Page* new_level(Ui& u) { return new Level(u); }
Page* new_updates(Ui& u) { return new Updates(u); }
Page* new_setup(Ui& u) { return new Setup(u); }
}}
