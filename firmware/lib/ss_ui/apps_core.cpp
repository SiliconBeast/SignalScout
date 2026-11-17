// Launcher and the instrument apps: Wi-Fi / Channels / Bluetooth, AP detail,
// Sweep, Position / Waypoints / Mesh, Navigate, Quick reply.
#include "ui.h"
#include "geo.h"
#include "correlate.h"
#include "weather.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace ss { namespace ui {

static int best_channel(const Vm& v, float* load_out = nullptr, int* n5_out = nullptr) {
  // 2.4 GHz congestion: each AP loads its own channel fully and the neighbours partly (20 MHz overlap).
  float load[14] = {0}; int n5 = 0;
  for (int i = 0; i < v.n_ap; i++) {
    int ch = v.aps[i].channel; if (ch > 14) { n5++; continue; }
    float wgt = v.aps[i].rssi >= -60 ? 1.0f : v.aps[i].rssi >= -75 ? 0.7f : 0.4f;
    static const float k[] = {1.0f, 0.75f, 0.5f, 0.25f};
    for (int d = -3; d <= 3; d++) { int c2 = ch + d; if (c2 >= 1 && c2 <= 13) load[c2] += wgt * k[d < 0 ? -d : d]; }
  }
  int best = 1; const int cand[] = {1, 6, 11}; for (int c : cand) if (load[c] < load[best]) best = c;
  if (load_out) memcpy(load_out, load, sizeof load);
  if (n5_out) *n5_out = n5;
  return best;
}

// ------------------------------------------------------------------ launcher
// Page 0 "Today" (three live widgets, different per profile), pages 1-2 app grid.
struct Home : Page {
  using Page::Page;
  lv_obj_t *strip = nullptr, *dots[3] = {nullptr}, *wdg[3] = {nullptr}; lv_obj_t* tiles[APP_COUNT] = {nullptr};
  static const int PAGES = 3, CELL = (W - 2 * M) / 3, TILE = 88, STRIP_H = H - BAR_H - 34;
  bool home() const override { return true; }
  bool holdable() const override { return ui.home_page > 0; }
  static void scrolled(lv_event_t* e) { Home* h = (Home*)lv_event_get_user_data(e); h->on_scroll(); }
  void on_scroll() {
    int p = (lv_obj_get_scroll_x(strip) + W / 2) / W; if (p < 0) p = 0; if (p >= PAGES) p = PAGES - 1;
    if (p != ui.home_page) { ui.home_page = p; ui.home_sel = 0; focus(); }
    dots_update();
  }
  void build() override {
    scr = page_root(ui);
    strip = lv_obj_create(scr); lv_obj_remove_style_all(strip);
    lv_obj_set_pos(strip, 0, BAR_H); lv_obj_set_size(strip, W, STRIP_H);
    lv_obj_set_scroll_dir(strip, LV_DIR_HOR); lv_obj_set_scroll_snap_x(strip, LV_SCROLL_SNAP_START);
    lv_obj_add_flag(strip, LV_OBJ_FLAG_SCROLL_ONE); lv_obj_set_scrollbar_mode(strip, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(strip, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_event_cb(strip, scrolled, LV_EVENT_SCROLL_END, this);
    for (int p = 0; p < PAGES; p++) { lv_obj_t* pg = box(strip, p * W, 0, W, STRIP_H, BG, 0); lv_obj_set_style_bg_opa(pg, LV_OPA_TRANSP, 0); lv_obj_add_flag(pg, LV_OBJ_FLAG_EVENT_BUBBLE); }
    today(lv_obj_get_child(strip, 0));
    int n; const int* order = launcher_order(profile(), n);
    for (int i = 0; i < n; i++) {
      lv_obj_t* pg = lv_obj_get_child(strip, 1 + i / 9); int k = i % 9, x = M + (k % 3) * CELL + (CELL - TILE) / 2, y = 14 + (k / 3) * 128;
      lv_obj_t* t = app_tile(pg, x, y, TILE, order[i]); tap_id(t, 2000 + order[i]); tiles[i] = t;
      text_w(pg, M + (k % 3) * CELL, y + TILE + 12, CELL, app_info(order[i]).name, theme().mono_data ? M12 : F14, theme().mono_data ? DIM : TXT, LV_TEXT_ALIGN_CENTER);
    }
    for (int i = 0; i < PAGES; i++) dots[i] = box(scr, 0, H - 30, 8, 8, FAINT, 4);
    lv_obj_update_layout(strip);
    lv_obj_scroll_to_x(strip, ui.home_page * W, LV_ANIM_OFF);
    dots_update(); focus(false); refresh(~0u);
  }
  void dots_update() {
    int x = W / 2 - (PAGES * 14 + 10) / 2;
    for (int i = 0; i < PAGES; i++) { bool a = i == ui.home_page; lv_obj_set_pos(dots[i], x, H - 30); lv_obj_set_width(dots[i], a ? 22 : 8); lv_obj_set_style_bg_color(dots[i], C(a ? theme().acc : FAINT), 0); x += a ? 30 : 14; }
  }
  void focus(bool animate = true) {
    int n; launcher_order(profile(), n);
    for (int i = 0; i < n; i++) { lv_obj_set_style_outline_width(tiles[i], 0, 0); lv_obj_set_style_transform_scale(tiles[i], 256, 0); }
    for (int i = 0; i < 3; i++) if (wdg[i]) lv_obj_set_style_outline_width(wdg[i], 0, 0);
    lv_obj_t* f = nullptr;
    if (ui.home_page == 0) f = wdg[ui.home_sel < 3 ? ui.home_sel : 0];
    else { int i = (ui.home_page - 1) * 9 + ui.home_sel; if (i < n) f = tiles[i]; }
    if (!f) return;
    lv_obj_set_style_outline_width(f, 3, 0); lv_obj_set_style_outline_color(f, C(ui.home_page == 0 ? theme().acc : TXT), 0); lv_obj_set_style_outline_pad(f, 3, 0);
    if (ui.home_page > 0 && animate) anim_scale(f, 232, 262, 260);
    else if (ui.home_page > 0) lv_obj_set_style_transform_scale(f, 262, 0);
  }
  // ---- Today widgets
  void today(lv_obj_t* pg) {
    int y = 10;
    for (int i = 0; i < 3; i++) { wdg[i] = card(pg, M, y, W - 2 * M, 122); tap_id(wdg[i], 3000 + i); y += 132; }
  }
  int widget_app(int i) const {
    static const int hack[] = {APP_SWEEP, APP_WIFI, APP_CONSOLE}, adv[] = {APP_WEATHER, APP_NAVIGATE, APP_MESH};
    return (profile() ? adv : hack)[i];
  }
  void draw_widgets() {
    const Vm& v = ui.vm; char b[80]; const Theme& t = theme();
    for (int i = 0; i < 3; i++) lv_obj_clean(wdg[i]);
    int cw = W - 2 * M;
    if (!profile()) { // Hackerspace
      lv_obj_t* c = wdg[0]; section(c, 10, "rf floor"); lv_obj_set_x(lv_obj_get_child(c, 0), 14);
      if (v.sled_linked && v.have_rf) {
        snprintf(b, sizeof b, "%.1f", v.rf.dbm); text(c, 14, 34, b, M28, v.rf.state == corr::UNEXPLAINED ? VIOLET : TXT); text(c, 120, 46, "dBm", M12, DIM);
        const char* st = v.rf.state == corr::UNEXPLAINED ? "UNEXPLAINED" : v.rf.state == corr::EXPLAINED ? "EXPLAINED" : "QUIET";
        lv_obj_t* p = pill(c, 0, 8, st, v.rf.state == corr::UNEXPLAINED ? VIOLET : v.rf.state == corr::EXPLAINED ? GREEN : CARD_HI, v.rf.state == corr::QUIET ? DIM : 0x000000, M12);
        lv_obj_update_layout(p); lv_obj_set_x(p, cw - 12 - lv_obj_get_width(p));
        int n = v.rf_n < 60 ? v.rf_n : 60, x0 = 14, bw = (cw - 28) / 60;
        for (int k = 0; k < n; k++) { float d = v.rf_hist[v.rf_n - n + k]; int h = (int)((d + 75) * 34 / 60); if (h < 2) h = 2; if (h > 34) h = 34; box(c, x0 + (60 - n + k) * bw, 112 - h, bw - 1, h, k == n - 1 ? t.acc : 0x1F4A30, 1); }
      } else { text(c, 14, 36, v.sled_linked ? "waiting for samples" : "sled offline", M20, DIM); text(c, 14, 70, "attach the UNO Q sled to measure", M12, MUTE); }
      c = wdg[1]; section(c, 10, "air"); lv_obj_set_x(lv_obj_get_child(c, 0), 14);
      snprintf(b, sizeof b, "%d wifi  %d ble", v.n_ap, v.n_ble); text(c, 14, 34, b, M20, TXT);
      if (v.n_ap) {
        snprintf(b, sizeof b, "top  %.18s  %d dBm", v.aps[0].ssid[0] ? v.aps[0].ssid : "(hidden)", v.aps[0].rssi); text_w(c, 14, 66, cw - 28, b, M12, DIM);
        snprintf(b, sizeof b, "best 2.4 GHz channel  %d", best_channel(v)); text(c, 14, 88, b, M12, t.acc);
      } else text(c, 14, 66, "open Wi-Fi to start scanning", M12, MUTE);
      c = wdg[2]; section(c, 10, "links"); lv_obj_set_x(lv_obj_get_child(c, 0), 14);
      struct { const char* k; char v[28]; bool ok; } L[4];
      snprintf(L[0].v, 28, "%s", v.sled_linked ? "linked" : "--"); L[0].k = "sled"; L[0].ok = v.sled_linked;
      snprintf(L[1].v, 28, v.lora.ok ? "node %04X" : "--", v.lora.node_id); L[1].k = "lora"; L[1].ok = v.lora.ok;
      snprintf(L[2].v, 28, "%.27s", v.net.state == NET_ONLINE ? v.net.ssid : v.net.ap ? "hotspot" : "off"); L[2].k = "wifi"; L[2].ok = v.net.state == NET_ONLINE || v.net.ap;
      snprintf(L[3].v, 28, "%s", !v.sd ? "no card" : v.settings.logging ? "logging" : "mounted"); L[3].k = "sd"; L[3].ok = v.sd;
      for (int k = 0; k < 4; k++) { int x = 14 + (k % 2) * (cw / 2), y = 36 + (k / 2) * 38;
        box(c, x, y + 6, 8, 8, L[k].ok ? t.acc : FAINT, 4); text(c, x + 16, y, L[k].k, M12, DIM); text_w(c, x + 16, y + 16, cw / 2 - 40, L[k].v, M14, TXT); }
    } else { // Adventure
      lv_obj_t* c = wdg[0]; const Weather* wx = v.weather();
      if (wx) {
        glyph(c, wx_icon(wx->cond, false), 18, 22, t.acc2);
        fmt_temp(b, sizeof b, wx->temp_c, v.settings.metric); text(c, 72, 12, b, F48, TXT);
        text_w(c, 190, 22, cw - 204, wx->online ? wx::cond_name((wx::Cond)wx->cond) : wx->text, F16, TXT, LV_TEXT_ALIGN_RIGHT);
        if (wx->online && wx->n_days) { char hi[8], lo[8]; fmt_temp(hi, 8, wx->days[0].hi, v.settings.metric); fmt_temp(lo, 8, wx->days[0].lo, v.settings.metric); snprintf(b, sizeof b, "H %s   L %s", hi, lo); }
        else snprintf(b, sizeof b, "%.0f hPa, %s", wx->hpa, wx->trend < 0 ? "falling" : wx->trend > 0 ? "rising" : "steady");
        text_w(c, 190, 48, cw - 204, b, F14, DIM, LV_TEXT_ALIGN_RIGHT);
        text_w(c, 18, 90, cw - 36, wx->online ? "Forecast from Open-Meteo" : "Barometer forecast (offline)", F12, MUTE);
      } else { glyph(c, &wx40_partly, 18, 22, FAINT); text(c, 72, 26, "Weather", F24, DIM); text(c, 72, 60, "Reading the barometer...", F14, MUTE); }
      c = wdg[1]; int wi = v.settings.wp_target < v.n_wp ? v.settings.wp_target : v.n_wp - 1;
      if (wi >= 0 && v.gnss.valid) {
        const Waypoint& w = v.wps[wi]; double d = geo::distance_m(v.gnss.lat, v.gnss.lon, w.lat, w.lon), br = geo::bearing_deg(v.gnss.lat, v.gnss.lon, w.lat, w.lon);
        lv_obj_t* a = glyph(c, &ic20_arrow_up, 26, 30, t.acc); lv_obj_set_style_transform_pivot_x(a, 10, 0); lv_obj_set_style_transform_pivot_y(a, 10, 0);
        lv_obj_set_style_transform_scale(a, 600, 0); lv_obj_set_style_transform_rotation(a, (int32_t)(br * 10), 0);
        fmt_dist(b, sizeof b, d, v.settings.metric); text(c, 80, 14, b, F36, TXT);
        snprintf(b, sizeof b, "to %s   %s %.0f\xC2\xB0", w.name, geo::cardinal(br), br); text_w(c, 80, 62, cw - 96, b, F16, DIM);
        text(c, 80, 90, "north-up bearing", F12, MUTE);
      } else { glyph(c, &ic30_pin, 22, 26, FAINT); text(c, 72, 24, wi >= 0 ? "Waiting for GPS" : "No waypoints", F24, DIM); text(c, 72, 60, wi >= 0 ? "Needs open sky" : "Save one from Position", F14, MUTE); }
      c = wdg[2]; glyph(c, &ic30_mesh, 22, 20, v.lora.ok ? t.acc : FAINT);
      const MeshText* last = nullptr; for (int k = v.n_msg - 1; k >= 0; k--) if (!v.msgs[k].mine) { last = &v.msgs[k]; break; }
      if (last) { snprintf(b, sizeof b, "%04X  %s", last->src, last->sos ? "SOS" : ""); text(c, 66, 16, b, F16, last->sos ? RED : DIM);
        text_w(c, 66, 40, cw - 82, last->text, F24, TXT); snprintf(b, sizeof b, "%u heard   %u sent   %u%% airtime left", v.lora.rx_count, v.lora.tx_count, v.lora.budget_pct); text_w(c, 66, 84, cw - 82, b, F12, MUTE); }
      else { text(c, 66, 20, "Mesh", F24, TXT); text(c, 66, 56, v.lora.ok ? "Quiet. Messages appear here." : "Radio not responding", F14, DIM); }
    }
  }
  void refresh(uint32_t what) override { if (what & (R_RF | R_WIFI | R_BLE | R_LINK | R_NET | R_WX | R_GNSS | R_WP | R_MSG | R_SET)) draw_widgets(); }
  void move(int dp, int ds) {
    ui.haptic();
    if (dp) { ui.home_page += dp; lv_obj_scroll_to_x(strip, ui.home_page * W, LV_ANIM_ON); dots_update(); }
    ui.home_sel = ds; focus();
  }
  bool key(uint16_t k) override {
    int n; launcher_order(profile(), n); int p = ui.home_page, s = ui.home_sel;
    if (p == 0) {
      if ((k & KEY_DOWN) && s < 2) { move(0, s + 1); return true; }
      if ((k & KEY_UP) && s > 0) { move(0, s - 1); return true; }
      if (k & KEY_RIGHT) { move(1, 0); return true; }
      if (k & KEY_A) { ui.haptic(); ui.open_app(widget_app(s)); return true; }
      return false;
    }
    int cnt = n - (p - 1) * 9; if (cnt > 9) cnt = 9;
    int col = s % 3, rw = s / 3;
    if (k & KEY_RIGHT) { if (col < 2 && s + 1 < cnt) move(0, s + 1); else if (p < PAGES - 1) { int ns = rw * 3; int nc = n - p * 9; move(1, ns < nc ? ns : 0); } return true; }
    if (k & KEY_LEFT) { if (col > 0) move(0, s - 1); else if (p > 1) move(-1, rw * 3 + 2); else move(-1, rw < 3 ? rw : 0); return true; }
    if ((k & KEY_DOWN) && s + 3 < cnt) { move(0, s + 3); return true; }
    if ((k & KEY_UP) && rw > 0) { move(0, s - 3); return true; }
    if (k & KEY_A) { int i = (p - 1) * 9 + s; if (i < n) { ui.haptic(); ui.open_app(launcher_order(profile(), n)[i]); } return true; }
    return false;
  }
  void hold() override { int n; const int* o = launcher_order(profile(), n); int i = (ui.home_page - 1) * 9 + ui.home_sel; if (ui.home_page > 0 && i < n) ui.quick_actions(o[i]); }
  bool tap(int id) override {
    if (id >= 3000) { ui.haptic(); ui.open_app(widget_app(id - 3000)); return true; }
    if (id >= 2000) { ui.haptic(); ui.open_app(id - 2000); return true; }
    return false;
  }
};

// ------------------------------------------------------------------ field test (Wi-Fi / Channels / Bluetooth)
struct Field : Page {
  using Page::Page;
  lv_obj_t* body = nullptr; int sel = 0, top = 0; bool first = true;
  Mode mode() const override { return MODE_FIELD; }
  void build() override {
    scr = page_root(ui);
    static const char* tabs[] = {"Networks", "Channels", "BLE"}; static const char* titles[] = {"Wi-Fi", "Channels", "Bluetooth"};
    header(scr, titles[ui.field_tab], BLUE, tabs, 3, ui.field_tab);
    body = box(scr, 0, TOP, W, H - TOP - 30, BG, 0); lv_obj_add_flag(body, LV_OBJ_FLAG_EVENT_BUBBLE);
    hint(scr, ui.field_tab == 0 ? "Left/Right tab   A details   B back" : "Left/Right tab   B back");
    draw();
  }
  void list_row(int i, int idx, const char* name, bool dim, const char* sub, uint32_t sub_col, int rssi) {
    lv_obj_t* r = row(body, i * 63, 57, idx == sel, BLUE, idx); char b[12];
    bars(r, 14, 17, rssi);
    text_w(r, 54, 7, 226, name, F20, dim ? DIM : TXT);
    text_w(r, 54, 34, 240, sub, theme().mono_data ? M12 : F12, sub_col);
    snprintf(b, sizeof b, "%d", rssi); text_w(r, 286, 6, 78, b, DATA(24), rssi_color(rssi), LV_TEXT_ALIGN_RIGHT);
    text_w(r, 286, 36, 78, "dBm", F12, DIM, LV_TEXT_ALIGN_RIGHT);
  }
  void draw() {
    lv_obj_clean(body); const Vm& v = ui.vm; char b[80];
    if (ui.field_tab == 0) {
      if (!v.n_ap) { text_w(body, 0, 150, W, "Scanning...", F20, DIM, LV_TEXT_ALIGN_CENTER); return; }
      if (sel >= v.n_ap) sel = v.n_ap - 1;
      if (sel < top) top = sel;
      if (sel > top + 5) top = sel - 5;
      for (int i = 0; i < 6 && top + i < v.n_ap; i++) {
        const WifiAp& a = v.aps[top + i];
        snprintf(b, sizeof b, "ch %d   %s   %s", a.channel, a.channel > 14 ? "5 GHz" : "2.4 GHz", auth_name(a.auth));
        list_row(i, top + i, a.ssid[0] ? a.ssid : "(hidden network)", !a.ssid[0], b, a.auth == AUTH_OPEN ? AMBER : DIM, a.rssi);
      }
      snprintf(b, sizeof b, "%d / %d", sel + 1, v.n_ap); text_w(scr, W - 96, H - 26, 80, b, M12, DIM, LV_TEXT_ALIGN_RIGHT);
    } else if (ui.field_tab == 1) {
      float load[14]; int n5; int best = best_channel(v, load, &n5);
      float mx = 1; for (int c = 1; c <= 13; c++) if (load[c] > mx) mx = load[c];
      lv_obj_t* cd = card(body, M, 0, W - 2 * M, 250);
      text(cd, 16, 12, "2.4 GHz channel load", F16, TXT);
      int bw = 20, gap = 6, x0 = (W - 2 * M - (13 * bw + 12 * gap)) / 2, base = 206;
      for (int c = 1; c <= 13; c++) {
        int h = 6 + (int)(150 * load[c] / mx); float rel = load[c] / mx;
        lv_obj_t* bar = box(cd, x0 + (c - 1) * (bw + gap), base - h, bw, h, c == best ? GREEN : (rel > 0.66f ? RED : rel > 0.33f ? AMBER : BLUE), 6);
        if (first) anim_y(bar, base, base - h, 380 + c * 20); // bars rise in on open
        snprintf(b, sizeof b, "%d", c); text_w(cd, x0 + (c - 1) * (bw + gap) - 4, base + 8, bw + 8, b, M12, c == best ? GREEN : DIM, LV_TEXT_ALIGN_CENTER);
      }
      lv_obj_t* c2 = card(body, M, 262, W - 2 * M, 58);
      snprintf(b, sizeof b, "Least congested: channel %d", best); text(c2, 16, 8, b, F20, GREEN);
      text(c2, 16, 34, "of the non-overlapping set 1 / 6 / 11", F12, DIM);
      lv_obj_t* c3 = card(body, M, 330, W - 2 * M, 46);
      snprintf(b, sizeof b, "5 GHz: %d network%s      2.4 GHz: %d", n5, n5 == 1 ? "" : "s", v.n_ap - n5); text(c3, 16, 13, b, F16, TXT);
      if (v.n_ap) first = false;
    } else {
      if (!v.n_ble) { text_w(body, 0, 150, W, "Scanning...", F20, DIM, LV_TEXT_ALIGN_CENTER); return; }
      if (sel >= v.n_ble) sel = v.n_ble - 1;
      if (sel < top) top = sel;
      if (sel > top + 5) top = sel - 5;
      for (int i = 0; i < 6 && top + i < v.n_ble; i++) {
        const BleDev& d = v.bles[top + i];
        snprintf(b, sizeof b, "%02X:%02X:%02X:%02X:%02X:%02X", d.addr[0], d.addr[1], d.addr[2], d.addr[3], d.addr[4], d.addr[5]);
        list_row(i, top + i, d.name[0] ? d.name : "(no name)", !d.name[0], b, DIM, d.rssi);
      }
    }
  }
  void refresh(uint32_t what) override { if (what & (R_WIFI | R_BLE)) draw(); }
  bool key(uint16_t k) override {
    int n = ui.field_tab == 0 ? ui.vm.n_ap : ui.field_tab == 2 ? ui.vm.n_ble : 0;
    if (k & KEY_B) { ui.go(PG_HOME, -1); return true; }
    if (k & (KEY_LEFT | KEY_RIGHT)) { ui.field_tab = (ui.field_tab + ((k & KEY_RIGHT) ? 1 : 2)) % 3; ui.haptic(); ui.go(PG_FIELD, 0); return true; }
    if ((k & KEY_DOWN) && sel < n - 1) { sel++; ui.haptic(); draw(); return true; }
    if ((k & KEY_UP) && sel > 0) { sel--; ui.haptic(); draw(); return true; }
    if ((k & KEY_A) && ui.field_tab == 0 && n) { memcpy(ui.ap_bssid, ui.vm.aps[sel].bssid, 6); ui.haptic(); ui.go(PG_AP, 1); return true; }
    return false;
  }
  bool tap(int id) override {
    if (id >= 1000) { ui.field_tab = id - 1000; ui.haptic(); ui.go(PG_FIELD, 0); return true; }
    sel = id; return key(KEY_A);
  }
};

struct ApDetail : Page {
  using Page::Page;
  lv_obj_t *l_rssi = nullptr, *l_q = nullptr, *chart = nullptr; lv_chart_series_t* ser = nullptr;
  Mode mode() const override { return MODE_FIELD; }
  const WifiAp* find() { for (int i = 0; i < ui.vm.n_ap; i++) if (!memcmp(ui.vm.aps[i].bssid, ui.ap_bssid, 6)) return &ui.vm.aps[i]; return nullptr; }
  void build() override {
    scr = page_root(ui); const WifiAp* a = find(); char b[80];
    text_w(scr, M + 2, BAR_H + 10, W - 2 * M, a && a->ssid[0] ? a->ssid : "(hidden network)", F28, TXT);
    lv_obj_t* c = card(scr, M, TOP, W - 2 * M, 112);
    l_rssi = text(c, 18, 12, "--", theme().mono_data ? M28 : F48, TXT); text(c, 20, 74, "dBm", F16, DIM);
    if (theme().mono_data) lv_obj_set_style_transform_scale(l_rssi, 380, 0), lv_obj_set_style_transform_pivot_x(l_rssi, 0, 0), lv_obj_set_style_transform_pivot_y(l_rssi, 0, 0);
    l_q = text_w(c, 170, 20, 190, "", F24, TXT, LV_TEXT_ALIGN_RIGHT);
    text_w(c, 170, 56, 190, "signal quality", F14, DIM, LV_TEXT_ALIGN_RIGHT);
    lv_obj_t* g = card(scr, M, TOP + 122, W - 2 * M, 150);
    text(g, 16, 10, "Signal over time", F14, DIM);
    chart = lv_chart_create(g); lv_obj_remove_style_all(chart); lv_obj_set_pos(chart, 14, 36); lv_obj_set_size(chart, W - 2 * M - 28, 100);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE); lv_chart_set_point_count(chart, 40); lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -95, -30);
    lv_chart_set_div_line_count(chart, 3, 0); lv_obj_set_style_line_color(chart, C(FAINT), LV_PART_MAIN); lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 3, LV_PART_ITEMS); lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
    ser = lv_chart_add_series(chart, C(theme().mono_data ? theme().acc : BLUE), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_t* i = card(scr, M, TOP + 282, W - 2 * M, 94);
    if (a) {
      snprintf(b, sizeof b, "%02X:%02X:%02X:%02X:%02X:%02X", a->bssid[0], a->bssid[1], a->bssid[2], a->bssid[3], a->bssid[4], a->bssid[5]);
      text(i, 16, 10, "BSSID", F12, DIM); text(i, 110, 8, b, M14, TXT);
      int mhz = a->channel <= 13 ? 2407 + 5 * a->channel : (a->channel == 14 ? 2484 : 5000 + 5 * a->channel);
      snprintf(b, sizeof b, "%d   %d MHz", a->channel, mhz); text(i, 16, 38, "Channel", F12, DIM); text(i, 110, 36, b, M14, TXT);
      text(i, 16, 66, "Security", F12, DIM); text(i, 110, 64, auth_name(a->auth), F16, a->auth == AUTH_OPEN ? AMBER : TXT);
    }
    hint(scr, "Walk around: the trace shows where the signal is strongest.   B back");
    refresh(R_WIFI);
  }
  void refresh(uint32_t what) override {
    if (!(what & R_WIFI)) return;
    const WifiAp* a = find(); char b[16];
    if (!a) { lv_label_set_text(l_rssi, "--"); lv_label_set_text(l_q, "Out of range"); lv_obj_set_style_text_color(l_q, C(DIM), 0); }
    else {
      snprintf(b, sizeof b, "%d", a->rssi); lv_label_set_text(l_rssi, b); lv_obj_set_style_text_color(l_rssi, C(rssi_color(a->rssi)), 0);
      lv_label_set_text(l_q, a->rssi >= -55 ? "Excellent" : a->rssi >= -65 ? "Good" : a->rssi >= -75 ? "Fair" : "Weak"); lv_obj_set_style_text_color(l_q, C(rssi_color(a->rssi)), 0);
    }
    for (int i = 0; i < ui.vm.n_hist; i++) if (!memcmp(ui.vm.hist[i].bssid, ui.ap_bssid, 6)) {
      const ApHist& h = ui.vm.hist[i];
      for (int k = 0; k < 40; k++) lv_chart_set_value_by_id(chart, ser, (uint32_t)k, k < 40 - h.n ? LV_CHART_POINT_NONE : h.rssi[k - (40 - h.n)]);
      lv_chart_refresh(chart);
    }
  }
  bool key(uint16_t k) override { if (k & KEY_B) { ui.go(PG_FIELD, -1); return true; } return false; }
};

// ------------------------------------------------------------------ sweep
struct Sweep : Page {
  using Page::Page;
  lv_obj_t *p_state = nullptr, *l_dbm = nullptr, *l_sub = nullptr, *meter = nullptr, *m_floor = nullptr, *m_exp = nullptr, *chart = nullptr, *l_known = nullptr, *l_src = nullptr;
  lv_chart_series_t *s_lvl = nullptr, *s_exp = nullptr; int last_state = -1, meter_w = 14;
  static const int LO = -75, HI = -15, MW = W - 2 * M - 32;
  Mode mode() const override { return MODE_SWEEP; }
  void build() override {
    scr = page_root(ui); header(scr, "Sweep", VIOLET, nullptr, 0, 0);
    p_state = pill(scr, 0, BAR_H + 13, "NO SLED", CARD, DIM, F14);
    lv_obj_t* c = card(scr, M, TOP, W - 2 * M, 150);
    l_dbm = text(c, 16, 6, "--.-", theme().mono_data ? M28 : F48, TXT); text(c, 18, 64, "dBm broadband", F14, DIM);
    if (theme().mono_data) { lv_obj_set_style_transform_scale(l_dbm, 400, 0); lv_obj_set_style_transform_pivot_x(l_dbm, 0, 0); lv_obj_set_style_transform_pivot_y(l_dbm, 0, 0); lv_obj_set_y(l_dbm, 8); }
    l_sub = text_w(c, 170, 16, W - 2 * M - 186, "", F14, DIM, LV_TEXT_ALIGN_RIGHT); lv_label_set_long_mode(l_sub, LV_LABEL_LONG_WRAP); lv_obj_set_height(l_sub, 60);
    box(c, 16, 100, MW, 14, FAINT, 7);
    meter = box(c, 16, 100, 14, 14, VIOLET, 7);
    m_floor = box(c, 16, 94, 3, 26, DIM, 1); m_exp = box(c, 16, 94, 3, 26, GREEN, 1);
    text(c, 16, 124, "-75", M12, DIM); text_w(c, 16 + MW - 40, 124, 40, "-15", M12, DIM, LV_TEXT_ALIGN_RIGHT);
    text_w(c, 60, 124, MW - 120, "grey: noise floor   green: expected", F12, MUTE, LV_TEXT_ALIGN_CENTER);
    lv_obj_t* g = card(scr, M, TOP + 160, W - 2 * M, 150);
    text(g, 16, 10, "Last 30 s", F14, DIM);
    chart = lv_chart_create(g); lv_obj_remove_style_all(chart); lv_obj_set_pos(chart, 14, 34); lv_obj_set_size(chart, W - 2 * M - 28, 104);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE); lv_chart_set_point_count(chart, 120); lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, LO, HI);
    lv_chart_set_div_line_count(chart, 4, 0); lv_obj_set_style_line_color(chart, C(FAINT), LV_PART_MAIN); lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS); lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
    s_exp = lv_chart_add_series(chart, C(GREEN), LV_CHART_AXIS_PRIMARY_Y); s_lvl = lv_chart_add_series(chart, C(VIOLET), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_t* f = card(scr, M, TOP + 320, W - 2 * M, 62);
    l_known = text(f, 16, 9, "", F16, TXT); l_src = text_w(f, 16, 35, W - 2 * M - 32, "", F12, DIM);
    hint(scr, "Shows how much RF energy is present, not what it is.");
    refresh(~0u);
  }
  void refresh(uint32_t what) override {
    const Vm& v = ui.vm; char b[96];
    if (what & (R_WIFI | R_BLE | R_LINK | R_RF)) {
      snprintf(b, sizeof b, "Known emitters: %d Wi-Fi, %d BLE", v.n_ap, v.n_ble); lv_label_set_text(l_known, b);
      lv_label_set_text(l_src, !v.sled_linked ? "Attach the UNO Q sled to measure" : v.rf.from_sled_verdict ? "Compared on the sled (UNO Q Linux side)" : "Compared on the base (sled Linux side not reporting)");
    }
    if (!v.sled_linked || !v.have_rf) {
      if (last_state != 9) { last_state = 9; set_pill(p_state, v.sled_linked ? "WAITING" : "NO SLED", CARD, AMBER); lv_obj_update_layout(p_state); lv_obj_set_x(p_state, W - M - lv_obj_get_width(p_state)); }
      lv_label_set_text(l_dbm, "--.-"); lv_obj_set_style_text_color(l_dbm, C(DIM), 0); lv_label_set_text(l_sub, ""); lv_obj_set_width(meter, 14); meter_w = 14;
      lv_obj_set_style_bg_color(meter, C(FAINT), 0); lv_obj_add_flag(m_floor, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(m_exp, LV_OBJ_FLAG_HIDDEN);
      lv_chart_set_all_value(chart, s_lvl, LV_CHART_POINT_NONE); lv_chart_set_all_value(chart, s_exp, LV_CHART_POINT_NONE); lv_chart_refresh(chart); return;
    }
    if (!(what & R_RF)) return;
    const RfReading& r = v.rf;
    snprintf(b, sizeof b, "%.1f", r.dbm); lv_label_set_text(l_dbm, b);
    uint32_t col = r.state == corr::UNEXPLAINED ? VIOLET : r.state == corr::EXPLAINED ? GREEN : GREY;
    if ((int)r.state != last_state) {
      uint32_t prev = last_state == corr::UNEXPLAINED ? VIOLET : last_state == corr::EXPLAINED ? GREEN : GREY;
      last_state = r.state;
      set_pill(p_state, r.state == corr::UNEXPLAINED ? "UNEXPLAINED" : r.state == corr::EXPLAINED ? "EXPLAINED" : "QUIET", col, 0x000000);
      lv_obj_update_layout(p_state); lv_obj_set_x(p_state, W - M - lv_obj_get_width(p_state)); anim_scale(p_state, 220, 256, 300);
      lv_obj_set_style_text_color(l_dbm, C(r.state == corr::QUIET ? TXT : col), 0); anim_bg(meter, prev, col, 300);
      if (r.state == corr::UNEXPLAINED) ui.haptic(HAP_DOUBLE);
    }
    if (r.state == corr::UNEXPLAINED) snprintf(b, sizeof b, "+%.0f dB above what the visible Wi-Fi/BLE explains", r.excess_db);
    else if (r.state == corr::EXPLAINED) snprintf(b, sizeof b, "Consistent with the visible Wi-Fi/BLE activity");
    else snprintf(b, sizeof b, "At the noise floor");
    lv_label_set_text(l_sub, b);
    auto px = [](float d) { float k = (d - LO) / (float)(HI - LO); if (k < 0) k = 0; if (k > 1) k = 1; return (int)(k * MW); };
    int w = px(r.dbm); if (w < 14) w = 14;
    if (w != meter_w) { anim_w(meter, meter_w, w, 140); meter_w = w; } // eased meter, no jumps between samples
    lv_obj_set_x(m_floor, 16 + px(r.floor_dbm)); lv_obj_set_x(m_exp, 16 + px(r.expected_dbm));
    lv_obj_remove_flag(m_floor, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(m_exp, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 120; i++) {
      bool has = i >= 120 - v.rf_n; int k = i - (120 - v.rf_n);
      lv_chart_set_value_by_id(chart, s_lvl, (uint32_t)i, has ? (int32_t)v.rf_hist[k] : LV_CHART_POINT_NONE);
      lv_chart_set_value_by_id(chart, s_exp, (uint32_t)i, has ? (int32_t)v.rf_exp[k] : LV_CHART_POINT_NONE);
    }
    lv_chart_refresh(chart);
  }
  bool key(uint16_t k) override { if (k & KEY_B) { ui.go(PG_HOME, -1); return true; } return false; }
};

// ------------------------------------------------------------------ position / waypoints / mesh
struct Beacon : Page {
  using Page::Page;
  lv_obj_t* body = nullptr; int sel = 0;
  Mode mode() const override { return MODE_BEACON; }
  void build() override {
    scr = page_root(ui);
    static const char* tabs[] = {"Position", "Waypoints", "Mesh"}; static const char* titles[] = {"Position", "Navigate", "Mesh"};
    header(scr, titles[ui.beacon_tab], GREEN, tabs, 3, ui.beacon_tab);
    body = box(scr, 0, TOP, W, H - TOP - 30, BG, 0); lv_obj_add_flag(body, LV_OBJ_FLAG_EVENT_BUBBLE);
    static const char* hints[] = {"A save waypoint   Left/Right tab   B back", "A navigate   Left/Right tab   B back", "A quick reply   Left/Right tab   B back"};
    hint(scr, hints[ui.beacon_tab]);
    draw();
  }
  void draw() {
    lv_obj_clean(body); const Vm& v = ui.vm; char b[96]; const Theme& t = theme();
    if (ui.beacon_tab == 0) {
      const GnssFix& g = v.gnss;
      lv_obj_t* c = card(body, M, 0, W - 2 * M, 150);
      if (g.valid) snprintf(b, sizeof b, "%s FIX   %d sats", g.fix_type == 3 ? "3D" : "2D", g.sats); else snprintf(b, sizeof b, "SEARCHING   %d sats", g.sats);
      pill(c, 14, 12, b, g.valid ? GREEN : AMBER, 0x000000, M12);
      if (g.valid) {
        snprintf(b, sizeof b, "%.5f\xC2\xB0 %c", fabs(g.lat), g.lat >= 0 ? 'N' : 'S'); text(c, 16, 48, b, t.mono_data ? M28 : F36, TXT);
        snprintf(b, sizeof b, "%.5f\xC2\xB0 %c", fabs(g.lon), g.lon >= 0 ? 'E' : 'W'); text(c, 16, 94, b, t.mono_data ? M28 : F36, TXT);
      } else { text(c, 16, 54, "Waiting for satellites", F24, DIM); text(c, 16, 92, "Needs open sky. First fix can take a minute.", F14, DIM); }
      const char* lab[] = {"Altitude", "Accuracy", "Speed"}; char val[3][24];
      if (g.valid) {
        if (v.settings.metric) { snprintf(val[0], 24, "%.0f m", g.alt_m); snprintf(val[1], 24, "+/-%.1f m", g.hacc_m); snprintf(val[2], 24, "%.1f km/h", g.speed_mps * 3.6f); }
        else { snprintf(val[0], 24, "%.0f ft", g.alt_m * 3.28084f); snprintf(val[1], 24, "+/-%.0f ft", g.hacc_m * 3.28084f); snprintf(val[2], 24, "%.1f mph", g.speed_mps * 2.23694f); }
      } else for (auto& s : val) snprintf(s, 24, "--");
      int cw = (W - 2 * M - 16) / 3;
      for (int i = 0; i < 3; i++) { lv_obj_t* k = card(body, M + i * (cw + 8), 160, cw, 70); text(k, 12, 10, lab[i], F12, DIM); text_w(k, 12, 32, cw - 16, val[i], DATA(20), TXT); }
      lv_obj_t* l = card(body, M, 240, W - 2 * M, 70);
      snprintf(b, sizeof b, "LoRa mesh   node %04X", v.lora.node_id); text(l, 16, 10, b, F16, v.lora.ok ? TXT : RED);
      if (v.lora.ok) snprintf(b, sizeof b, "%.1f MHz   sent %u  heard %u   airtime %u%%", lora::CHANNELS[v.lora.channel % lora::N_CHANNELS], v.lora.tx_count, v.lora.rx_count, v.lora.budget_pct);
      else snprintf(b, sizeof b, "Radio not responding");
      text_w(l, 16, 38, W - 2 * M - 32, b, M12, DIM);
      lv_obj_t* s = box(body, M, 320, W - 2 * M, 56, v.sos ? RED : t.card, 18);
      if (!v.sos && t.border_w) { lv_obj_set_style_border_width(s, 1, 0); lv_obj_set_style_border_color(s, C(0x4A1A16), 0); }
      glyph(s, &ic30_sos, 14, 13, v.sos ? TXT : RED);
      text(s, 56, 8, v.sos ? "SOS ACTIVE" : "SOS", F20, v.sos ? TXT : RED);
      text_w(s, 56, 34, W - 2 * M - 70, v.sos ? "Position sent every 20 s. Hold SOS 2 s to cancel." : "Hold the SOS button 2 s. Broadcasts position to the mesh.", F12, v.sos ? TXT : DIM);
    } else if (ui.beacon_tab == 1) {
      if (!v.n_wp) { text_w(body, 0, 130, W, "No waypoints yet", F20, DIM, LV_TEXT_ALIGN_CENTER); text_w(body, 0, 162, W, "Save one from the Position tab", F14, DIM, LV_TEXT_ALIGN_CENTER); return; }
      if (sel >= v.n_wp) sel = v.n_wp - 1;
      int top = sel > 5 ? sel - 5 : 0;
      for (int i = 0; i < 6 && top + i < v.n_wp; i++) {
        const Waypoint& w = v.wps[top + i]; lv_obj_t* r = row(body, i * 63, 57, top + i == sel, GREEN, top + i);
        glyph(r, &ic20_pin, 16, 18, (int)v.settings.wp_target == top + i ? t.acc : DIM);
        text_w(r, 46, 7, 180, w.name, F20, TXT);
        snprintf(b, sizeof b, "%.5f, %.5f", w.lat, w.lon); text_w(r, 46, 34, 190, b, M12, DIM);
        if (v.gnss.valid) {
          char d[24]; fmt_dist(d, sizeof d, geo::distance_m(v.gnss.lat, v.gnss.lon, w.lat, w.lon), v.settings.metric);
          text_w(r, 230, 7, 134, d, DATA(20), GREEN, LV_TEXT_ALIGN_RIGHT);
          double br = geo::bearing_deg(v.gnss.lat, v.gnss.lon, w.lat, w.lon); snprintf(b, sizeof b, "%s  %.0f\xC2\xB0", geo::cardinal(br), br);
          text_w(r, 230, 34, 134, b, M12, DIM, LV_TEXT_ALIGN_RIGHT);
        }
      }
    } else {
      if (!v.n_msg) { text_w(body, 0, 130, W, "No messages yet", F20, DIM, LV_TEXT_ALIGN_CENTER); text_w(body, 0, 162, W, "Press A to send a quick reply", F14, DIM, LV_TEXT_ALIGN_CENTER); return; }
      int y = H - TOP - 34;
      for (int i = v.n_msg - 1; i >= 0 && y > 60; i--) { // newest at the bottom
        const MeshText& m = v.msgs[i]; int bw = 296; y -= 64;
        lv_obj_t* r = box(body, m.mine ? W - M - bw : M, y, bw, 58, m.sos ? RED : (m.mine ? (profile() ? 0x5A3A0A : 0x0E4A2A) : t.card), 18);
        text_w(r, 14, 8, bw - 28, m.text, F16, TXT);
        if (m.mine) snprintf(b, sizeof b, "You");
        else if (m.has_pos) snprintf(b, sizeof b, "%04X   %.4f, %.4f   batt %u%%", m.src, m.lat, m.lon, m.batt);
        else snprintf(b, sizeof b, "%04X   %u hop%s   %.0f dBm", m.src, m.hops + 1, m.hops ? "s" : "", m.rssi);
        text_w(r, 14, 34, bw - 28, b, M12, m.sos ? TXT : DIM);
      }
    }
  }
  void refresh(uint32_t what) override {
    if ((ui.beacon_tab == 0 && (what & (R_GNSS | R_LINK))) || (ui.beacon_tab == 1 && (what & (R_WP | R_GNSS | R_SET))) || (ui.beacon_tab == 2 && (what & R_MSG))) draw();
  }
  bool key(uint16_t k) override {
    if (k & KEY_B) { ui.go(PG_HOME, -1); return true; }
    if (k & (KEY_LEFT | KEY_RIGHT)) { ui.beacon_tab = (ui.beacon_tab + ((k & KEY_RIGHT) ? 1 : 2)) % 3; ui.haptic(); ui.go(PG_BEACON, 0); return true; }
    if (ui.beacon_tab == 0 && (k & KEY_A)) { ui.haptic(HAP_DOUBLE); ui.cmd(Cmd(CMD_WP_SAVE)); return true; }
    if (ui.beacon_tab == 1) {
      if ((k & KEY_DOWN) && sel < ui.vm.n_wp - 1) { sel++; ui.haptic(); draw(); return true; }
      if ((k & KEY_UP) && sel > 0) { sel--; ui.haptic(); draw(); return true; }
      if ((k & KEY_A) && ui.vm.n_wp) {
        ui.nav_target = sel; ui.nav_active = true; Settings s = ui.vm.settings; s.wp_target = (uint8_t)sel; ui.apply(s);
        ui.haptic(); ui.go(PG_NAV, 1); return true;
      }
    }
    if (ui.beacon_tab == 2 && (k & KEY_A)) { ui.haptic(); ui.go(PG_REPLY, 1); return true; }
    return false;
  }
  bool tap(int id) override {
    if (id >= 1000) { ui.beacon_tab = id - 1000; ui.haptic(); ui.go(PG_BEACON, 0); return true; }
    sel = id; return key(KEY_A);
  }
};

struct Nav : Page { // return-to-waypoint, needle eased toward the target bearing every frame
  using Page::Page;
  lv_obj_t *l_dist = nullptr, *l_brg = nullptr, *l_turn = nullptr, *needle = nullptr, *dial = nullptr; lv_point_precise_t pts[5];
  double target = 0, shown = 0; bool have = false, arrived = false;
  Mode mode() const override { return MODE_BEACON; }
  void build() override {
    scr = page_root(ui); const Vm& v = ui.vm; const char* name = ui.nav_target < v.n_wp ? v.wps[ui.nav_target].name : "?";
    char b[40]; snprintf(b, sizeof b, "To %s", name); text_w(scr, M + 2, BAR_H + 10, W - 2 * M, b, F28, TXT);
    int cx = W / 2, cy = TOP + 118, r = 104;
    dial = box(scr, cx - r, cy - r, 2 * r, 2 * r, theme().card, 999);
    lv_obj_set_style_border_width(dial, 3, 0); lv_obj_set_style_border_color(dial, C(theme().border_w ? theme().border : FAINT), 0);
    for (int i = 0; i < 12; i++) { double a = i * 0.5235987755982988; int rr = i % 3 ? 92 : 88; box(dial, (int)(r + rr * sin(a)) - 2, (int)(r - rr * cos(a)) - 2, 4, 4, i % 3 ? FAINT : DIM, 2); }
    text_w(dial, 0, 14, 2 * r, "ahead", F12, DIM, LV_TEXT_ALIGN_CENTER);
    needle = lv_line_create(dial); lv_obj_set_style_line_width(needle, 10, 0); lv_obj_set_style_line_color(needle, C(theme().acc), 0); lv_obj_set_style_line_rounded(needle, true, 0);
    l_dist = text_w(scr, 0, TOP + 236, W, "--", theme().mono_data ? M28 : F48, TXT, LV_TEXT_ALIGN_CENTER);
    l_brg = text_w(scr, 0, TOP + 296, W, "", DATA(20), theme().acc, LV_TEXT_ALIGN_CENTER);
    l_turn = text_w(scr, 0, TOP + 328, W, "", F14, DIM, LV_TEXT_ALIGN_CENTER);
    hint(scr, "Arrow follows your GPS course.   A end   B keep in background");
    refresh(R_GNSS);
  }
  void place(double deg) {
    double a = deg * 0.017453292519943295; int c = 104;
    auto P = [&](double rr, double off) { lv_point_precise_t p; p.x = (lv_value_precise_t)(c + rr * sin(a + off)); p.y = (lv_value_precise_t)(c - rr * cos(a + off)); return p; };
    pts[0] = P(-56, 0); pts[1] = P(74, 0); pts[2] = P(44, -0.50); pts[3] = P(74, 0); pts[4] = P(44, 0.50);
    lv_line_set_points(needle, pts, 5);
  }
  void tick() override {
    if (!have) return;
    double d = fmod(target - shown + 540.0, 360.0) - 180.0;  // shortest way round
    if (fabs(d) < 0.3) return;
    shown += d * 0.18; place(shown);
  }
  void refresh(uint32_t what) override {
    if (!(what & R_GNSS)) return;
    const Vm& v = ui.vm; char b[64];
    if (ui.nav_target >= v.n_wp || !v.gnss.valid) { lv_label_set_text(l_dist, "--"); lv_label_set_text(l_brg, "No GPS fix"); lv_label_set_text(l_turn, ""); lv_obj_add_flag(needle, LV_OBJ_FLAG_HIDDEN); have = false; return; }
    const Waypoint& w = v.wps[ui.nav_target];
    double d = geo::distance_m(v.gnss.lat, v.gnss.lon, w.lat, w.lon), br = geo::bearing_deg(v.gnss.lat, v.gnss.lon, w.lat, w.lon);
    bool moving = v.gnss.speed_mps > 0.5f; double rel = moving ? geo::relative_bearing(br, v.gnss.heading_deg) : br; // standing still: north-up
    fmt_dist(b, sizeof b, d, v.settings.metric); lv_label_set_text(l_dist, b);
    snprintf(b, sizeof b, "%s   %.0f\xC2\xB0", geo::cardinal(br), br); lv_label_set_text(l_brg, b);
    if (d < 8) snprintf(b, sizeof b, "You have arrived");
    else if (!moving) snprintf(b, sizeof b, "Standing still: arrow is north-up");
    else if (fabs(rel) < 12) snprintf(b, sizeof b, "Straight ahead");
    else snprintf(b, sizeof b, "Turn %s %.0f\xC2\xB0", rel > 0 ? "right" : "left", fabs(rel));
    lv_label_set_text(l_turn, b);
    if (d < 8 && !arrived) { arrived = true; ui.haptic(HAP_ALERT); char t[40]; snprintf(t, sizeof t, "Arrived at %s", w.name); ui.notify(N_ARRIVED, APP_NAVIGATE, t, "Navigation ended", false); ui.nav_active = false; }
    target = rel; if (!have) { shown = rel; have = true; place(shown); }
    lv_obj_remove_flag(needle, LV_OBJ_FLAG_HIDDEN);
  }
  bool key(uint16_t k) override {
    if (k & KEY_B) { ui.go(PG_BEACON, -1); return true; }
    if (k & KEY_A) { ui.nav_active = false; ui.toast("Navigation ended"); ui.go(PG_BEACON, -1); return true; }
    return false;
  }
};

struct Reply : Page {
  using Page::Page; int sel = 0;
  static const char* const* list(int& n) { static const char* m[] = {"OK", "On my way", "At the waypoint", "Heading back", "Running late", "Need help, not urgent"}; n = 6; return m; }
  Mode mode() const override { return MODE_BEACON; }
  lv_obj_t* body = nullptr;
  void build() override {
    scr = page_root(ui); text(scr, M + 2, BAR_H + 10, "Quick reply", F28, TXT);
    body = box(scr, 0, TOP, W, H - TOP - 30, BG, 0); lv_obj_add_flag(body, LV_OBJ_FLAG_EVENT_BUBBLE);
    hint(scr, "A send to everyone in range   B cancel"); draw();
  }
  void draw() {
    lv_obj_clean(body); int n; const char* const* m = list(n);
    for (int i = 0; i < n; i++) { lv_obj_t* r = row(body, i * 63, 57, i == sel, GREEN, i); text(r, 18, 15, m[i], F20, TXT); }
  }
  bool key(uint16_t k) override {
    int n; const char* const* m = list(n);
    if (k & KEY_B) { ui.go(PG_BEACON, -1); return true; }
    if ((k & KEY_DOWN) && sel < n - 1) { sel++; ui.haptic(); draw(); return true; }
    if ((k & KEY_UP) && sel > 0) { sel--; ui.haptic(); draw(); return true; }
    if (k & KEY_A) { Cmd c(CMD_SEND_TEXT); snprintf(c.text, sizeof c.text, "%s", m[sel]); ui.cmd(c); ui.haptic(HAP_DOUBLE); ui.go(PG_BEACON, -1); return true; }
    return false;
  }
  bool tap(int id) override { sel = id; return key(KEY_A); }
};

Page* new_home(Ui& u) { return new Home(u); }
Page* new_field(Ui& u) { return new Field(u); }
Page* new_ap(Ui& u) { return new ApDetail(u); }
Page* new_sweep(Ui& u) { return new Sweep(u); }
Page* new_beacon(Ui& u) { return new Beacon(u); }
Page* new_nav(Ui& u) { return new Nav(u); }
Page* new_reply(Ui& u) { return new Reply(u); }
}}
