#include "ui.h"
#include "geo.h"
#include "correlate.h"
#include "board.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace ss { namespace ui {

// ------------------------------------------------------------------ theme
static const int W = SS_LCD_W, H = SS_LCD_H, BAR_H = 40, TOP = 92, M = 16;
#define C(hex) lv_color_hex(hex)
static const uint32_t BG = 0x000000, CARD = 0x15171C, CARD_HI = 0x23262E, TXT = 0xFFFFFF, DIM = 0x8B909A, FAINT = 0x3A3E47,
  BLUE = 0x2F7BFF, VIOLET = 0xB45CFF, GREEN = 0x1FD67A, GREY = 0x9AA0A8, RED = 0xFF3B30, AMBER = 0xFFB020;
static const lv_font_t *F12 = &lv_font_montserrat_12, *F14 = &lv_font_montserrat_14, *F16 = &lv_font_montserrat_16, *F20 = &lv_font_montserrat_20,
  *F24 = &lv_font_montserrat_24, *F28 = &lv_font_montserrat_28, *F36 = &lv_font_montserrat_36, *F48 = &lv_font_montserrat_48;

static lv_obj_t* box(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color, int radius) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, C(color), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}
static lv_obj_t* text(lv_obj_t* parent, int x, int y, const char* s, const lv_font_t* f, uint32_t color) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, f, 0); lv_obj_set_style_text_color(l, C(color), 0);
  lv_label_set_text(l, s); lv_obj_set_pos(l, x, y);
  return l;
}
static lv_obj_t* text_w(lv_obj_t* parent, int x, int y, int w, const char* s, const lv_font_t* f, uint32_t color, lv_text_align_t al = LV_TEXT_ALIGN_LEFT) {
  lv_obj_t* l = text(parent, x, y, s, f, color);
  lv_obj_set_width(l, w); lv_label_set_long_mode(l, LV_LABEL_LONG_DOT); lv_obj_set_style_text_align(l, al, 0);
  lv_obj_set_height(l, lv_font_get_line_height(f));
  return l;
}
static lv_obj_t* pill(lv_obj_t* parent, int x, int y, const char* s, uint32_t bg, uint32_t fg, const lv_font_t* f = F14) {
  lv_obj_t* p = box(parent, x, y, LV_SIZE_CONTENT, LV_SIZE_CONTENT, bg, 99);
  lv_obj_set_style_pad_hor(p, 12, 0); lv_obj_set_style_pad_ver(p, 5, 0);
  lv_obj_t* l = lv_label_create(p); lv_obj_set_style_text_font(l, f, 0); lv_obj_set_style_text_color(l, C(fg), 0); lv_label_set_text(l, s);
  return p;
}
static void set_pill(lv_obj_t* p, const char* s, uint32_t bg, uint32_t fg) {
  lv_obj_set_style_bg_color(p, C(bg), 0); lv_obj_t* l = lv_obj_get_child(p, 0);
  lv_label_set_text(l, s); lv_obj_set_style_text_color(l, C(fg), 0);
}
// A focusable card: highlighted background and an accent outline when focused.
static lv_obj_t* row(lv_obj_t* parent, int y, int h, bool focus, uint32_t accent) {
  lv_obj_t* r = box(parent, M, y, W - 2 * M, h, focus ? CARD_HI : CARD, 18);
  if (focus) { lv_obj_set_style_outline_width(r, 2, 0); lv_obj_set_style_outline_color(r, C(accent), 0); lv_obj_set_style_outline_pad(r, 1, 0); }
  return r;
}
static uint32_t rssi_color(int rssi) { return rssi >= -60 ? GREEN : (rssi >= -75 ? AMBER : RED); }
static void bars(lv_obj_t* parent, int x, int y, int rssi) { // 4-bar signal glyph
  int lvl = rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : rssi >= -85 ? 1 : 0;
  for (int i = 0; i < 4; i++) { int h = 6 + i * 5; box(parent, x + i * 7, y + 21 - h, 5, h, i < lvl ? rssi_color(rssi) : FAINT, 2); }
}
static const char* auth_name(uint8_t a) { static const char* n[] = {"Open", "WEP", "WPA", "WPA2", "WPA3", "WPA2-Ent"}; return n[a < 6 ? a : 0]; }
static void header(lv_obj_t* scr, const char* title, uint32_t accent, const char* const* tabs, int n_tabs, int sel) {
  text(scr, M + 2, BAR_H + (n_tabs ? 13 : 10), title, n_tabs ? F24 : F28, TXT);
  if (!n_tabs) return;
  int x = W - M;
  for (int i = n_tabs - 1; i >= 0; i--) { // right-aligned tab pills
    lv_obj_t* p = pill(scr, 0, BAR_H + 13, tabs[i], i == sel ? accent : CARD, i == sel ? 0x000000 : DIM, F12);
    lv_obj_set_style_pad_hor(p, 9, 0);
    lv_obj_update_layout(p); x -= lv_obj_get_width(p); lv_obj_set_x(p, x); x -= 5;
  }
}
static void hint(lv_obj_t* scr, const char* s) { text_w(scr, 0, H - 22, W, s, F12, 0x5C616B, LV_TEXT_ALIGN_CENTER); }
static lv_obj_t* screen() {
  lv_obj_t* s = lv_obj_create(NULL); lv_obj_remove_style_all(s);
  lv_obj_set_style_bg_color(s, C(BG), 0); lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0); lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}
static lv_obj_t* arc(lv_obj_t* parent, int cx, int cy, int r, int a0, int a1, int w, uint32_t color) {
  lv_obj_t* a = lv_arc_create(parent); lv_obj_remove_style_all(a);
  lv_obj_set_size(a, 2 * r, 2 * r); lv_obj_set_pos(a, cx - r, cy - r);
  lv_arc_set_bg_angles(a, (lv_value_precise_t)a0, (lv_value_precise_t)a1);
  lv_obj_set_style_arc_width(a, w, 0); lv_obj_set_style_arc_color(a, C(color), 0); lv_obj_set_style_arc_rounded(a, true, 0); lv_obj_set_style_arc_opa(a, LV_OPA_COVER, 0);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
  return a;
}
// Launcher icons, drawn from primitives (no bitmap assets).
static void icon(lv_obj_t* p, int kind, int s) { // p is an s x s tile
  uint32_t w = 0xFFFFFF; int c = s / 2;
  if (kind == 0) { // Field Test: wifi fan
    for (int i = 0; i < 3; i++) arc(p, c, c + s / 4, s / 6 + i * s / 7, 225, 315, s / 14, w);
    box(p, c - s / 14, c + s / 4 - s / 14, s / 7, s / 7, w, 99);
  } else if (kind == 1) { // Sweep: level bars
    static const int hh[] = {30, 55, 85, 45, 65};
    int bw = s / 9, gap = s / 18, x0 = c - (5 * bw + 4 * gap) / 2;
    for (int i = 0; i < 5; i++) { int h = s * hh[i] / 150; box(p, x0 + i * (bw + gap), c + s / 4 - h, bw, h, w, bw / 2); }
  } else if (kind == 2) { // Beacon: dot with radiating waves
    box(p, c - s / 10, c - s / 10, s / 5, s / 5, w, 99);
    for (int i = 0; i < 2; i++) { int r = s / 5 + i * s / 8 + s / 16; arc(p, c, c, r, 135, 225, s / 16, w); arc(p, c, c, r, 315, 45, s / 16, w); }
  } else { // Settings: sliders
    static const int kx[] = {62, 32, 52};
    for (int i = 0; i < 3; i++) {
      int y = c - s / 4 + i * s / 4;
      box(p, s / 5, y - s / 40, s * 3 / 5, s / 20, w, 99);
      box(p, s * kx[i] / 100, y - s / 12, s / 6, s / 6, w, 99);
    }
  }
}

// ------------------------------------------------------------------ home
struct Home : Page {
  using Page::Page;
  lv_obj_t* sub[4] = {nullptr};
  void build() override {
    scr = screen();
    static const char* names[] = {"Field Test", "Sweep", "Beacon", "Settings"};
    static const uint32_t col[] = {BLUE, VIOLET, GREEN, GREY};
    int tw = (W - 2 * M - 12) / 2, th = 204, y0 = BAR_H + 14;
    for (int i = 0; i < 4; i++) {
      bool f = (i == ui.home_sel);
      lv_obj_t* t = box(scr, M + (i % 2) * (tw + 12), y0 + (i / 2) * (th + 12), tw, th, f ? CARD_HI : CARD, 30);
      if (f) { lv_obj_set_style_outline_width(t, 3, 0); lv_obj_set_style_outline_color(t, C(TXT), 0); lv_obj_set_style_outline_pad(t, 2, 0); }
      int is = 100; lv_obj_t* ic = box(t, (tw - is) / 2, 22, is, is, col[i], 30);
      icon(ic, i, is);
      text_w(t, 0, 134, tw, names[i], F20, TXT, LV_TEXT_ALIGN_CENTER);
      sub[i] = text_w(t, 0, 164, tw, "", F14, DIM, LV_TEXT_ALIGN_CENTER);
    }
    hint(scr, "D-pad move    A open    hold SOS 2 s for emergency");
    refresh(~0u);
  }
  void refresh(uint32_t) override {
    char b[48]; const Vm& v = ui.vm;
    if (v.n_ap || v.n_ble) snprintf(b, sizeof b, "%d Wi-Fi  %d BLE", v.n_ap, v.n_ble); else snprintf(b, sizeof b, "Wi-Fi + Bluetooth");
    lv_label_set_text(sub[0], b);
    lv_label_set_text(sub[1], v.sled_linked ? "Sled linked" : "Sled not attached");
    if (v.gnss.valid) snprintf(b, sizeof b, "Fix  %d sats", v.gnss.sats); else snprintf(b, sizeof b, "Searching  %d sats", v.gnss.sats);
    lv_label_set_text(sub[2], b);
    snprintf(b, sizeof b, "v%s", SS_FW_VERSION); lv_label_set_text(sub[3], b);
  }
  bool key(uint16_t k) override {
    int s = ui.home_sel;
    if (k & KEY_LEFT) s = (s & 2) | 0;
    if (k & KEY_RIGHT) s = (s & 2) | 1;
    if (k & KEY_UP) s = s & 1;
    if (k & KEY_DOWN) s = (s & 1) | 2;
    if (s != ui.home_sel) { ui.home_sel = s; ui.haptic(); ui.go(PG_HOME, 0); return true; }
    if (k & KEY_A) { static const PageId pg[] = {PG_FIELD, PG_SWEEP, PG_BEACON, PG_SETTINGS}; ui.haptic(); ui.go(pg[s], 1); return true; }
    return false;
  }
};

// ------------------------------------------------------------------ field test
struct Field : Page {
  using Page::Page;
  lv_obj_t* body = nullptr; int sel = 0, top = 0;
  Mode mode() const override { return MODE_FIELD; }
  void build() override {
    scr = screen();
    static const char* tabs[] = {"Networks", "Channels", "Bluetooth"};
    header(scr, "Field Test", BLUE, tabs, 3, ui.field_tab);
    body = box(scr, 0, TOP, W, H - TOP - 26, BG, 0);
    hint(scr, ui.field_tab == 0 ? "Left/Right tab    A details    B home" : "Left/Right tab    B home");
    draw();
  }
  void draw() {
    lv_obj_clean(body); const Vm& v = ui.vm; char b[80];
    if (ui.field_tab == 0) {
      if (!v.n_ap) { text_w(body, 0, 150, W, "Scanning...", F20, DIM, LV_TEXT_ALIGN_CENTER); return; }
      if (sel >= v.n_ap) sel = v.n_ap - 1;
      if (sel < top) top = sel;
      if (sel > top + 5) top = sel - 5;
      for (int i = 0; i < 6 && top + i < v.n_ap; i++) {
        const WifiAp& a = v.aps[top + i]; lv_obj_t* r = row(body, i * 63, 57, top + i == sel, BLUE);
        bars(r, 14, 17, a.rssi);
        text_w(r, 54, 8, 230, a.ssid[0] ? a.ssid : "(hidden network)", F20, a.ssid[0] ? TXT : DIM);
        snprintf(b, sizeof b, "ch %d    %s    %s", a.channel, a.channel > 14 ? "5 GHz" : "2.4 GHz", auth_name(a.auth));
        text_w(r, 54, 34, 240, b, F12, a.auth == AUTH_OPEN ? AMBER : DIM);
        snprintf(b, sizeof b, "%d", a.rssi); text_w(r, 290, 8, 74, b, F24, rssi_color(a.rssi), LV_TEXT_ALIGN_RIGHT);
        text_w(r, 290, 36, 74, "dBm", F12, DIM, LV_TEXT_ALIGN_RIGHT);
      }
      snprintf(b, sizeof b, "%d / %d", sel + 1, v.n_ap); text_w(scr, W - 90, H - 22, 76, b, F12, DIM, LV_TEXT_ALIGN_RIGHT);
    } else if (ui.field_tab == 1) {
      // 2.4 GHz congestion: each AP loads its own channel fully and the 4 neighbours partly (20 MHz overlap).
      float load[14] = {0}; int n5 = 0;
      for (int i = 0; i < v.n_ap; i++) {
        int ch = v.aps[i].channel; if (ch > 14) { n5++; continue; }
        float wgt = v.aps[i].rssi >= -60 ? 1.0f : v.aps[i].rssi >= -75 ? 0.7f : 0.4f;
        static const float k[] = {1.0f, 0.75f, 0.5f, 0.25f};
        for (int d = -3; d <= 3; d++) { int c2 = ch + d; if (c2 >= 1 && c2 <= 13) load[c2] += wgt * k[d < 0 ? -d : d]; }
      }
      float mx = 1; for (int c = 1; c <= 13; c++) if (load[c] > mx) mx = load[c];
      int best = 1; const int cand[] = {1, 6, 11}; for (int c : cand) if (load[c] < load[best]) best = c;
      lv_obj_t* card = box(body, M, 0, W - 2 * M, 250, CARD, 22);
      text(card, 16, 12, "2.4 GHz channel load", F16, TXT);
      int bw = 20, gap = 6, x0 = (W - 2 * M - (13 * bw + 12 * gap)) / 2, base = 206;
      for (int c = 1; c <= 13; c++) {
        int h = 6 + (int)(150 * load[c] / mx); float rel = load[c] / mx;
        box(card, x0 + (c - 1) * (bw + gap), base - h, bw, h, c == best ? GREEN : (rel > 0.66f ? RED : rel > 0.33f ? AMBER : BLUE), 6);
        snprintf(b, sizeof b, "%d", c); text_w(card, x0 + (c - 1) * (bw + gap) - 4, base + 8, bw + 8, b, F12, c == best ? GREEN : DIM, LV_TEXT_ALIGN_CENTER);
      }
      lv_obj_t* c2 = box(body, M, 262, W - 2 * M, 58, CARD, 18);
      snprintf(b, sizeof b, "Least congested: channel %d", best); text(c2, 16, 8, b, F20, GREEN);
      text(c2, 16, 34, "of the non-overlapping set 1 / 6 / 11", F12, DIM);
      lv_obj_t* c3 = box(body, M, 330, W - 2 * M, 46, CARD, 18);
      snprintf(b, sizeof b, "5 GHz: %d network%s      2.4 GHz: %d", n5, n5 == 1 ? "" : "s", v.n_ap - n5); text(c3, 16, 13, b, F16, TXT);
    } else {
      if (!v.n_ble) { text_w(body, 0, 150, W, "Scanning...", F20, DIM, LV_TEXT_ALIGN_CENTER); return; }
      if (sel >= v.n_ble) sel = v.n_ble - 1;
      if (sel < top) top = sel;
      if (sel > top + 5) top = sel - 5;
      for (int i = 0; i < 6 && top + i < v.n_ble; i++) {
        const BleDev& d = v.bles[top + i]; lv_obj_t* r = row(body, i * 63, 57, top + i == sel, BLUE);
        bars(r, 14, 17, d.rssi);
        text_w(r, 54, 8, 230, d.name[0] ? d.name : "(no name)", F20, d.name[0] ? TXT : DIM);
        snprintf(b, sizeof b, "%02X:%02X:%02X:%02X:%02X:%02X", d.addr[0], d.addr[1], d.addr[2], d.addr[3], d.addr[4], d.addr[5]);
        text_w(r, 54, 34, 240, b, F12, DIM);
        snprintf(b, sizeof b, "%d", d.rssi); text_w(r, 290, 8, 74, b, F24, rssi_color(d.rssi), LV_TEXT_ALIGN_RIGHT);
        text_w(r, 290, 36, 74, "dBm", F12, DIM, LV_TEXT_ALIGN_RIGHT);
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
};

struct ApDetail : Page {
  using Page::Page;
  lv_obj_t *l_rssi = nullptr, *l_q = nullptr, *chart = nullptr; lv_chart_series_t* ser = nullptr;
  Mode mode() const override { return MODE_FIELD; }
  const WifiAp* find() { for (int i = 0; i < ui.vm.n_ap; i++) if (!memcmp(ui.vm.aps[i].bssid, ui.ap_bssid, 6)) return &ui.vm.aps[i]; return nullptr; }
  void build() override {
    scr = screen(); const WifiAp* a = find(); char b[80];
    text_w(scr, M + 2, BAR_H + 10, W - 2 * M, a && a->ssid[0] ? a->ssid : "(hidden network)", F28, TXT);
    lv_obj_t* c = box(scr, M, TOP, W - 2 * M, 112, CARD, 22);
    l_rssi = text(c, 18, 12, "--", F48, TXT); text(c, 20, 74, "dBm", F16, DIM);
    l_q = text_w(c, 170, 20, 190, "", F24, TXT, LV_TEXT_ALIGN_RIGHT);
    text_w(c, 170, 56, 190, "signal quality", F14, DIM, LV_TEXT_ALIGN_RIGHT);
    lv_obj_t* g = box(scr, M, TOP + 122, W - 2 * M, 150, CARD, 22);
    text(g, 16, 10, "Signal over time", F14, DIM);
    chart = lv_chart_create(g); lv_obj_remove_style_all(chart); lv_obj_set_pos(chart, 14, 36); lv_obj_set_size(chart, W - 2 * M - 28, 100);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE); lv_chart_set_point_count(chart, 40); lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -95, -30);
    lv_chart_set_div_line_count(chart, 3, 0); lv_obj_set_style_line_color(chart, C(FAINT), LV_PART_MAIN); lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 3, LV_PART_ITEMS); lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
    ser = lv_chart_add_series(chart, C(BLUE), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_t* i = box(scr, M, TOP + 282, W - 2 * M, 94, CARD, 22);
    if (a) {
      snprintf(b, sizeof b, "%02X:%02X:%02X:%02X:%02X:%02X", a->bssid[0], a->bssid[1], a->bssid[2], a->bssid[3], a->bssid[4], a->bssid[5]);
      text(i, 16, 10, "BSSID", F12, DIM); text(i, 110, 8, b, F16, TXT);
      int mhz = a->channel <= 13 ? 2407 + 5 * a->channel : (a->channel == 14 ? 2484 : 5000 + 5 * a->channel);
      snprintf(b, sizeof b, "%d    %d MHz", a->channel, mhz); text(i, 16, 38, "Channel", F12, DIM); text(i, 110, 36, b, F16, TXT);
      text(i, 16, 66, "Security", F12, DIM); text(i, 110, 64, auth_name(a->auth), F16, a->auth == AUTH_OPEN ? AMBER : TXT);
    }
    hint(scr, "B back");
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
  lv_chart_series_t *s_lvl = nullptr, *s_exp = nullptr; int last_state = -1;
  static const int LO = -75, HI = -15, MW = W - 2 * M - 32;
  Mode mode() const override { return MODE_SWEEP; }
  void build() override {
    scr = screen(); header(scr, "Sweep", VIOLET, nullptr, 0, 0);
    p_state = pill(scr, 0, BAR_H + 13, "NO SLED", CARD, DIM, F14);
    lv_obj_t* c = box(scr, M, TOP, W - 2 * M, 150, CARD, 22);
    l_dbm = text(c, 16, 6, "--.-", F48, TXT); text(c, 18, 64, "dBm broadband", F14, DIM);
    l_sub = text_w(c, 150, 16, W - 2 * M - 166, "", F16, DIM, LV_TEXT_ALIGN_RIGHT); lv_label_set_long_mode(l_sub, LV_LABEL_LONG_WRAP); lv_obj_set_height(l_sub, 60);
    box(c, 16, 100, MW, 14, FAINT, 7);
    meter = box(c, 16, 100, 10, 14, VIOLET, 7);
    m_floor = box(c, 16, 94, 3, 26, DIM, 1); m_exp = box(c, 16, 94, 3, 26, GREEN, 1);
    text(c, 16, 124, "-75", F12, DIM); text_w(c, 16 + MW - 40, 124, 40, "-15", F12, DIM, LV_TEXT_ALIGN_RIGHT);
    text_w(c, 60, 124, MW - 120, "grey: noise floor   green: expected", F12, 0x5C616B, LV_TEXT_ALIGN_CENTER);
    lv_obj_t* g = box(scr, M, TOP + 160, W - 2 * M, 150, CARD, 22);
    text(g, 16, 10, "Last 30 s", F14, DIM);
    chart = lv_chart_create(g); lv_obj_remove_style_all(chart); lv_obj_set_pos(chart, 14, 34); lv_obj_set_size(chart, W - 2 * M - 28, 104);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE); lv_chart_set_point_count(chart, 120); lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, LO, HI);
    lv_chart_set_div_line_count(chart, 4, 0); lv_obj_set_style_line_color(chart, C(FAINT), LV_PART_MAIN); lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS); lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
    s_exp = lv_chart_add_series(chart, C(GREEN), LV_CHART_AXIS_PRIMARY_Y); s_lvl = lv_chart_add_series(chart, C(VIOLET), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_t* f = box(scr, M, TOP + 320, W - 2 * M, 62, CARD, 18);
    l_known = text(f, 16, 9, "", F16, TXT); l_src = text_w(f, 16, 35, W - 2 * M - 32, "", F12, DIM);
    hint(scr, "Shows how much RF energy is present, not what it is.    B home");
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
      lv_label_set_text(l_dbm, "--.-"); lv_obj_set_style_text_color(l_dbm, C(DIM), 0); lv_label_set_text(l_sub, ""); lv_obj_set_width(meter, 14);
      lv_obj_set_style_bg_color(meter, C(FAINT), 0); lv_obj_add_flag(m_floor, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(m_exp, LV_OBJ_FLAG_HIDDEN);
      lv_chart_set_all_value(chart, s_lvl, LV_CHART_POINT_NONE); lv_chart_set_all_value(chart, s_exp, LV_CHART_POINT_NONE); lv_chart_refresh(chart); return;
    }
    if (!(what & R_RF)) return;
    const RfReading& r = v.rf;
    snprintf(b, sizeof b, "%.1f", r.dbm); lv_label_set_text(l_dbm, b);
    uint32_t col = r.state == corr::UNEXPLAINED ? VIOLET : r.state == corr::EXPLAINED ? GREEN : GREY;
    if ((int)r.state != last_state) {
      last_state = r.state;
      set_pill(p_state, r.state == corr::UNEXPLAINED ? "UNEXPLAINED" : r.state == corr::EXPLAINED ? "EXPLAINED" : "QUIET", col, 0x000000);
      lv_obj_update_layout(p_state); lv_obj_set_x(p_state, W - M - lv_obj_get_width(p_state));
      lv_obj_set_style_text_color(l_dbm, C(r.state == corr::QUIET ? TXT : col), 0); lv_obj_set_style_bg_color(meter, C(col), 0);
    }
    if (r.state == corr::UNEXPLAINED) snprintf(b, sizeof b, "+%.0f dB above what the\nvisible Wi-Fi/BLE explains", r.excess_db);
    else if (r.state == corr::EXPLAINED) snprintf(b, sizeof b, "Consistent with the\nvisible Wi-Fi/BLE activity");
    else snprintf(b, sizeof b, "At the noise floor");
    lv_label_set_text(l_sub, b);
    auto px = [](float d) { float k = (d - LO) / (float)(HI - LO); if (k < 0) k = 0; if (k > 1) k = 1; return (int)(k * MW); };
    int w = px(r.dbm); lv_obj_set_width(meter, w < 14 ? 14 : w);
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

// ------------------------------------------------------------------ beacon
static void fmt_dist(char* b, size_t n, double m, bool metric) {
  if (metric) { if (m < 1000) snprintf(b, n, "%.0f m", m); else snprintf(b, n, "%.2f km", m / 1000); }
  else { double ft = m * 3.28084; if (ft < 1000) snprintf(b, n, "%.0f ft", ft); else snprintf(b, n, "%.2f mi", m / 1609.344); }
}
struct Beacon : Page {
  using Page::Page;
  lv_obj_t* body = nullptr; int sel = 0;
  Mode mode() const override { return MODE_BEACON; }
  void build() override {
    scr = screen();
    static const char* tabs[] = {"Position", "Waypoints", "Messages"};
    header(scr, "Beacon", GREEN, tabs, 3, ui.beacon_tab);
    body = box(scr, 0, TOP, W, H - TOP - 26, BG, 0);
    static const char* hints[] = {"A save waypoint    Left/Right tab    B home", "A navigate    Left/Right tab    B home", "A quick reply    Left/Right tab    B home"};
    hint(scr, hints[ui.beacon_tab]);
    draw();
  }
  void draw() {
    lv_obj_clean(body); const Vm& v = ui.vm; char b[96];
    if (ui.beacon_tab == 0) {
      const GnssFix& g = v.gnss;
      lv_obj_t* c = box(body, M, 0, W - 2 * M, 150, CARD, 22);
      if (g.valid) snprintf(b, sizeof b, "%s FIX    %d sats", g.fix_type == 3 ? "3D" : "2D", g.sats); else snprintf(b, sizeof b, "SEARCHING    %d sats", g.sats);
      pill(c, 14, 12, b, g.valid ? GREEN : AMBER, 0x000000, F12);
      if (g.valid) {
        snprintf(b, sizeof b, "%.5f° %c", fabs(g.lat), g.lat >= 0 ? 'N' : 'S'); text(c, 16, 48, b, F36, TXT);
        snprintf(b, sizeof b, "%.5f° %c", fabs(g.lon), g.lon >= 0 ? 'E' : 'W'); text(c, 16, 94, b, F36, TXT);
      } else { text(c, 16, 54, "Waiting for satellites", F24, DIM); text(c, 16, 92, "Needs open sky. First fix can take a minute.", F14, DIM); }
      const char* lab[] = {"Altitude", "Accuracy", "Speed"}; char val[3][24];
      if (g.valid) {
        if (v.settings.metric) { snprintf(val[0], 24, "%.0f m", g.alt_m); snprintf(val[1], 24, "+/- %.1f m", g.hacc_m); snprintf(val[2], 24, "%.1f km/h", g.speed_mps * 3.6f); }
        else { snprintf(val[0], 24, "%.0f ft", g.alt_m * 3.28084f); snprintf(val[1], 24, "+/- %.0f ft", g.hacc_m * 3.28084f); snprintf(val[2], 24, "%.1f mph", g.speed_mps * 2.23694f); }
      } else for (auto& s : val) snprintf(s, 24, "--");
      int cw = (W - 2 * M - 16) / 3;
      for (int i = 0; i < 3; i++) { lv_obj_t* k = box(body, M + i * (cw + 8), 160, cw, 70, CARD, 18); text(k, 12, 10, lab[i], F12, DIM); text_w(k, 12, 32, cw - 16, val[i], F20, TXT); }
      lv_obj_t* l = box(body, M, 240, W - 2 * M, 70, CARD, 18);
      snprintf(b, sizeof b, "LoRa mesh    node %04X", v.lora.node_id); text(l, 16, 10, b, F16, v.lora.ok ? TXT : RED);
      if (v.lora.ok) snprintf(b, sizeof b, "%.1f MHz    sent %u  heard %u    airtime left %u%%", lora::CHANNELS[v.lora.channel % lora::N_CHANNELS], v.lora.tx_count, v.lora.rx_count, v.lora.budget_pct);
      else snprintf(b, sizeof b, "Radio not responding");
      text_w(l, 16, 38, W - 2 * M - 32, b, F12, DIM);
      lv_obj_t* s = box(body, M, 320, W - 2 * M, 56, v.sos ? RED : CARD, 18);
      text(s, 16, 8, v.sos ? "SOS ACTIVE" : "SOS", F20, v.sos ? TXT : RED);
      text(s, 16, 34, v.sos ? "Position sent every 20 s. Hold SOS 2 s to cancel." : "Hold the SOS button 2 s. Broadcasts position to the mesh.", F12, v.sos ? TXT : DIM);
    } else if (ui.beacon_tab == 1) {
      if (!v.n_wp) { text_w(body, 0, 130, W, "No waypoints yet", F20, DIM, LV_TEXT_ALIGN_CENTER); text_w(body, 0, 162, W, "Save one from the Position tab", F14, DIM, LV_TEXT_ALIGN_CENTER); return; }
      if (sel >= v.n_wp) sel = v.n_wp - 1;
      int top = sel > 5 ? sel - 5 : 0;
      for (int i = 0; i < 6 && top + i < v.n_wp; i++) {
        const Waypoint& w = v.wps[top + i]; lv_obj_t* r = row(body, i * 63, 57, top + i == sel, GREEN);
        text_w(r, 16, 8, 200, w.name, F20, TXT);
        snprintf(b, sizeof b, "%.5f, %.5f", w.lat, w.lon); text_w(r, 16, 34, 220, b, F12, DIM);
        if (v.gnss.valid) {
          char d[24]; fmt_dist(d, sizeof d, geo::distance_m(v.gnss.lat, v.gnss.lon, w.lat, w.lon), v.settings.metric);
          text_w(r, 230, 8, 134, d, F20, GREEN, LV_TEXT_ALIGN_RIGHT);
          double br = geo::bearing_deg(v.gnss.lat, v.gnss.lon, w.lat, w.lon); snprintf(b, sizeof b, "%s  %.0f°", geo::cardinal(br), br);
          text_w(r, 230, 34, 134, b, F12, DIM, LV_TEXT_ALIGN_RIGHT);
        }
      }
    } else {
      if (!v.n_msg) { text_w(body, 0, 130, W, "No messages yet", F20, DIM, LV_TEXT_ALIGN_CENTER); text_w(body, 0, 162, W, "Press A to send a quick reply", F14, DIM, LV_TEXT_ALIGN_CENTER); return; }
      int y = H - TOP - 30;
      for (int i = v.n_msg - 1; i >= 0 && y > 60; i--) { // newest at the bottom
        const MeshText& m = v.msgs[i]; int bw = 290; y -= 64;
        lv_obj_t* r = box(body, m.mine ? W - M - bw : M, y, bw, 58, m.sos ? RED : (m.mine ? 0x0E5C36 : CARD), 18);
        text_w(r, 14, 8, bw - 28, m.text, F16, TXT);
        if (m.mine) snprintf(b, sizeof b, "You");
        else if (m.has_pos) snprintf(b, sizeof b, "%04X    %.4f, %.4f    batt %u%%", m.src, m.lat, m.lon, m.batt);
        else snprintf(b, sizeof b, "%04X    %u hop%s    %.0f dBm", m.src, m.hops + 1, m.hops ? "s" : "", m.rssi);
        text_w(r, 14, 34, bw - 28, b, F12, m.sos ? TXT : DIM);
      }
    }
  }
  void refresh(uint32_t what) override {
    if ((ui.beacon_tab == 0 && (what & (R_GNSS | R_LINK))) || (ui.beacon_tab == 1 && (what & (R_WP | R_GNSS))) || (ui.beacon_tab == 2 && (what & R_MSG))) draw();
  }
  bool key(uint16_t k) override {
    if (k & KEY_B) { ui.go(PG_HOME, -1); return true; }
    if (k & (KEY_LEFT | KEY_RIGHT)) { ui.beacon_tab = (ui.beacon_tab + ((k & KEY_RIGHT) ? 1 : 2)) % 3; ui.haptic(); ui.go(PG_BEACON, 0); return true; }
    if (ui.beacon_tab == 0 && (k & KEY_A)) { ui.haptic(HAP_DOUBLE); ui.cmd(Cmd(CMD_WP_SAVE)); return true; }
    if (ui.beacon_tab == 1) {
      if ((k & KEY_DOWN) && sel < ui.vm.n_wp - 1) { sel++; ui.haptic(); draw(); return true; }
      if ((k & KEY_UP) && sel > 0) { sel--; ui.haptic(); draw(); return true; }
      if ((k & KEY_A) && ui.vm.n_wp) { ui.nav_target = sel; ui.haptic(); ui.go(PG_NAV, 1); return true; }
    }
    if (ui.beacon_tab == 2 && (k & KEY_A)) { ui.haptic(); ui.go(PG_REPLY, 1); return true; }
    return false;
  }
};

struct Nav : Page { // return-to-waypoint
  using Page::Page;
  lv_obj_t *l_dist = nullptr, *l_brg = nullptr, *l_turn = nullptr, *needle = nullptr, *dial = nullptr; lv_point_precise_t pts[5];
  Mode mode() const override { return MODE_BEACON; }
  void build() override {
    scr = screen(); const Vm& v = ui.vm; const char* name = ui.nav_target < v.n_wp ? v.wps[ui.nav_target].name : "?";
    char b[40]; snprintf(b, sizeof b, "To %s", name); text_w(scr, M + 2, BAR_H + 10, W - 2 * M, b, F28, TXT);
    int cx = W / 2, cy = TOP + 118, r = 104;
    dial = box(scr, cx - r, cy - r, 2 * r, 2 * r, CARD, 999);
    lv_obj_set_style_border_width(dial, 3, 0); lv_obj_set_style_border_color(dial, C(FAINT), 0);
    text_w(dial, 0, 8, 2 * r, "ahead", F12, DIM, LV_TEXT_ALIGN_CENTER);
    needle = lv_line_create(dial); lv_obj_set_style_line_width(needle, 10, 0); lv_obj_set_style_line_color(needle, C(GREEN), 0); lv_obj_set_style_line_rounded(needle, true, 0);
    l_dist = text_w(scr, 0, TOP + 236, W, "--", F48, TXT, LV_TEXT_ALIGN_CENTER);
    l_brg = text_w(scr, 0, TOP + 296, W, "", F20, GREEN, LV_TEXT_ALIGN_CENTER);
    l_turn = text_w(scr, 0, TOP + 328, W, "", F14, DIM, LV_TEXT_ALIGN_CENTER);
    hint(scr, "Arrow uses GPS course: keep walking for it to settle.    B back");
    refresh(R_GNSS);
  }
  void refresh(uint32_t what) override {
    if (!(what & R_GNSS)) return;
    const Vm& v = ui.vm; char b[64];
    if (ui.nav_target >= v.n_wp || !v.gnss.valid) { lv_label_set_text(l_dist, "--"); lv_label_set_text(l_brg, "No GPS fix"); lv_label_set_text(l_turn, ""); lv_obj_add_flag(needle, LV_OBJ_FLAG_HIDDEN); return; }
    const Waypoint& w = v.wps[ui.nav_target];
    double d = geo::distance_m(v.gnss.lat, v.gnss.lon, w.lat, w.lon), br = geo::bearing_deg(v.gnss.lat, v.gnss.lon, w.lat, w.lon);
    bool moving = v.gnss.speed_mps > 0.5f; double rel = moving ? geo::relative_bearing(br, v.gnss.heading_deg) : br; // standing still: north-up
    fmt_dist(b, sizeof b, d, v.settings.metric); lv_label_set_text(l_dist, b);
    snprintf(b, sizeof b, "%s   %.0f°", geo::cardinal(br), br); lv_label_set_text(l_brg, b);
    if (d < 8) snprintf(b, sizeof b, "You have arrived");
    else if (!moving) snprintf(b, sizeof b, "Standing still: arrow is north-up");
    else if (fabs(rel) < 12) snprintf(b, sizeof b, "Straight ahead");
    else snprintf(b, sizeof b, "Turn %s %.0f°", rel > 0 ? "right" : "left", fabs(rel));
    lv_label_set_text(l_turn, b);
    double a = rel * 0.017453292519943295; int c = 104; // arrow: shaft with a two-stroke head
    auto P = [&](double rr, double off) { lv_point_precise_t p; p.x = (lv_value_precise_t)(c + rr * sin(a + off)); p.y = (lv_value_precise_t)(c - rr * cos(a + off)); return p; };
    pts[0] = P(-56, 0); pts[1] = P(74, 0); pts[2] = P(44, -0.50); pts[3] = P(74, 0); pts[4] = P(44, 0.50);
    lv_line_set_points(needle, pts, 5); lv_obj_remove_flag(needle, LV_OBJ_FLAG_HIDDEN);
  }
  bool key(uint16_t k) override { if (k & KEY_B) { ui.go(PG_BEACON, -1); return true; } return false; }
};

struct Reply : Page {
  using Page::Page; int sel = 0;
  static const char* const* list(int& n) { static const char* m[] = {"OK", "On my way", "At the waypoint", "Heading back", "Running late", "Need help, not urgent"}; n = 6; return m; }
  Mode mode() const override { return MODE_BEACON; }
  void build() override {
    scr = screen(); text(scr, M + 2, BAR_H + 10, "Quick reply", F28, TXT);
    int n; const char* const* m = list(n);
    for (int i = 0; i < n; i++) { lv_obj_t* r = row(scr, TOP + i * 63, 57, i == sel, GREEN); text(r, 18, 16, m[i], F20, TXT); }
    hint(scr, "A send to everyone in range    B cancel");
  }
  bool key(uint16_t k) override {
    int n; const char* const* m = list(n);
    if (k & KEY_B) { ui.go(PG_BEACON, -1); return true; }
    if ((k & KEY_DOWN) && sel < n - 1) { sel++; ui.haptic(); lv_obj_clean(scr); rebuild(); return true; }
    if ((k & KEY_UP) && sel > 0) { sel--; ui.haptic(); lv_obj_clean(scr); rebuild(); return true; }
    if (k & KEY_A) { Cmd c(CMD_SEND_TEXT); snprintf(c.text, sizeof c.text, "%s", m[sel]); ui.cmd(c); ui.haptic(HAP_DOUBLE); ui.go(PG_BEACON, -1); return true; }
    return false;
  }
  void rebuild() { lv_obj_t* old = scr; build(); lv_obj_t* fresh = scr; // move children of the fresh screen into the live one
    while (lv_obj_get_child_count(fresh)) lv_obj_set_parent(lv_obj_get_child(fresh, 0), old);
    lv_obj_delete(fresh); scr = old; }
};

// ------------------------------------------------------------------ settings
struct SettingsPage : Page {
  using Page::Page; int sel = 0; lv_obj_t* body = nullptr;
  static const int N = 9;
  Mode mode() const override { return MODE_SETTINGS; }
  void build() override {
    scr = screen(); header(scr, "Settings", GREY, nullptr, 0, 0);
    body = box(scr, 0, TOP, W, H - TOP - 26, BG, 0);
    hint(scr, "Up/Down choose    Left/Right change    B home");
    draw();
  }
  void value(int i, char* b, size_t n) {
    const Vm& v = ui.vm; const Settings& s = v.settings;
    switch (i) {
      case 0: snprintf(b, n, "%u%%", s.brightness); break;
      case 1: snprintf(b, n, "%s", s.haptics ? "On" : "Off"); break;
      case 2: snprintf(b, n, "%s", s.siren ? "On" : "Off"); break;
      case 3: snprintf(b, n, "%s", s.metric ? "Metric" : "Imperial"); break;
      case 4: snprintf(b, n, "%d   %.1f MHz", s.lora_channel + 1, lora::CHANNELS[s.lora_channel % lora::N_CHANNELS]); break;
      case 5: snprintf(b, n, "%u dBm", s.lora_power); break;
      case 6: snprintf(b, n, "%s", s.gnss_psm ? "On" : "Off"); break;
      case 7: snprintf(b, n, "%s", !v.sd ? "No card" : s.logging ? "On" : "Off"); break;
      default: snprintf(b, n, "Press A"); break;
    }
  }
  void draw() {
    lv_obj_clean(body);
    static const char* names[N] = {"Brightness", "Haptics", "SOS siren", "Units", "LoRa channel", "LoRa power", "GPS power save", "Log to microSD", "Sleep now"};
    int top = sel > 5 ? sel - 5 : 0; char b[40];
    for (int i = 0; i < 6 && top + i < N; i++) {
      int k = top + i; lv_obj_t* r = row(body, i * 63, 57, k == sel, GREY);
      text(r, 18, 17, names[k], F20, TXT); value(k, b, sizeof b);
      text_w(r, 180, 18, 184, b, F16, k == sel ? TXT : DIM, LV_TEXT_ALIGN_RIGHT);
    }
  }
  void refresh(uint32_t what) override { if (what & (R_SET | R_LINK)) draw(); }
  bool key(uint16_t k) override {
    if (k & KEY_B) { ui.go(PG_HOME, -1); return true; }
    if ((k & KEY_DOWN) && sel < N - 1) { sel++; ui.haptic(); draw(); return true; }
    if ((k & KEY_UP) && sel > 0) { sel--; ui.haptic(); draw(); return true; }
    int d = (k & KEY_RIGHT) ? 1 : (k & KEY_LEFT) ? -1 : 0;
    if (sel == 8) { if (k & KEY_A) { ui.cmd(Cmd(CMD_SLEEP)); return true; } return false; }
    if (!d && (k & KEY_A)) d = 1;
    if (!d) return false;
    Settings s = ui.vm.settings;
    switch (sel) {
      case 0: { int v = s.brightness + d * 10; s.brightness = (uint8_t)(v < 10 ? 10 : v > 100 ? 100 : v); break; }
      case 1: s.haptics = !s.haptics; break;
      case 2: s.siren = !s.siren; break;
      case 3: s.metric = !s.metric; break;
      case 4: s.lora_channel = (uint8_t)((s.lora_channel + lora::N_CHANNELS + d) % lora::N_CHANNELS); break;
      case 5: { int v = s.lora_power + d * 2; s.lora_power = (uint8_t)(v < 2 ? 2 : v > 22 ? 22 : v); break; }
      case 6: s.gnss_psm = !s.gnss_psm; break;
      case 7: s.logging = !s.logging; break;
    }
    ui.vm.settings = s; Cmd c(CMD_SET_SETTINGS); c.settings = s; ui.cmd(c); ui.haptic(); draw();
    return true;
  }
};

// ------------------------------------------------------------------ shell
void Ui::status_build() {
  lv_obj_t* top = lv_layer_top();
  bar_ = box(top, 0, 0, W, BAR_H, BG, 0);
  l_time_ = text(bar_, M + 2, 10, "--:--", F20, TXT);
  l_batt_ = text_w(bar_, W - M - 92, 11, 92, "", F16, TXT, LV_TEXT_ALIGN_RIGHT);
  static const char* sym[4] = {LV_SYMBOL_SD_CARD, LV_SYMBOL_USB, LV_SYMBOL_SHUFFLE, LV_SYMBOL_GPS}; // card, sled, mesh, fix
  for (int i = 0; i < 4; i++) icons_[i] = text_w(bar_, W - M - 92 - 118 + i * 28, 11, 24, sym[i], F16, FAINT, LV_TEXT_ALIGN_CENTER);
  toast_ = box(top, 30, H - 86, W - 60, 46, 0x2B2F38, 23); lv_obj_add_flag(toast_, LV_OBJ_FLAG_HIDDEN);
  l_toast_ = text_w(toast_, 10, 13, W - 80, "", F16, TXT, LV_TEXT_ALIGN_CENTER);
  lock_ = box(top, 0, 0, W, H, BG, 0); lv_obj_add_flag(lock_, LV_OBJ_FLAG_HIDDEN);
  l_lock_time_ = text_w(lock_, 0, 170, W, "--:--", F48, TXT, LV_TEXT_ALIGN_CENTER);
  text_w(lock_, 0, 240, W, LV_SYMBOL_EYE_CLOSE "  Locked", F20, DIM, LV_TEXT_ALIGN_CENTER);
  text_w(lock_, 0, 276, W, "Slide the side switch to unlock. SOS still works.", F14, 0x5C616B, LV_TEXT_ALIGN_CENTER);
}
void Ui::status_update() {
  char b[160];
  if (vm.sos) { lv_obj_set_style_bg_color(bar_, C(((now / 500) & 1) ? RED : 0x8A1F1A), 0); lv_label_set_text(l_time_, "SOS ACTIVE"); }
  else {
    lv_obj_set_style_bg_color(bar_, C(BG), 0);
    if (vm.gnss.time_valid) snprintf(b, sizeof b, "%02u:%02u", vm.gnss.hour, vm.gnss.minute); else snprintf(b, sizeof b, "--:--");
    lv_label_set_text(l_time_, b); lv_label_set_text(l_lock_time_, b);
  }
  const char* sym = vm.power.charging ? LV_SYMBOL_CHARGE : vm.power.pct > 80 ? LV_SYMBOL_BATTERY_FULL : vm.power.pct > 55 ? LV_SYMBOL_BATTERY_3 :
                    vm.power.pct > 30 ? LV_SYMBOL_BATTERY_2 : vm.power.pct > 10 ? LV_SYMBOL_BATTERY_1 : LV_SYMBOL_BATTERY_EMPTY;
  if (vm.power.gauge_ok) snprintf(b, sizeof b, "%.0f%%  %s", vm.power.pct, sym); else snprintf(b, sizeof b, "--  %s", LV_SYMBOL_BATTERY_EMPTY);
  lv_label_set_text(l_batt_, b);
  lv_obj_set_style_text_color(l_batt_, C(vm.sos ? TXT : vm.power.charging ? GREEN : vm.power.pct <= 15 && vm.power.gauge_ok ? RED : TXT), 0);
  const bool on[4] = {vm.sd && vm.settings.logging, vm.sled_linked, vm.lora.ok, vm.gnss.valid};
  for (int i = 0; i < 4; i++) lv_obj_set_style_text_color(icons_[i], C(on[i] ? TXT : (vm.sos ? 0xE08A85 : FAINT)), 0);
}
void Ui::toast(const char* s) {
  lv_label_set_text(l_toast_, s); lv_obj_remove_flag(toast_, LV_OBJ_FLAG_HIDDEN); toast_until_ = now + 2500;
}
void Ui::begin() {
  lv_obj_set_style_bg_color(lv_layer_top(), C(BG), 0);
  status_build();
  go(PG_HOME, 0);
  status_update();
}
void Ui::go(PageId id, int dir, int) {
  Page* p = nullptr;
  switch (id) {
    case PG_HOME: p = new Home(*this); break;
    case PG_FIELD: p = new Field(*this); break;
    case PG_AP: p = new ApDetail(*this); break;
    case PG_SWEEP: p = new Sweep(*this); break;
    case PG_BEACON: p = new Beacon(*this); break;
    case PG_NAV: p = new Nav(*this); break;
    case PG_REPLY: p = new Reply(*this); break;
    case PG_SETTINGS: p = new SettingsPage(*this); break;
  }
  p->build();
  // Forward = slide in from the right, back = from the left, same level = cut.
  lv_screen_load_anim(p->scr, dir > 0 ? LV_SCR_LOAD_ANIM_MOVE_LEFT : dir < 0 ? LV_SCR_LOAD_ANIM_MOVE_RIGHT : LV_SCR_LOAD_ANIM_NONE, dir ? 200 : 0, 0, true);
  Mode before = page_ ? page_->mode() : (Mode)255;
  delete page_; page_ = p; page_id_ = id;
  if (p->mode() != before) cmd(Cmd(CMD_SET_MODE, p->mode()));
}
void Ui::on_event(const Event& e, uint32_t& what) {
  switch (e.type) {
    case EV_KEY: {
      bool locked = (e.u.key.keys & KEY_LOCK) != 0;
      if (locked != vm.locked) { vm.locked = locked; if (locked) lv_obj_remove_flag(lock_, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(lock_, LV_OBJ_FLAG_HIDDEN); }
      held_ = (uint16_t)(e.u.key.keys & (KEY_UP | KEY_DOWN)); repeat_at_ = now + 450;
      uint16_t k = e.u.key.pressed;
      if (locked || !k) break;
      if (k & KEY_SIDE) { if (page_id_ != PG_HOME) go(PG_HOME, -1); break; } // side button = home from anywhere
      if (page_) page_->key(k);
      break;
    }
    case EV_POWER: vm.power = e.u.power; what |= R_POWER; break;
    case EV_ENV: vm.env = e.u.env; break;
    case EV_GNSS: vm.gnss = e.u.gnss; what |= R_GNSS; break;
    case EV_SETTINGS: vm.settings = e.u.settings; what |= R_SET; break;
    case EV_STORAGE: vm.sd = e.u.storage.mounted; what |= R_LINK; break;
    case EV_LORA: vm.lora = e.u.lora; what |= R_LINK; break;
    case EV_SLED: vm.sled_linked = e.u.sled.linked; if (!vm.sled_linked) { vm.have_rf = false; vm.rf_n = 0; } what |= R_LINK; break;
    case EV_SOS: vm.sos = e.u.sos.active; toast(vm.sos ? "SOS started: broadcasting position" : "SOS cancelled"); what |= R_GNSS | R_LINK; break;
    case EV_RF:
      vm.rf = e.u.rf; vm.have_rf = true;
      if ((uint32_t)(e.t_ms - t_tick_) >= 250) { // 4 points per second in the history
        t_tick_ = e.t_ms;
        if (vm.rf_n == 120) { memmove(vm.rf_hist, vm.rf_hist + 1, 119 * sizeof(float)); memmove(vm.rf_exp, vm.rf_exp + 1, 119 * sizeof(float)); vm.rf_n = 119; }
        vm.rf_hist[vm.rf_n] = e.u.rf.dbm; vm.rf_exp[vm.rf_n] = e.u.rf.expected_dbm; vm.rf_n++;
      }
      what |= R_RF; break;
    case EV_MSG:
      if (vm.n_msg == 12) { memmove(vm.msgs, vm.msgs + 1, 11 * sizeof(MeshText)); vm.n_msg = 11; }
      vm.msgs[vm.n_msg++] = e.u.msg; what |= R_MSG;
      if (!e.u.msg.mine) { char b[48]; snprintf(b, sizeof b, e.u.msg.sos ? "SOS from %04X" : "Message from %04X", e.u.msg.src); toast(b); haptic(e.u.msg.sos ? HAP_ALERT : HAP_BUZZ); }
      break;
    case EV_WIFI_BEGIN: vm.n_ap_in = 0; break;
    case EV_WIFI_AP: if (vm.n_ap_in < 32) vm.aps_in[vm.n_ap_in++] = e.u.ap; break;
    case EV_WIFI_END: {
      for (int i = 1; i < vm.n_ap_in; i++) { WifiAp t = vm.aps_in[i]; int j = i - 1; while (j >= 0 && vm.aps_in[j].rssi < t.rssi) { vm.aps_in[j + 1] = vm.aps_in[j]; j--; } vm.aps_in[j + 1] = t; }
      // Keep rows from jumping about: an AP only moves in the list when its RSSI changes by a real margin.
      memcpy(vm.aps, vm.aps_in, sizeof(WifiAp) * (size_t)vm.n_ap_in); vm.n_ap = vm.n_ap_in;
      for (int i = 0; i < vm.n_ap; i++) {
        ApHist* h = nullptr;
        for (int k = 0; k < vm.n_hist; k++) if (!memcmp(vm.hist[k].bssid, vm.aps[i].bssid, 6)) h = &vm.hist[k];
        if (!h && vm.n_hist < 32) { h = &vm.hist[vm.n_hist++]; memcpy(h->bssid, vm.aps[i].bssid, 6); h->n = 0; }
        if (!h) continue;
        if (h->n == 40) { memmove(h->rssi, h->rssi + 1, 39); h->n = 39; }
        h->rssi[h->n++] = vm.aps[i].rssi;
      }
      what |= R_WIFI; break;
    }
    case EV_BLE_BEGIN: vm.n_ble_in = 0; break;
    case EV_BLE_DEV: if (vm.n_ble_in < 32) vm.bles_in[vm.n_ble_in++] = e.u.ble; break;
    case EV_BLE_END:
      for (int i = 1; i < vm.n_ble_in; i++) { BleDev t = vm.bles_in[i]; int j = i - 1; while (j >= 0 && vm.bles_in[j].rssi < t.rssi) { vm.bles_in[j + 1] = vm.bles_in[j]; j--; } vm.bles_in[j + 1] = t; }
      memcpy(vm.bles, vm.bles_in, sizeof(BleDev) * (size_t)vm.n_ble_in); vm.n_ble = vm.n_ble_in; what |= R_BLE; break;
    case EV_WP_BEGIN: vm.n_wp_in = 0; break;
    case EV_WP_ITEM: if (vm.n_wp_in < 16) vm.wps_in[vm.n_wp_in++] = e.u.wp; break;
    case EV_WP_END: memcpy(vm.wps, vm.wps_in, sizeof(Waypoint) * (size_t)vm.n_wp_in); vm.n_wp = vm.n_wp_in; what |= R_WP; break;
    case EV_TOAST: toast(e.u.toast); break;
  }
}
void Ui::tick(uint32_t now_ms) {
  now = now_ms; uint32_t what = 0; Event e; int budget = 64;
  while (budget-- > 0 && bus.events.pop(e)) on_event(e, what);
  if (held_ && !vm.locked && (int32_t)(now - repeat_at_) >= 0) { repeat_at_ = now + 130; if (page_) page_->key(held_); } // key repeat for lists
  if (toast_until_ && (int32_t)(now - toast_until_) >= 0) { toast_until_ = 0; lv_obj_add_flag(toast_, LV_OBJ_FLAG_HIDDEN); }
  static uint32_t t_status = 0;
  if (what & (R_POWER | R_GNSS | R_LINK | R_SET) || (uint32_t)(now - t_status) >= 500) { t_status = now; status_update(); }
  if (what && page_) page_->refresh(what);
}
}}
