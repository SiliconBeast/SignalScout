// SignalOS shell: status bar, live-activity pill, banners, pull-down shade,
// lock screen / always-on display, quick-action sheet, gestures, page stack.
#include "ui.h"
#include "geo.h"
#include "correlate.h"
#include "weather.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace ss { namespace ui {

static Ui* g_ui = nullptr;

// ------------------------------------------------------------------ page plumbing
static void page_event(lv_event_t* e) {
  if (!g_ui || !g_ui->page()) return;
  lv_obj_t* t = (lv_obj_t*)lv_event_get_target(e);
  intptr_t id = (intptr_t)lv_obj_get_user_data(t) - 1; if (id < 0) return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) { g_ui->wake(); g_ui->page()->tap((int)id); }
  else if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED && id >= 2000) { g_ui->wake(); g_ui->quick_actions((int)id - 2000); }
}
lv_obj_t* page_root(Ui& ui) {
  lv_obj_t* s = box(ui.stage, 0, 0, W, H, BG, 0);
  lv_obj_add_event_cb(s, page_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(s, page_event, LV_EVENT_LONG_PRESSED, nullptr);
  return s;
}

void Ui::go(PageId id, int dir) {
  Page* p = nullptr;
  switch (id) {
    case PG_HOME: p = new_home(*this); break;       case PG_FIELD: p = new_field(*this); break;
    case PG_AP: p = new_ap(*this); break;           case PG_SWEEP: p = new_sweep(*this); break;
    case PG_BEACON: p = new_beacon(*this); break;   case PG_NAV: p = new_nav(*this); break;
    case PG_REPLY: p = new_reply(*this); break;     case PG_SETTINGS: p = new_settings(*this); break;
    case PG_WEATHER: p = new_weather(*this); break; case PG_CONSOLE: p = new_console(*this); break;
    case PG_TORCH: p = new_torch(*this); break;     case PG_LEVEL: p = new_level(*this); break;
    case PG_UPDATES: p = new_updates(*this); break; case PG_SETUP: p = new_setup(*this); break;
  }
  p->build();
  lv_obj_t* old = page_ ? page_->scr : nullptr;
  if (old && dir) {
    // Parallax push: the new page slides the full width, the old one a third, then is deleted.
    lv_anim_delete(old, nullptr);
    if (dir > 0) { anim_x(p->scr, W, 0, 280); anim_x(old, 0, -W / 3, 280, 0, true); }
    else { lv_obj_move_to_index(p->scr, 0); anim_x(p->scr, -W / 3, 0, 280); anim_x(old, 0, W, 280, 0, true); }
  } else if (old) lv_obj_delete(old);
  Mode before = page_ ? page_->mode() : (Mode)255;
  bool torch_before = page_id_ == PG_TORCH && page_;
  delete page_; page_ = p; page_id_ = id;
  if (p->mode() != before) cmd(Cmd(CMD_SET_MODE, p->mode()));
  if (torch_before && id != PG_TORCH) cmd(Cmd(CMD_BRIGHTNESS, -1));
  // Torch is full-bleed: no status bar or home indicator over the light.
  if (bar_) { if (id == PG_TORCH) { lv_obj_add_flag(bar_, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(home_bar_, LV_OBJ_FLAG_HIDDEN); } else { lv_obj_remove_flag(bar_, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(home_bar_, LV_OBJ_FLAG_HIDDEN); } }
}
void Ui::open_app(int app) {
  switch (app) {
    case APP_WIFI: field_tab = 0; go(PG_FIELD, 1); break;
    case APP_CHANNELS: field_tab = 1; go(PG_FIELD, 1); break;
    case APP_BLE: field_tab = 2; go(PG_FIELD, 1); break;
    case APP_SWEEP: go(PG_SWEEP, 1); break;
    case APP_POSITION: beacon_tab = 0; go(PG_BEACON, 1); break;
    case APP_NAVIGATE: beacon_tab = 1; go(PG_BEACON, 1); break;
    case APP_MESH: beacon_tab = 2; go(PG_BEACON, 1); break;
    case APP_WEATHER: go(PG_WEATHER, 1); break;
    case APP_CONSOLE: go(PG_CONSOLE, 1); break;
    case APP_TORCH: go(PG_TORCH, 1); break;
    case APP_LEVEL: go(PG_LEVEL, 1); break;
    case APP_UPDATES: go(PG_UPDATES, 1); break;
    default: go(PG_SETTINGS, 1); break;
  }
}
void Ui::apply(const Settings& s) {
  bool restyle = s.profile != vm.settings.profile;
  vm.settings = s; Cmd c(CMD_SET_SETTINGS); c.settings = s; cmd(c);
  if (restyle) {
    set_profile(s.profile);
    // Rebuild the shell in the new style. Async: this can run inside a click event of the shade itself.
    for (lv_obj_t* o : {bar_, shade_, lock_, banner_, toast_, home_bar_}) if (o) lv_obj_delete_async(o);
    bar_ = shade_ = lock_ = banner_ = toast_ = home_bar_ = nullptr; live_txt_[0] = 0;
    status_build(); shade_build(); lock_build(); shade_set_y(shade_open_ ? 0 : -H);
    if (shade_open_) shade_draw();
    go(page_id_, 0); status_update();
  }
}

// ------------------------------------------------------------------ time
bool Ui::local_time(clk::Civil& c) const {
  if (!vm.utc) return false;
  uint32_t t = vm.utc + (now - vm.utc_ms) / 1000 + (int32_t)vm.settings.tz_min * 60;
  c = clk::from_unix(t); return true;
}

// ------------------------------------------------------------------ status bar
static lv_obj_t* make_batt(lv_obj_t* parent, int x, int y, lv_obj_t** fill) {
  lv_obj_t* b = box(parent, x, y, 26, 13, BG, 4); lv_obj_set_style_border_width(b, 2, 0); lv_obj_set_style_border_color(b, C(DIM), 0);
  box(parent, x + 27, y + 4, 2, 5, DIM, 1);
  *fill = box(b, 2, 2, 18, 5, TXT, 2);
  return b;
}
void Ui::status_build() {
  lv_obj_t* top = lv_layer_top();
  bar_ = box(top, 0, 0, W, BAR_H, BG, 0);
  l_time_ = text(bar_, M + 4, 9, "--:--", theme().mono_data ? M20 : F20, TXT);
  live_ = box(bar_, W / 2 - 40, 7, 80, 26, CARD_HI, 13); lv_obj_add_flag(live_, LV_OBJ_FLAG_HIDDEN);
  d_live_ = box(live_, 10, 9, 8, 8, RED, 4);
  l_live_ = text(live_, 24, 4, "", theme().mono_data ? M12 : F14, TXT);
  static const lv_image_dsc_t* g[5] = {&ic20_sd, &ic20_sled, &ic20_mesh, &ic20_position, &ic20_wifi};
  for (int i = 0; i < 5; i++) { ic_[i] = glyph(bar_, g[i], W - M - 74 - 26 * (5 - i), 10, FAINT); lv_obj_add_flag(ic_[i], LV_OBJ_FLAG_HIDDEN); }
  batt_ = make_batt(bar_, W - M - 30, 14, &batt_fill_);
  l_batt_ = text_w(bar_, W - M - 74, 11, 40, "", theme().mono_data ? M12 : F14, TXT, LV_TEXT_ALIGN_RIGHT);
  toast_ = box(top, 40, H - 92, W - 80, 44, 0x24272E, 22); lv_obj_add_flag(toast_, LV_OBJ_FLAG_HIDDEN);
  l_toast_ = text_w(toast_, 10, 12, W - 100, "", F16, TXT, LV_TEXT_ALIGN_CENTER);
  home_bar_ = box(top, W / 2 - 60, H - 9, 120, 5, 0x6A707A, 3);
  banner_ = nullptr;
}
void Ui::live_update() {
  char b[40] = ""; uint32_t dot = RED, bg = CARD_HI; clk::Civil c;
  if (vm.sos) { snprintf(b, sizeof b, "SOS  broadcasting"); bg = RED; dot = TXT; }
  else if (vm.ota.state == OTA_DOWNLOADING || vm.ota.state == OTA_VERIFYING) { snprintf(b, sizeof b, "Updating  %u%%", vm.ota.progress); dot = 0xFF3D7F; }
  else if (vm.ota.state == OTA_READY) { snprintf(b, sizeof b, "Update ready"); dot = 0xFF3D7F; }
  else if (nav_active && page_id_ != PG_NAV && vm.gnss.valid && nav_target < vm.n_wp) {
    const Waypoint& w = vm.wps[nav_target]; char d[16]; fmt_dist(d, sizeof d, geo::distance_m(vm.gnss.lat, vm.gnss.lon, w.lat, w.lon), vm.settings.metric);
    snprintf(b, sizeof b, "%s  %s", w.name, d); dot = theme().acc;
  }
  else if (vm.net.ap) { snprintf(b, sizeof b, "Hotspot  %u", vm.net.ap_clients); dot = CYAN; }
  else if (vm.sd && vm.settings.logging) { snprintf(b, sizeof b, "REC"); dot = RED; }
  (void)c;
  if (!strcmp(b, live_txt_)) return;
  bool was = live_txt_[0] != 0; snprintf(live_txt_, sizeof live_txt_, "%s", b);
  if (!b[0]) { lv_obj_add_flag(live_, LV_OBJ_FLAG_HIDDEN); return; }
  lv_label_set_text(l_live_, b); lv_obj_set_style_bg_color(live_, C(bg), 0); lv_obj_set_style_bg_color(d_live_, C(dot), 0);
  // fits between the clock and the leftmost link glyph
  int left = 92, right = W - M - 74; for (int i = 0; i < 5; i++) if (!lv_obj_has_flag(ic_[i], LV_OBJ_FLAG_HIDDEN) && lv_obj_get_x(ic_[i]) < right) right = lv_obj_get_x(ic_[i]);
  right -= 8; int room = right - left;
  lv_obj_set_width(l_live_, LV_SIZE_CONTENT); lv_obj_update_layout(l_live_); int w = lv_obj_get_width(l_live_) + 36; if (w > room) w = room;
  lv_obj_set_width(l_live_, w - 34); lv_label_set_long_mode(l_live_, LV_LABEL_LONG_DOT);
  lv_obj_remove_flag(live_, LV_OBJ_FLAG_HIDDEN);
  int from = was ? lv_obj_get_width(live_) : 26;
  lv_obj_set_x(live_, left + (room - w) / 2); anim_w(live_, from, w, 260);   // the pill grows/shrinks like a live activity
}
void Ui::status_update() {
  char b[24]; clk::Civil c;
  if (local_time(c)) snprintf(b, sizeof b, "%02u:%02u", c.hour, c.minute); else snprintf(b, sizeof b, "--:--");
  lv_label_set_text(l_time_, b);
  const Power& p = vm.power;
  if (p.gauge_ok) snprintf(b, sizeof b, "%.0f%%", p.pct); else snprintf(b, sizeof b, "--");
  lv_label_set_text(l_batt_, b);
  int fw = p.gauge_ok ? (int)(18 * p.pct / 100.0f + 0.5f) : 0; if (fw < 2) fw = 2;
  lv_obj_set_width(batt_fill_, fw);
  lv_obj_set_style_bg_color(batt_fill_, C(p.charging ? GREEN : (p.gauge_ok && p.pct <= 15) ? RED : TXT), 0);
  // link glyphs: shown only when relevant, bright when active
  const bool show[5] = {vm.sd && vm.settings.logging, vm.sled_linked, vm.lora.ok, vm.gnss.valid || vm.gnss.sats > 0, vm.net.state != NET_OFF || vm.net.ap};
  const uint32_t col[5] = {TXT, TXT, TXT, vm.gnss.valid ? TXT : FAINT, vm.net.state == NET_ONLINE ? TXT : vm.net.ap ? CYAN : FAINT};
  int x = W - M - 74;
  for (int i = 4; i >= 0; i--) {
    if (!show[i]) { lv_obj_add_flag(ic_[i], LV_OBJ_FLAG_HIDDEN); continue; }
    x -= 26; lv_obj_set_x(ic_[i], x); lv_obj_remove_flag(ic_[i], LV_OBJ_FLAG_HIDDEN); lv_obj_set_style_image_recolor(ic_[i], C(col[i]), 0);
  }
  lv_obj_set_style_bg_color(bar_, C(BG), 0);
  static int last_x = 0; if (x != last_x) { last_x = x; live_txt_[0] = 0; } // glyphs moved: refit the pill
  live_update();
}
void Ui::toast(const char* s) {
  if (!toast_) return;
  lv_label_set_text(l_toast_, s); lv_obj_remove_flag(toast_, LV_OBJ_FLAG_HIDDEN); anim_opa(toast_, 0, 255, 160); toast_until_ = now + 2200;
}

// ------------------------------------------------------------------ notifications
static void banner_click(lv_event_t* e) {
  if (!g_ui) return;
  int app = (int)(intptr_t)lv_event_get_user_data(e);
  lv_obj_t* b = (lv_obj_t*)lv_event_get_current_target(e); lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
  g_ui->wake(); if (g_ui->state() == ST_ACTIVE && app >= 0) g_ui->open_app(app);
}
void Ui::notify(uint8_t kind, uint8_t app, const char* title, const char* body, bool banner) {
  if (vm.n_notif == 12) { memmove(vm.notifs + 1, vm.notifs, 11 * sizeof(Notif)); vm.n_notif = 11; }
  else if (vm.n_notif) memmove(vm.notifs + 1, vm.notifs, (size_t)vm.n_notif * sizeof(Notif));
  Notif& n = vm.notifs[0]; n.kind = kind; n.app = app; n.t_ms = now;
  snprintf(n.title, sizeof n.title, "%s", title); snprintf(n.body, sizeof n.body, "%s", body);
  vm.n_notif++; if (vm.unread < 99) vm.unread++;
  // One banner per app per 45 s (an SOS always gets one); the rest go quietly to the list.
  static uint32_t last_banner[APP_COUNT] = {0};
  if (kind != N_SOS && app < APP_COUNT && last_banner[app] && now - last_banner[app] < 45000) banner = false;
  if (banner && st_ == ST_ACTIVE && !shade_open_ && page_id_ != PG_SETUP) { banner_show(n); if (app < APP_COUNT) last_banner[app] = now ? now : 1; }
  if (shade_open_) shade_draw();
  if (st_ != ST_ACTIVE) lock_update();
}
void Ui::banner_show(const Notif& n) {
  if (banner_) lv_obj_delete(banner_);
  const Theme& t = theme();
  banner_ = box(lv_layer_top(), 12, 6, W - 24, 74, n.kind == N_SOS ? 0x3A0B08 : 0x1A1D22, t.radius < 20 ? t.radius : 20);
  lv_obj_set_style_border_width(banner_, 1, 0); lv_obj_set_style_border_color(banner_, C(n.kind == N_SOS ? RED : LINE), 0);
  lv_obj_set_style_shadow_width(banner_, 24, 0); lv_obj_set_style_shadow_color(banner_, C(0x000000), 0); lv_obj_set_style_shadow_opa(banner_, LV_OPA_80, 0);
  lv_obj_t* tile = app_tile(banner_, 12, 11, 52, n.app); lv_obj_set_style_radius(tile, 14, 0);
  lv_obj_set_style_transform_scale(lv_obj_get_child(tile, 0), 200, 0); lv_obj_set_style_transform_pivot_x(lv_obj_get_child(tile, 0), 26, 0); lv_obj_set_style_transform_pivot_y(lv_obj_get_child(tile, 0), 26, 0);
  text_w(banner_, 76, 12, W - 24 - 140, n.title, F16, n.kind == N_SOS ? 0xFF8A80 : TXT);
  text_w(banner_, 76, 38, W - 24 - 92, n.body, F14, DIM);
  text_w(banner_, W - 24 - 64, 13, 52, "now", t.mono_data ? M12 : F12, MUTE, LV_TEXT_ALIGN_RIGHT);
  lv_obj_add_flag(banner_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(banner_, banner_click, LV_EVENT_CLICKED, (void*)(intptr_t)n.app);
  anim_y(banner_, -90, 6, 420, lv_anim_path_overshoot);
  banner_until_ = now + (n.kind == N_SOS ? 8000 : 3800);
}

// ------------------------------------------------------------------ shade
enum { SH_TOGGLES = 8, SH_SLIDER = 8, SH_ACT0 = 9, SH_N_ACT = 3, SH_CLEAR = 12, SH_N = 13 };
static const char* sh_names[SH_TOGGLES] = {"Wi-Fi", "Hotspot", "Torch", "Log", "Haptics", "Siren", "GPS save", "Profile"};
static void shade_click(lv_event_t* e) {
  if (!g_ui) return;
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  Cmd fake; (void)fake;
  // reuse the D-pad path: focus then activate
  extern void shade_activate(Ui& ui, int i, int x_px);
  lv_point_t p; lv_indev_get_point(lv_indev_active(), &p);
  shade_activate(*g_ui, i, p.x);
}
static bool sh_on(const Ui& ui, int i) {
  const Settings& s = ui.vm.settings;
  switch (i) {
    case 0: return s.wifi; case 1: return ui.vm.net.ap; case 2: return ui.page_id() == PG_TORCH; case 3: return s.logging;
    case 4: return s.haptics; case 5: return s.siren; case 6: return s.gnss_psm; default: return s.profile == PROFILE_ADVENTURE;
  }
}
void shade_activate(Ui& ui, int i, int x_px) {
  Settings s = ui.vm.settings; ui.haptic();
  switch (i) {
    case 0: s.wifi = !s.wifi; ui.cmd(Cmd(CMD_NET, s.wifi)); ui.apply(s); break;
    case 1: ui.cmd(Cmd(CMD_HOTSPOT, !ui.vm.net.ap)); break;
    case 2: ui.shade(false); if (ui.page_id() == PG_TORCH) ui.go(PG_HOME, -1); else ui.go(PG_TORCH, 1); return;
    case 3: s.logging = !s.logging; ui.apply(s); break;
    case 4: s.haptics = !s.haptics; ui.apply(s); break;
    case 5: s.siren = !s.siren; ui.apply(s); break;
    case 6: s.gnss_psm = !s.gnss_psm; ui.apply(s); break;
    case 7: s.profile = s.profile ? PROFILE_HACKER : PROFILE_ADVENTURE; ui.apply(s); ui.toast(s.profile ? "Adventure profile" : "Hackerspace profile"); return;
    case SH_SLIDER: {
      int v = x_px >= 0 ? (x_px - M - 10) * 100 / (W - 2 * M - 20) : s.brightness;
      v = v < 10 ? 10 : v > 100 ? 100 : (v + 4) / 5 * 5; s.brightness = (uint8_t)v; ui.apply(s); break;
    }
    case SH_ACT0: ui.cmd(Cmd(CMD_WP_SAVE)); ui.toast("Waypoint saved here"); break;
    case SH_ACT0 + 1: { Cmd c(CMD_SEND_TEXT); snprintf(c.text, sizeof c.text, "OK"); ui.cmd(c); ui.toast("Sent \"OK\" to the mesh"); break; }
    case SH_ACT0 + 2: ui.shade(false); ui.cmd(Cmd(CMD_SLEEP)); return;
    case SH_CLEAR: ui.vm.n_notif = 0; ui.vm.unread = 0; break;
  }
  ui.shade(true); // redraw in place
}
void Ui::shade_set_y(int y) { shade_y_ = y; lv_obj_set_y(shade_, y); }
void Ui::shade_build() {
  shade_ = box(lv_layer_top(), 0, -H, W, H, BG, 0);
  shade_body_ = box(shade_, 0, 0, W, H, BG, 0);
  box(shade_, W / 2 - 24, H - 12, 48, 5, 0x6A707A, 3); // grabber
}
void Ui::shade_draw() {
  lv_obj_clean(shade_body_); const Theme& t = theme(); char b[64]; clk::Civil c;
  lv_obj_t* s = shade_body_;
  // header: time, date, battery, profile chip
  if (local_time(c)) { snprintf(b, sizeof b, "%02u:%02u", c.hour, c.minute); text(s, M + 4, 10, b, F36, TXT);
    snprintf(b, sizeof b, "%s %u %s", clk::weekday_short(c.weekday), c.day, clk::month_short(c.month)); text(s, M + 120, 24, b, F16, DIM); }
  else text(s, M + 4, 10, "--:--", F36, TXT);
  if (vm.power.gauge_ok) { snprintf(b, sizeof b, "%.0f%%%s", vm.power.pct, vm.power.charging ? "  charging" : ""); text_w(s, W - M - 160, 24, 156, b, t.mono_data ? M14 : F16, vm.power.charging ? GREEN : TXT, LV_TEXT_ALIGN_RIGHT); }
  // toggles
  static const lv_image_dsc_t* gi[SH_TOGGLES] = {&ic30_wifi, &ic30_web, &ic30_torch, &ic30_sd, &ic30_vibrate, &ic30_speaker, &ic30_position, &ic30_terminal};
  int tw = 84, gap = (W - 4 * tw - 2 * 19) / 3;
  for (int i = 0; i < SH_TOGGLES; i++) {
    bool on = sh_on(*this, i), f = shade_sel_ == i;
    lv_obj_t* tile = box(s, 19 + (i % 4) * (tw + gap), 66 + (i / 4) * (tw + 12), tw, tw, on ? t.acc : CARD_HI, t.radius < 22 ? t.radius + 4 : 24);
    if (f) { lv_obj_set_style_outline_width(tile, 3, 0); lv_obj_set_style_outline_color(tile, C(TXT), 0); lv_obj_set_style_outline_pad(tile, 2, 0); }
    const lv_image_dsc_t* g = gi[i]; if (i == 7) g = vm.settings.profile ? &ic30_mountain : &ic30_terminal;
    glyph(tile, g, (tw - 30) / 2, 14, on ? t.acc_ink : TXT);
    const char* nm = i == 7 ? (vm.settings.profile ? "Adventure" : "Hacker") : sh_names[i];
    text_w(tile, 2, 54, tw - 4, nm, F12, on ? t.acc_ink : DIM, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(tile, shade_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
  }
  // brightness slider
  int sy = 66 + 2 * (tw + 12) + 4, sw = W - 2 * M;
  lv_obj_t* tr = box(s, M, sy, sw, 54, CARD_HI, 27);
  if (shade_sel_ == SH_SLIDER) { lv_obj_set_style_outline_width(tr, 3, 0); lv_obj_set_style_outline_color(tr, C(TXT), 0); lv_obj_set_style_outline_pad(tr, 2, 0); }
  int fw = 54 + (sw - 54) * (vm.settings.brightness - 10) / 90;
  lv_obj_t* fill = box(tr, 0, 0, fw, 54, TXT, 27); (void)fill;
  glyph(tr, &ic30_brightness, 14, 12, 0x15171C);
  snprintf(b, sizeof b, "%u%%", vm.settings.brightness); text_w(tr, sw - 70, 16, 56, b, t.mono_data ? M14 : F16, fw > sw - 60 ? 0x15171C : DIM, LV_TEXT_ALIGN_RIGHT);
  lv_obj_add_flag(tr, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(tr, shade_click, LV_EVENT_CLICKED, (void*)(intptr_t)SH_SLIDER);
  // quick actions
  static const char* act[SH_N_ACT] = {"Save waypoint", "Send OK", "Sleep"};
  int ax = M, ay = sy + 68;
  for (int i = 0; i < SH_N_ACT; i++) {
    bool f = shade_sel_ == SH_ACT0 + i;
    lv_obj_t* p = pill(s, ax, ay, act[i], f ? t.acc : CARD_HI, f ? t.acc_ink : TXT, F14);
    lv_obj_set_style_pad_ver(p, 9, 0); lv_obj_set_style_pad_hor(p, 14, 0);
    lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(p, shade_click, LV_EVENT_CLICKED, (void*)(intptr_t)(SH_ACT0 + i));
    lv_obj_update_layout(p); ax += lv_obj_get_width(p) + 8;
  }
  // notifications
  int ny = ay + 52;
  section(s, ny, "Notifications");
  if (vm.n_notif) {
    bool f = shade_sel_ == SH_CLEAR;
    lv_obj_t* p = pill(s, 0, ny - 4, "Clear", f ? t.acc : CARD_HI, f ? t.acc_ink : DIM, F12);
    lv_obj_update_layout(p); lv_obj_set_x(p, W - M - lv_obj_get_width(p));
    lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(p, shade_click, LV_EVENT_CLICKED, (void*)(intptr_t)SH_CLEAR);
  }
  ny += 26;
  if (!vm.n_notif) text_w(s, 0, ny + 18, W, "No notifications", F14, MUTE, LV_TEXT_ALIGN_CENTER);
  int shown = 0;
  for (int i = 0; i < vm.n_notif && ny + 64 < H - 18; i++, shown++) {
    const Notif& n = vm.notifs[i];
    lv_obj_t* r = box(s, M, ny, W - 2 * M, 58, n.kind == N_SOS ? 0x3A0B08 : CARD, 16);
    lv_obj_t* tile = app_tile(r, 9, 9, 40, n.app); lv_obj_set_style_radius(tile, 11, 0);
    lv_obj_t* gl = lv_obj_get_child(tile, 0); lv_obj_set_pos(gl, -6, -6); lv_obj_set_style_transform_scale(gl, 160, 0); lv_obj_set_style_transform_pivot_x(gl, 26, 0); lv_obj_set_style_transform_pivot_y(gl, 26, 0);
    text_w(r, 60, 8, W - 2 * M - 130, n.title, F14, TXT);
    text_w(r, 60, 31, W - 2 * M - 72, n.body, F12, DIM);
    uint32_t ago = (now - n.t_ms) / 60000; snprintf(b, sizeof b, ago ? "%lum" : "now", (unsigned long)ago);
    text_w(r, W - 2 * M - 64, 9, 54, b, t.mono_data ? M12 : F12, MUTE, LV_TEXT_ALIGN_RIGHT);
    ny += 64;
  }
  if (vm.n_notif > shown) { snprintf(b, sizeof b, "+%d more", vm.n_notif - shown); text_w(s, 0, H - 34, W, b, F12, MUTE, LV_TEXT_ALIGN_CENTER); }
}
void Ui::shade(bool open) {
  if (open) { if (!shade_open_) vm.unread = 0; shade_draw(); if (!shade_open_ || shade_y_ != 0) anim_y(shade_, shade_y_, 0, 300); shade_open_ = true; shade_y_ = 0; }
  else if (shade_open_ || shade_y_ != -H) { anim_y(shade_, shade_y_, -H, 260); shade_open_ = false; shade_y_ = -H; }
}
void Ui::shade_key(uint16_t k) {
  int s = shade_sel_ < 0 ? 0 : shade_sel_;
  if (k & (KEY_B | KEY_SIDE)) { shade(false); return; }
  if (shade_sel_ < 0) { shade_sel_ = 0; shade_draw(); return; } // first key after a touch-open just shows focus
  if (k & KEY_A) { shade_activate(*this, s, -1); return; }
  if (s < SH_TOGGLES) {
    if (k & KEY_RIGHT) s = (s % 4 < 3) ? s + 1 : s;
    if (k & KEY_LEFT) s = (s % 4 > 0) ? s - 1 : s;
    if (k & KEY_DOWN) s = s < 4 ? s + 4 : SH_SLIDER;
    if (k & KEY_UP) { if (s < 4) { shade(false); return; } s -= 4; }
  } else if (s == SH_SLIDER) {
    if (k & (KEY_LEFT | KEY_RIGHT)) { Settings st = vm.settings; int v = st.brightness + ((k & KEY_RIGHT) ? 10 : -10); st.brightness = (uint8_t)(v < 10 ? 10 : v > 100 ? 100 : v); apply(st); }
    if (k & KEY_DOWN) s = SH_ACT0;
    if (k & KEY_UP) s = 4;
  } else if (s < SH_CLEAR) {
    if (k & KEY_RIGHT) s = s < SH_ACT0 + SH_N_ACT - 1 ? s + 1 : s;
    if (k & KEY_LEFT) s = s > SH_ACT0 ? s - 1 : s;
    if (k & KEY_UP) s = SH_SLIDER;
    if ((k & KEY_DOWN) && vm.n_notif) s = SH_CLEAR;
  } else if (k & KEY_UP) s = SH_ACT0;
  if (s != shade_sel_) { shade_sel_ = s; haptic(); }
  shade_draw();
}

// ------------------------------------------------------------------ quick actions sheet
struct Action { const char* label; int code; };
static int actions_for(const Ui& ui, int app, Action* a) {
  int n = 0;
  auto add = [&](const char* l, int c) { a[n].label = l; a[n].code = c; n++; };
  add("Open", 0);
  switch (app) {
    case APP_POSITION: add("Save waypoint here", 1); add("Check in on the mesh", 2); break;
    case APP_NAVIGATE: add(ui.nav_active ? "End navigation" : "Navigate to last waypoint", 3); break;
    case APP_MESH: add("Send: OK", 4); add("Send: On my way", 5); add("Send: Need help, not urgent", 6); break;
    case APP_WIFI: add("Channel congestion", 7); add("Bluetooth devices", 8); break;
    case APP_WEATHER: add("Refresh forecast", 9); break;
    case APP_CONSOLE: add(ui.vm.net.ap ? "Stop hotspot" : "Start hotspot", 10); break;
    case APP_TORCH: add("Red light (night vision)", 11); break;
    case APP_UPDATES: add("Check now", 12); break;
    case APP_SETTINGS: add(ui.vm.settings.aod ? "Turn always-on off" : "Turn always-on on", 13); add("Switch profile", 14); break;
    default: break;
  }
  return n;
}
static void menu_click(lv_event_t* e) { if (!g_ui) return; extern void menu_run(Ui& ui, int idx); menu_run(*g_ui, (int)(intptr_t)lv_event_get_user_data(e)); }
static int g_menu_app = -1;
void menu_run(Ui& ui, int idx) {
  Action a[6]; int n = actions_for(ui, g_menu_app, a); if (idx < 0 || idx >= n) return;
  int app = g_menu_app; int code = a[idx].code; Settings s = ui.vm.settings; Cmd c(CMD_SEND_TEXT);
  ui.quick_actions(-1); ui.haptic(HAP_DOUBLE);
  switch (code) {
    case 0: ui.open_app(app); break;
    case 1: ui.cmd(Cmd(CMD_WP_SAVE)); ui.toast("Waypoint saved here"); break;
    case 2: snprintf(c.text, sizeof c.text, "Check-in: all good"); ui.cmd(c); ui.toast("Check-in sent"); break;
    case 3: if (ui.nav_active) ui.nav_active = false; else if (ui.vm.n_wp) { ui.nav_target = ui.vm.n_wp - 1; ui.nav_active = true; ui.go(PG_NAV, 1); } break;
    case 4: case 5: case 6: { static const char* m[] = {"OK", "On my way", "Need help, not urgent"}; snprintf(c.text, sizeof c.text, "%s", m[code - 4]); ui.cmd(c); ui.toast("Sent to the mesh"); break; }
    case 7: ui.open_app(APP_CHANNELS); break;
    case 8: ui.open_app(APP_BLE); break;
    case 9: ui.cmd(Cmd(CMD_WX_REFRESH)); ui.toast("Refreshing forecast"); break;
    case 10: ui.cmd(Cmd(CMD_HOTSPOT, !ui.vm.net.ap)); break;
    case 11: ui.torch_red = true; ui.go(PG_TORCH, 1); break;
    case 12: ui.cmd(Cmd(CMD_OTA, 0)); ui.open_app(APP_UPDATES); break;
    case 13: s.aod = !s.aod; ui.apply(s); break;
    case 14: s.profile = s.profile ? PROFILE_HACKER : PROFILE_ADVENTURE; ui.apply(s); break;
  }
}
void Ui::quick_actions(int app) {
  if (menu_) { lv_obj_delete(menu_); menu_ = nullptr; }
  menu_app_ = g_menu_app = app; if (app < 0) return;
  Action a[6]; menu_n_ = actions_for(*this, app, a); if (menu_sel_ >= menu_n_) menu_sel_ = 0;
  const Theme& t = theme(); haptic(HAP_DOUBLE);
  menu_ = box(lv_layer_top(), 0, 0, W, H, 0x000000, 0); lv_obj_set_style_bg_opa(menu_, LV_OPA_70, 0);
  lv_obj_add_flag(menu_, LV_OBJ_FLAG_CLICKABLE);
  int h = 92 + menu_n_ * 58;
  lv_obj_t* sh = box(menu_, 10, H - h - 10, W - 20, h, 0x16181C, 28);
  lv_obj_set_style_border_width(sh, 1, 0); lv_obj_set_style_border_color(sh, C(LINE), 0);
  app_tile(sh, 16, 16, 56, app);
  lv_obj_set_style_transform_scale(lv_obj_get_child(lv_obj_get_child(sh, 0), 0), 256, 0);
  lv_obj_t* g = lv_obj_get_child(lv_obj_get_child(sh, 0), 0); lv_obj_set_pos(g, 2, 2);
  text(sh, 86, 22, app_info(app).name, F24, TXT); text(sh, 86, 52, "Quick actions", F12, DIM);
  for (int i = 0; i < menu_n_; i++) {
    bool f = i == menu_sel_;
    lv_obj_t* r = box(sh, 12, 84 + i * 58, W - 44, 52, f ? t.acc : CARD_HI, 16);
    text(r, 18, 14, a[i].label, F16, f ? t.acc_ink : TXT);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(r, menu_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
  }
  anim_y(sh, H, H - h - 10, 300);
}
void Ui::menu_key(uint16_t k) {
  if (k & (KEY_B | KEY_SIDE)) { menu_sel_ = 0; quick_actions(-1); return; }
  if ((k & KEY_DOWN) && menu_sel_ < menu_n_ - 1) { menu_sel_++; haptic(); }
  else if ((k & KEY_UP) && menu_sel_ > 0) { menu_sel_--; haptic(); }
  else if (k & KEY_A) { int s = menu_sel_; menu_sel_ = 0; menu_run(*this, s); return; }
  else return;
  // redraw without the entrance animation
  int app = menu_app_; lv_obj_delete(menu_); menu_ = nullptr; quick_actions(app);
  lv_anim_delete(lv_obj_get_child(menu_, 0), nullptr); lv_obj_set_y(lv_obj_get_child(menu_, 0), H - (92 + menu_n_ * 58) - 10);
}

// ------------------------------------------------------------------ lock screen / always-on
void Ui::lock_build() {
  lock_ = box(lv_layer_top(), 0, 0, W, H, BG, 0); lv_obj_add_flag(lock_, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(lock_, LV_OBJ_FLAG_CLICKABLE);
  lk_clock_box_ = box(lock_, 0, 0, W, 300, BG, 0); lv_obj_set_style_bg_opa(lk_clock_box_, LV_OPA_TRANSP, 0);
  lk_time_ = text_w(lk_clock_box_, 0, 46, W, "--:--", CLK, TXT, LV_TEXT_ALIGN_CENTER);
  lk_date_ = text_w(lk_clock_box_, 0, 168, W, "", F20, DIM, LV_TEXT_ALIGN_CENTER);
  lk_cx_ = box(lk_clock_box_, 0, 206, W, 92, BG, 0); lv_obj_set_style_bg_opa(lk_cx_, LV_OPA_TRANSP, 0);
  lk_list_ = box(lock_, 0, 306, W, 150, BG, 0); lv_obj_set_style_bg_opa(lk_list_, LV_OPA_TRANSP, 0);
  lk_hint_ = text_w(lock_, 0, H - 40, W, "", theme().mono_data ? M12 : F14, MUTE, LV_TEXT_ALIGN_CENTER);
  box(lock_, W / 2 - 60, H - 9, 120, 5, 0x6A707A, 3);
}
static void complication(Ui& ui, lv_obj_t* parent, int x, int w, uint8_t cx, bool aod) {
  const Vm& v = ui.vm; char val[24] = "--", lab[28] = ""; const lv_image_dsc_t* g = nullptr; uint32_t gc = aod ? DIM : theme().acc;
  switch (cx) {
    case CX_WEATHER: {
      const Weather* wx = v.weather();
      if (wx) { fmt_temp(val, sizeof val, wx->temp_c, v.settings.metric); g = wx_icon(wx->cond, false);
        if (wx->online) snprintf(lab, sizeof lab, "%s", wx::cond_name((wx::Cond)wx->cond));
        else snprintf(lab, sizeof lab, "%.0f hPa %s", wx->hpa, wx->trend < 0 ? "falling" : wx->trend > 0 ? "rising" : "steady"); }
      else { g = &wx40_partly; snprintf(lab, sizeof lab, "Weather"); }
      break;
    }
    case CX_WAYPOINT: {
      g = &ic30_pin; int i = v.settings.wp_target < v.n_wp ? v.settings.wp_target : v.n_wp - 1;
      if (i >= 0 && v.gnss.valid) { const Waypoint& w = v.wps[i]; fmt_dist(val, sizeof val, geo::distance_m(v.gnss.lat, v.gnss.lon, w.lat, w.lon), v.settings.metric);
        snprintf(lab, sizeof lab, "%s %s", w.name, geo::cardinal(geo::bearing_deg(v.gnss.lat, v.gnss.lon, w.lat, w.lon))); }
      else snprintf(lab, sizeof lab, i >= 0 ? "No GPS fix" : "No waypoint");
      break;
    }
    case CX_BATTERY: g = nullptr; if (v.power.gauge_ok) { snprintf(val, sizeof val, "%.0f%%", v.power.pct);
        if (v.power.charging) snprintf(lab, sizeof lab, "Charging"); else if (v.power.minutes > 0) snprintf(lab, sizeof lab, "%dh %02dm left", v.power.minutes / 60, v.power.minutes % 60); else snprintf(lab, sizeof lab, "Battery"); }
      else snprintf(lab, sizeof lab, "Battery");
      break;
    case CX_MESH: g = &ic30_mesh; snprintf(val, sizeof val, "%u", v.lora.rx_count); snprintf(lab, sizeof lab, v.lora.ok ? "heard on mesh" : "Mesh off"); break;
    case CX_SATS: g = &ic30_position; snprintf(val, sizeof val, "%u", v.gnss.sats); snprintf(lab, sizeof lab, v.gnss.valid ? "sats, fixed" : "sats, searching"); break;
    case CX_RF: g = &ic30_sweep; if (v.have_rf) snprintf(val, sizeof val, "%.0f", v.rf.dbm); snprintf(lab, sizeof lab, v.sled_linked ? "dBm broadband" : "Sled not attached"); break;
    case CX_NET: g = &ic30_wifi; snprintf(val, sizeof val, "%s", v.net.state == NET_ONLINE ? "Online" : v.net.ap ? "Hotspot" : "Off"); snprintf(lab, sizeof lab, "%.27s", v.net.state == NET_ONLINE ? v.net.ssid : "Wi-Fi"); break;
    default: return;
  }
  if (g) { int gw = (int)g->header.w; glyph(parent, g, x + (w - gw) / 2, 30 - gw / 2 > 0 ? 30 - gw / 2 : 0, gc); }
  else { // battery: drawn
    lv_obj_t* f; lv_obj_t* b = make_batt(parent, x + w / 2 - 15, 14, &f); (void)b;
    int fw = v.power.gauge_ok ? (int)(18 * v.power.pct / 100.0f + 0.5f) : 2; lv_obj_set_width(f, fw < 2 ? 2 : fw); lv_obj_set_style_bg_color(f, C(v.power.charging ? GREEN : aod ? DIM : TXT), 0);
  }
  text_w(parent, x, 44, w, val, DATA(20), aod ? 0xC8CCD2 : TXT, LV_TEXT_ALIGN_CENTER);
  text_w(parent, x + 2, 70, w - 4, lab, F12, aod ? 0x6A707A : DIM, LV_TEXT_ALIGN_CENTER);
}
void Ui::lock_update() {
  if (!lock_ || st_ == ST_ACTIVE) return;
  bool aod = st_ == ST_AOD; char b[48]; clk::Civil c;
  if (local_time(c)) { snprintf(b, sizeof b, "%02u:%02u", c.hour, c.minute); lv_label_set_text(lk_time_, b);
    snprintf(b, sizeof b, "%s %u %s", clk::weekday_short(c.weekday), c.day, clk::month_short(c.month)); lv_label_set_text(lk_date_, b); }
  else { lv_label_set_text(lk_time_, "--:--"); lv_label_set_text(lk_date_, "Waiting for GPS or network time"); }
  // AOD: grey clock at low brightness, accent-coloured when awake
  lv_obj_set_style_text_color(lk_time_, C(aod ? 0xB8BCC4 : TXT), 0);
  lv_obj_clean(lk_cx_);
  int n = 0; for (int i = 0; i < 3; i++) if (vm.settings.aod_cx[i] != CX_NONE) n++;
  int w = n ? (W - 2 * M) / n : 0, k = 0;
  for (int i = 0; i < 3; i++) if (vm.settings.aod_cx[i] != CX_NONE) complication(*this, lk_cx_, M + (k++) * w, w, vm.settings.aod_cx[i], aod);
  lv_obj_clean(lk_list_);
  if (vm.sos) {
    lv_obj_t* r = box(lk_list_, M, 0, W - 2 * M, 64, 0x3A0B08, 18);
    text(r, 16, 10, "SOS ACTIVE", F20, 0xFF8A80); text(r, 16, 38, "Position sent every 20 s. Hold SOS 2 s to cancel.", F12, TXT);
  } else if (aod) {
    if (vm.unread) { snprintf(b, sizeof b, "%u new", vm.unread); glyph(lk_list_, &ic20_bell, W / 2 - 34, 12, DIM); text(lk_list_, W / 2 - 8, 10, b, F14, DIM); }
  } else {
    int y = 0;
    for (int i = 0; i < vm.n_notif && i < 2; i++) {
      const Notif& nf = vm.notifs[i];
      lv_obj_t* r = box(lk_list_, M, y, W - 2 * M, 64, 0x15171B, 18);
      lv_obj_t* tile = app_tile(r, 10, 10, 44, nf.app); lv_obj_set_style_radius(tile, 12, 0);
      lv_obj_t* gl = lv_obj_get_child(tile, 0); lv_obj_set_pos(gl, -4, -4); lv_obj_set_style_transform_scale(gl, 180, 0); lv_obj_set_style_transform_pivot_x(gl, 26, 0); lv_obj_set_style_transform_pivot_y(gl, 26, 0);
      text_w(r, 66, 10, W - 2 * M - 80, nf.title, F14, TXT); text_w(r, 66, 34, W - 2 * M - 80, nf.body, F12, DIM);
      y += 72;
    }
  }
  const char* h = vm.locked ? "Locked by the side switch.  SOS still works." : aod ? "" : "Swipe up or press A to open";
  lv_label_set_text(lk_hint_, h);
  if (aod) { // burn-in protection: move the clock block a few pixels every minute
    uint32_t m = now / 60000; lv_obj_set_pos(lk_clock_box_, (int)(m * 7 % 13) - 6, (int)(m * 5 % 11) - 5);
  } else lv_obj_set_pos(lk_clock_box_, 0, 0);
}
void Ui::set_state(UiState s) {
  if (s == st_) { lock_update(); return; }
  UiState was = st_; st_ = s;
  if (s == ST_ACTIVE) {
    cmd(Cmd(CMD_BRIGHTNESS, page_id_ == PG_TORCH ? 100 : -1));
    anim_y(lock_, 0, -H, 320, lv_anim_path_ease_in_out); // unlock: the lock screen lifts away
    lv_obj_remove_flag(stage, LV_OBJ_FLAG_HIDDEN);
    idle_at_ = now + vm.settings.screen_off_s * 1000u;
    return;
  }
  if (menu_) quick_actions(-1);
  if (shade_open_) shade(false);
  if (banner_) { lv_obj_delete(banner_); banner_ = nullptr; banner_until_ = 0; }
  lv_anim_delete(lock_, nullptr); lv_obj_set_y(lock_, 0); lv_obj_remove_flag(lock_, LV_OBJ_FLAG_HIDDEN);
  if (s == ST_AOD) {
    lv_obj_add_flag(stage, LV_OBJ_FLAG_HIDDEN); // nothing else is drawn in AOD
    cmd(Cmd(CMD_BRIGHTNESS, vm.settings.aod ? 12 : 0));
    if (was == ST_ACTIVE) cmd(Cmd(CMD_SET_MODE, MODE_AOD));
  } else {
    cmd(Cmd(CMD_BRIGHTNESS, -1));
    if (was == ST_ACTIVE) anim_opa(lock_, 0, 255, 200);
  }
  lock_update();
}
void Ui::wake() {
  idle_at_ = now + vm.settings.screen_off_s * 1000u;
  if (st_ == ST_AOD) set_state(ST_LOCK);
}

// ------------------------------------------------------------------ gestures (touch)
// Edge swipes, all following the finger:
//   from the top edge down     pull the shade
//   on the open shade, up      close it
//   from the bottom edge up    home (or unlock on the lock screen)
//   from the left edge right   back
void Ui::gestures() {
  lv_indev_t* in = nullptr;
  for (lv_indev_t* i = lv_indev_get_next(nullptr); i; i = lv_indev_get_next(i)) if (lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER) { in = i; break; }
  if (!in) return;
  lv_point_t p; lv_indev_get_point(in, &p); bool down = lv_indev_get_state(in) == LV_INDEV_STATE_PRESSED;
  enum { G_NONE, G_SHADE, G_SHADE_CLOSE, G_HOME, G_BACK, G_UNLOCK, G_IGNORE };
  if (down && !g_mode_) {
    g_start_ = p; g_t0_ = now; wake();
    if (st_ == ST_AOD) g_mode_ = G_IGNORE;
    else if (st_ == ST_LOCK) g_mode_ = (p.y > H - 120 && !vm.locked) ? G_UNLOCK : G_IGNORE;
    else if (menu_) g_mode_ = G_IGNORE;
    else if (shade_open_) g_mode_ = G_SHADE_CLOSE;
    else if (p.y < 36 && page_id_ != PG_SETUP) g_mode_ = G_SHADE;
    else if (p.y > H - 34 && !page_->home() && page_id_ != PG_SETUP) g_mode_ = G_HOME;
    else if (p.x < 18 && !page_->home()) g_mode_ = G_BACK;
    else g_mode_ = G_IGNORE;
    if (g_mode_ == G_SHADE) { shade_sel_ = -1; shade_draw(); }
  }
  int dx = p.x - g_start_.x, dy = p.y - g_start_.y;
  if (down) {
    if (g_mode_ == G_SHADE) { int y = dy - H; if (y > 0) y = 0; shade_set_y(y); }
    else if (g_mode_ == G_SHADE_CLOSE && dy < -12) { shade_set_y(dy > 0 ? 0 : dy); }
    else if (g_mode_ == G_UNLOCK && dy < 0) lv_obj_set_y(lock_, dy);
    else if (g_mode_ == G_HOME && dy < 0) lv_obj_set_y(page_->scr, dy / 4);
    g_last_ = p; return;
  }
  if (!g_mode_) return;
  uint32_t dt = now - g_t0_ + 1; float vy = (float)dy / (float)dt; // px per ms
  switch (g_mode_) {
    case G_SHADE: if (dy > H / 3 || vy > 0.6f) { shade_open_ = false; shade(true); } else { shade_open_ = true; shade(false); } break;
    case G_SHADE_CLOSE: if (dy < -H / 4 || vy < -0.6f) shade(false); else if (dy < -12) { shade_open_ = false; shade(true); } break;
    case G_UNLOCK: if ((dy < -H / 4 || vy < -0.5f) && !vm.locked) set_state(ST_ACTIVE); else anim_y(lock_, lv_obj_get_y(lock_), 0, 220); break;
    case G_HOME: lv_obj_set_y(page_->scr, 0); if (dy < -60) { haptic(); go(PG_HOME, -1); } break;
    case G_BACK: if (dx > 70) { haptic(); if (page_) page_->key(KEY_B); } break;
  }
  g_mode_ = 0;
}

// ------------------------------------------------------------------ input
void Ui::key(uint16_t k, uint16_t keys) {
  if (st_ == ST_AOD) { if (k & ~KEY_SOS) set_state(ST_LOCK); return; }
  if (st_ == ST_LOCK) {
    if ((k & KEY_A) && !vm.locked) set_state(ST_ACTIVE);
    else if (k & (KEY_B | KEY_SIDE)) set_state(ST_AOD);
    return;
  }
  wake();
  if (menu_) { menu_key(k); return; }
  if (shade_open_) { shade_key(k); return; }
  if (banner_ && (k & KEY_A) && (keys & KEY_UP)) {} // reserved
  if (k & KEY_SIDE) { haptic(); if (page_->home()) { shade_sel_ = 0; shade(true); } else go(PG_HOME, -1); return; }
  if ((k & KEY_A) && page_->holdable()) { a_down_ = now; a_held_ = false; return; } // decided on release / hold
  page_->key(k);
}
void Ui::on_event(const Event& e, uint32_t& what) {
  switch (e.type) {
    case EV_KEY: {
      bool locked = (e.u.key.keys & KEY_LOCK) != 0;
      if (locked != vm.locked) { vm.locked = locked; set_state(locked ? ST_AOD : ST_LOCK); }
      held_ = (uint16_t)(e.u.key.keys & (KEY_UP | KEY_DOWN)); repeat_at_ = now + 450; keys_ = e.u.key.keys;
      if ((e.u.key.released & KEY_A) && a_down_) { bool was_hold = a_held_; a_down_ = 0; if (!was_hold && st_ == ST_ACTIVE && page_) page_->key(KEY_A); }
      if (e.u.key.pressed & KEY_B) { b_down_ = now; b_held_ = false; }
      if (e.u.key.released & KEY_B) b_down_ = 0;
      uint16_t k = (uint16_t)(e.u.key.pressed & ~KEY_LOCK);
      if (!k) break;
      if (vm.locked && st_ != ST_ACTIVE) { if (st_ == ST_AOD && (k & ~KEY_SOS)) set_state(ST_LOCK); break; }
      key(k, e.u.key.keys);
      break;
    }
    case EV_POWER: {
      float before = vm.power.pct; bool had = vm.power.gauge_ok; vm.power = e.u.power; what |= R_POWER;
      if (had && !e.u.power.charging) for (float th : {20.0f, 10.0f}) if (before > th && e.u.power.pct <= th) {
        char b[48]; snprintf(b, sizeof b, "%.0f%% left. Mesh and SOS keep working to 3%%.", e.u.power.pct); notify(N_BATTERY, APP_SETTINGS, "Battery low", b); }
      break;
    }
    case EV_ENV: vm.env = e.u.env; break;
    case EV_GNSS:
      vm.gnss = e.u.gnss; what |= R_GNSS;
      if (e.u.gnss.time_valid && vm.time_src != 2 && e.u.gnss.year >= 2024) {
        vm.utc = clk::to_unix(e.u.gnss.year, e.u.gnss.month, e.u.gnss.day, e.u.gnss.hour, e.u.gnss.minute, e.u.gnss.second); vm.utc_ms = now; vm.time_src = 1; what |= R_TIME;
      }
      break;
    case EV_SETTINGS: {
      bool first = !vm.settings.setup_done && e.u.settings.setup_done;
      uint8_t prof = vm.settings.profile; vm.settings = e.u.settings; what |= R_SET;
      if (prof != vm.settings.profile) { Settings s = vm.settings; vm.settings.profile = prof; apply(s); }
      if (first && page_id_ == PG_SETUP) go(PG_HOME, 0);
      else if (!vm.settings.setup_done && page_id_ == PG_HOME) go(PG_SETUP, 0);
      if (vm.settings.wifi && vm.net.state == NET_OFF) cmd(Cmd(CMD_NET, 1));
      break;
    }
    case EV_STORAGE: vm.sd = e.u.storage.mounted; what |= R_LINK; break;
    case EV_LORA: vm.lora = e.u.lora; what |= R_LINK; break;
    case EV_SLED:
      if (e.u.sled.linked != vm.sled_linked) notify(N_SLED, APP_SWEEP, e.u.sled.linked ? "UNO Q sled attached" : "Sled detached", e.u.sled.linked ? "Sweep and 5 GHz scanning are available" : "Sweep needs the sled", page_id_ != PG_SWEEP);
      vm.sled_linked = e.u.sled.linked; if (!vm.sled_linked) { vm.have_rf = false; vm.rf_n = 0; } what |= R_LINK; break;
    case EV_SOS: vm.sos = e.u.sos.active; toast(vm.sos ? "SOS started: broadcasting position" : "SOS cancelled"); what |= R_GNSS | R_LINK; if (st_ != ST_ACTIVE) lock_update(); break;
    case EV_RF: {
      uint8_t before = vm.rf.state; vm.rf = e.u.rf; vm.have_rf = true;
      if ((uint32_t)(e.t_ms - t_tick_) >= 250) { // 4 points per second in the history
        t_tick_ = e.t_ms;
        if (vm.rf_n == 120) { memmove(vm.rf_hist, vm.rf_hist + 1, 119 * sizeof(float)); memmove(vm.rf_exp, vm.rf_exp + 1, 119 * sizeof(float)); vm.rf_n = 119; }
        vm.rf_hist[vm.rf_n] = e.u.rf.dbm; vm.rf_exp[vm.rf_n] = e.u.rf.expected_dbm; vm.rf_n++;
      }
      static uint32_t last_rf_note = 0;
      if (before != corr::UNEXPLAINED && e.u.rf.state == corr::UNEXPLAINED && page_id_ != PG_SWEEP && (uint32_t)(now - last_rf_note) > 60000) {
        last_rf_note = now; char b[60]; snprintf(b, sizeof b, "+%.0f dB above what nearby Wi-Fi/BLE explains", e.u.rf.excess_db); notify(N_RF, APP_SWEEP, "Unexplained RF energy", b);
      }
      what |= R_RF; break;
    }
    case EV_MSG:
      if (vm.n_msg == 12) { memmove(vm.msgs, vm.msgs + 1, 11 * sizeof(MeshText)); vm.n_msg = 11; }
      vm.msgs[vm.n_msg++] = e.u.msg; what |= R_MSG;
      if (!e.u.msg.mine) {
        char t[32]; snprintf(t, sizeof t, e.u.msg.sos ? "SOS from %04X" : "Mesh  %04X", e.u.msg.src);
        notify(e.u.msg.sos ? N_SOS : N_MSG, APP_MESH, t, e.u.msg.text, !(page_id_ == PG_BEACON && beacon_tab == 2));
        haptic(e.u.msg.sos ? HAP_ALERT : HAP_BUZZ);
        if (e.u.msg.sos && st_ == ST_AOD) set_state(ST_LOCK); // an SOS from someone else wakes the screen
      }
      break;
    case EV_WIFI_BEGIN: vm.n_ap_in = 0; break;
    case EV_WIFI_AP: if (vm.n_ap_in < 32) vm.aps_in[vm.n_ap_in++] = e.u.ap; break;
    case EV_WIFI_END: {
      for (int i = 1; i < vm.n_ap_in; i++) { WifiAp t = vm.aps_in[i]; int j = i - 1; while (j >= 0 && vm.aps_in[j].rssi < t.rssi) { vm.aps_in[j + 1] = vm.aps_in[j]; j--; } vm.aps_in[j + 1] = t; }
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
    case EV_NET: {
      uint8_t before = vm.net.state; vm.net = e.u.net; what |= R_NET | R_LINK;
      if (before != NET_ONLINE && vm.net.state == NET_ONLINE) { char b[60]; snprintf(b, sizeof b, "%.20s  %d dBm. Clock and forecast synced.", vm.net.ssid, vm.net.rssi); notify(N_NET, APP_CONSOLE, "Wi-Fi connected", b, false); }
      break;
    }
    case EV_TIME:
      if (e.u.time.utc) { vm.utc = e.u.time.utc; vm.utc_ms = now; vm.time_src = e.u.time.source; }
      if (e.u.time.tz_min != vm.settings.tz_min && !e.u.time.utc) { Settings s = vm.settings; s.tz_min = e.u.time.tz_min; apply(s); }
      what |= R_TIME; break;
    case EV_WEATHER:
      if (e.u.wx.online) { vm.wx_online = e.u.wx; vm.have_wx_online = true; vm.wx_online_ms = now; } else { vm.wx_local = e.u.wx; vm.have_wx_local = true; }
      if (vm.have_wx_online && (uint32_t)(now - vm.wx_online_ms) > 3 * 3600000u) vm.have_wx_online = false; // stale: fall back to the barometer
      what |= R_WX; break;
    case EV_OTA: {
      uint8_t before = vm.ota.state; vm.ota = e.u.ota; what |= R_OTA;
      char b[60];
      if (before != OTA_AVAILABLE && vm.ota.state == OTA_AVAILABLE) { snprintf(b, sizeof b, "SignalOS %.10s: %.38s", vm.ota.version, vm.ota.notes); notify(N_UPDATE, APP_UPDATES, "Update available", b, page_id_ != PG_UPDATES); }
      if (before != OTA_READY && vm.ota.state == OTA_READY) notify(N_UPDATE, APP_UPDATES, "Update ready", "Restart to finish installing", page_id_ != PG_UPDATES);
      break;
    }
    case EV_TILT: vm.pitch = e.u.tilt.pitch; vm.roll = e.u.tilt.roll; what |= R_TILT; break;
  }
}

// ------------------------------------------------------------------ main loop
void Ui::begin() {
  g_ui = this; set_profile(vm.settings.profile);
  lv_obj_t* scr = lv_screen_active(); lv_obj_set_style_bg_color(scr, C(BG), 0); lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0); lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  stage = box(scr, 0, 0, W, H, BG, 0);
  status_build(); shade_build(); lock_build(); shade_y_ = -H;
  go(vm.settings.setup_done ? PG_HOME : PG_SETUP, 0);
  idle_at_ = vm.settings.screen_off_s * 1000u;
  status_update();
}
void Ui::tick(uint32_t now_ms) {
  now = now_ms; uint32_t what = 0; Event e; int budget = 64;
  while (budget-- > 0 && bus.events.pop(e)) on_event(e, what);
  gestures();
  if (held_ && st_ == ST_ACTIVE && !menu_ && !shade_open_ && (int32_t)(now - repeat_at_) >= 0) { repeat_at_ = now + 130; if (page_) page_->key(held_); } // list key repeat
  if (a_down_ && !a_held_ && (keys_ & KEY_A) && now - a_down_ >= 550 && st_ == ST_ACTIVE) { a_held_ = true; if (page_) page_->hold(); }
  if (b_down_ && !b_held_ && (keys_ & KEY_B) && now - b_down_ >= 700 && st_ == ST_ACTIVE && page_ && !page_->home() && !shade_open_) { b_held_ = true; haptic(HAP_DOUBLE); go(PG_HOME, -1); }
  if (toast_until_ && (int32_t)(now - toast_until_) >= 0) { toast_until_ = 0; anim_opa(toast_, 255, 0, 220, true); }
  if (banner_until_ && (int32_t)(now - banner_until_) >= 0) { banner_until_ = 0; if (banner_) { anim_y(banner_, lv_obj_get_y(banner_), -96, 260, lv_anim_path_ease_in, true); banner_ = nullptr; } }
  // idle: instrument pages keep the screen on, everything else drops to the always-on display
  bool keep_on = page_id_ == PG_FIELD || page_id_ == PG_AP || page_id_ == PG_SWEEP || page_id_ == PG_NAV || page_id_ == PG_TORCH || page_id_ == PG_LEVEL || page_id_ == PG_SETUP ||
                 vm.ota.state == OTA_DOWNLOADING || vm.sos;
  if (keep_on || g_mode_) idle_at_ = now + vm.settings.screen_off_s * 1000u;
  if (st_ == ST_ACTIVE && (int32_t)(now - idle_at_) >= 0) set_state(ST_AOD);
  if (what & (R_POWER | R_GNSS | R_LINK | R_SET | R_NET | R_OTA | R_TIME) || (uint32_t)(now - t_status_) >= 1000) { t_status_ = now; status_update(); }
  if (st_ != ST_ACTIVE && ((what & (R_WX | R_GNSS | R_POWER | R_TIME | R_SET)) || (uint32_t)(now - t_lock_) >= 15000)) { t_lock_ = now; lock_update(); }
  if (shade_open_ && (what & (R_SET | R_NET | R_POWER))) shade_draw();
  if (st_ == ST_ACTIVE && page_) { if (what) page_->refresh(what); page_->tick(); }
}
}}
