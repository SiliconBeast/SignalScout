#include "theme.h"
#include "weather.h"
#include <stdio.h>
#include <string.h>
namespace ss { namespace ui {

const lv_font_t *F12 = &ss_sans_12, *F14 = &ss_sans_14, *F16 = &ss_sans_16, *F20 = &ss_sans_20, *F24 = &ss_sans_24, *F28 = &ss_sans_28,
  *F36 = &ss_sans_36, *F48 = &ss_sans_48, *M12 = &ss_mono_12, *M14 = &ss_mono_14, *M20 = &ss_mono_20, *M28 = &ss_mono_28,
  *CLK = &ss_clock_112, *CLK64 = &ss_clock_64;

static const Theme HACKER = {0x39FF88, 0x00E0FF, 0x001A0C, 0x0A0D0B, 0x132018, 0x1D3A28, 14, 1, true, false, "Hackerspace"};
static const Theme ADVENTURE = {0xFF9F1C, 0xFFD166, 0x1A0E00, 0x131210, 0x24211C, 0x2A2620, 24, 0, false, true, "Adventure"};
static uint8_t g_profile = 0;
const Theme& theme() { return g_profile ? ADVENTURE : HACKER; }
void set_profile(uint8_t p) { g_profile = p ? 1 : 0; }
uint8_t profile() { return g_profile; }

lv_obj_t* box(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color, int radius) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, C(color), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}
lv_obj_t* card(lv_obj_t* parent, int x, int y, int w, int h) {
  const Theme& t = theme(); lv_obj_t* c = box(parent, x, y, w, h, t.card, t.radius);
  if (t.border_w) { lv_obj_set_style_border_width(c, t.border_w, 0); lv_obj_set_style_border_color(c, C(t.border), 0); }
  return c;
}
lv_obj_t* text(lv_obj_t* parent, int x, int y, const char* s, const lv_font_t* f, uint32_t color) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, f, 0); lv_obj_set_style_text_color(l, C(color), 0);
  lv_label_set_text(l, s); lv_obj_set_pos(l, x, y);
  return l;
}
lv_obj_t* text_w(lv_obj_t* parent, int x, int y, int w, const char* s, const lv_font_t* f, uint32_t color, lv_text_align_t al) {
  lv_obj_t* l = text(parent, x, y, s, f, color);
  lv_obj_set_width(l, w); lv_label_set_long_mode(l, LV_LABEL_LONG_DOT); lv_obj_set_style_text_align(l, al, 0);
  lv_obj_set_height(l, lv_font_get_line_height(f));
  return l;
}
lv_obj_t* pill(lv_obj_t* parent, int x, int y, const char* s, uint32_t bg, uint32_t fg, const lv_font_t* f) {
  lv_obj_t* p = box(parent, x, y, LV_SIZE_CONTENT, LV_SIZE_CONTENT, bg, 99);
  lv_obj_set_style_pad_hor(p, 12, 0); lv_obj_set_style_pad_ver(p, 5, 0);
  lv_obj_t* l = lv_label_create(p); lv_obj_set_style_text_font(l, f ? f : (theme().mono_data ? M12 : F14), 0); lv_obj_set_style_text_color(l, C(fg), 0); lv_label_set_text(l, s);
  return p;
}
void set_pill(lv_obj_t* p, const char* s, uint32_t bg, uint32_t fg) {
  lv_obj_set_style_bg_color(p, C(bg), 0); lv_obj_t* l = lv_obj_get_child(p, 0);
  lv_label_set_text(l, s); lv_obj_set_style_text_color(l, C(fg), 0);
}
void tap_id(lv_obj_t* o, int id) {
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_flag(o, LV_OBJ_FLAG_EVENT_BUBBLE); lv_obj_set_user_data(o, (void*)(intptr_t)(id + 1));
}
lv_obj_t* row(lv_obj_t* parent, int y, int h, bool focus, uint32_t accent, int id) {
  const Theme& t = theme(); int r = t.radius < 18 ? t.radius : 18;
  lv_obj_t* o = box(parent, M, y, W - 2 * M, h, focus ? t.card_hi : t.card, r);
  if (t.border_w) { lv_obj_set_style_border_width(o, 1, 0); lv_obj_set_style_border_color(o, C(focus ? accent : t.border), 0); }
  if (focus) {
    if (!t.border_w) { lv_obj_set_style_outline_width(o, 2, 0); lv_obj_set_style_outline_color(o, C(accent), 0); lv_obj_set_style_outline_pad(o, 1, 0); }
    else box(o, 0, 12, 3, h - 26, accent, 2); // Hackerspace: cursor bar
  }
  if (id >= 0) tap_id(o, id);
  return o;
}
lv_obj_t* glyph(lv_obj_t* parent, const lv_image_dsc_t* img, int x, int y, uint32_t color) {
  lv_obj_t* i = lv_image_create(parent); lv_image_set_src(i, img); lv_obj_set_pos(i, x, y);
  lv_obj_set_style_image_recolor(i, C(color), 0); lv_obj_set_style_image_recolor_opa(i, LV_OPA_COVER, 0);
  lv_obj_remove_flag(i, LV_OBJ_FLAG_CLICKABLE);
  return i;
}
lv_obj_t* arc(lv_obj_t* parent, int cx, int cy, int r, int a0, int a1, int w, uint32_t color) {
  lv_obj_t* a = lv_arc_create(parent); lv_obj_remove_style_all(a);
  lv_obj_set_size(a, 2 * r, 2 * r); lv_obj_set_pos(a, cx - r, cy - r);
  lv_arc_set_bg_angles(a, (lv_value_precise_t)a0, (lv_value_precise_t)a1);
  lv_obj_set_style_arc_width(a, w, 0); lv_obj_set_style_arc_color(a, C(color), 0); lv_obj_set_style_arc_rounded(a, true, 0); lv_obj_set_style_arc_opa(a, LV_OPA_COVER, 0);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
  return a;
}
lv_obj_t* section(lv_obj_t* parent, int y, const char* label) {
  char b[48];
  if (theme().mono_data) { snprintf(b, sizeof b, "// %s", label); for (char* p = b; *p; p++) if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32); return text(parent, M + 4, y, b, M12, theme().acc); }
  return text(parent, M + 4, y, label, F14, DIM);
}
static const AppInfo APPS[APP_COUNT] = {
  {"Wi-Fi", &ic52_wifi, 0x2F7BFF, 0x00C2FF, 0xFFFFFF}, {"Bluetooth", &ic52_devices, 0x5B4BFF, 0x9A7BFF, 0xFFFFFF},
  {"Channels", &ic52_channels, 0x0077E6, 0x2FE0C8, 0xFFFFFF}, {"Sweep", &ic52_sweep, 0x7A2CFF, 0xD45CFF, 0xFFFFFF},
  {"Position", &ic52_position, 0x0F9F5A, 0x5CE08A, 0xFFFFFF}, {"Navigate", &ic52_navigate, 0x00927C, 0x3DE0B5, 0xFFFFFF},
  {"Mesh", &ic52_mesh, 0x4C9A12, 0xA3E635, 0xFFFFFF}, {"Weather", &ic52_weather, 0xFF8A1C, 0xFFD166, 0xFFFFFF},
  {"Console", &ic52_console, 0x1C1F26, 0x3A3F4B, 0x39FF88}, {"Torch", &ic52_torch, 0xFFB020, 0xFFE59A, 0x2A1A00},
  {"Level", &ic52_level, 0xFF4A2E, 0xFF9A3C, 0xFFFFFF}, {"Updates", &ic52_updates, 0xFF3D7F, 0xFF8A5C, 0xFFFFFF},
  {"Settings", &ic52_settings, 0x4A505B, 0x8C939E, 0xFFFFFF},
};
const AppInfo& app_info(int a) { return APPS[a < 0 || a >= APP_COUNT ? APP_SETTINGS : a]; }
const int* launcher_order(uint8_t p, int& n) {
  static const int hack[] = {APP_WIFI, APP_BLE, APP_CHANNELS, APP_SWEEP, APP_CONSOLE, APP_MESH, APP_LEVEL, APP_UPDATES, APP_SETTINGS, APP_POSITION, APP_NAVIGATE, APP_WEATHER, APP_TORCH};
  static const int adv[] = {APP_POSITION, APP_NAVIGATE, APP_MESH, APP_WEATHER, APP_TORCH, APP_LEVEL, APP_SWEEP, APP_UPDATES, APP_SETTINGS, APP_WIFI, APP_BLE, APP_CHANNELS, APP_CONSOLE};
  n = APP_COUNT; return p ? adv : hack;
}
lv_obj_t* app_tile(lv_obj_t* parent, int x, int y, int s, int app) {
  const AppInfo& a = app_info(app); const Theme& t = theme();
  lv_obj_t* tile = box(parent, x, y, s, s, t.colour_tiles ? a.top : 0x0A0F0C, s * 30 / 100);
  if (t.colour_tiles) { lv_obj_set_style_bg_grad_color(tile, C(a.bottom), 0); lv_obj_set_style_bg_grad_dir(tile, LV_GRAD_DIR_VER, 0); }
  else { lv_obj_set_style_border_width(tile, 2, 0); lv_obj_set_style_border_color(tile, C(t.border), 0); }
  lv_obj_t* g = glyph(tile, a.icon, (s - 52) / 2, (s - 52) / 2, t.colour_tiles ? a.ink : t.acc);
  (void)g; return tile;
}
void header(lv_obj_t* scr, const char* title, uint32_t accent, const char* const* tabs, int n_tabs, int sel) {
  text(scr, M + 2, BAR_H + (n_tabs ? 13 : 10), title, n_tabs ? F24 : F28, TXT);
  if (!n_tabs) return;
  const Theme& t = theme(); int x = W - M;
  for (int i = n_tabs - 1; i >= 0; i--) {
    lv_obj_t* p = pill(scr, 0, BAR_H + 13, tabs[i], i == sel ? accent : t.card, i == sel ? 0x000000 : DIM, t.mono_data ? M12 : F12);
    lv_obj_set_style_pad_hor(p, 9, 0); tap_id(p, 1000 + i);
    if (t.border_w && i != sel) { lv_obj_set_style_border_width(p, 1, 0); lv_obj_set_style_border_color(p, C(t.border), 0); }
    lv_obj_update_layout(p); x -= lv_obj_get_width(p); lv_obj_set_x(p, x); x -= 5;
  }
}
void hint(lv_obj_t* scr, const char* s) { text_w(scr, 0, H - 26, W, s, theme().mono_data ? M12 : F12, MUTE, LV_TEXT_ALIGN_CENTER); }
uint32_t rssi_color(int rssi) { return rssi >= -60 ? GREEN : (rssi >= -75 ? AMBER : RED); }
void bars(lv_obj_t* parent, int x, int y, int rssi) {
  int lvl = rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : rssi >= -85 ? 1 : 0;
  for (int i = 0; i < 4; i++) { int h = 6 + i * 5; box(parent, x + i * 7, y + 21 - h, 5, h, i < lvl ? rssi_color(rssi) : FAINT, 2); }
}
const char* auth_name(uint8_t a) { static const char* n[] = {"Open", "WEP", "WPA", "WPA2", "WPA3", "WPA2-Ent"}; return n[a < 6 ? a : 0]; }
const lv_image_dsc_t* wx_icon(uint8_t c, bool big) {
  static const lv_image_dsc_t* s[] = {&wx40_clear, &wx40_partly, &wx40_cloud, &wx40_fog, &wx40_rain, &wx40_snow, &wx40_storm, &wx40_cloud};
  static const lv_image_dsc_t* b[] = {&wx96_clear, &wx96_partly, &wx96_cloud, &wx96_fog, &wx96_rain, &wx96_snow, &wx96_storm, &wx96_cloud};
  return (big ? b : s)[c > 7 ? 7 : c];
}
void fmt_dist(char* b, size_t n, double m, bool metric) {
  if (metric) { if (m < 1000) snprintf(b, n, "%.0f m", m); else snprintf(b, n, "%.2f km", m / 1000); }
  else { double ft = m * 3.28084; if (ft < 1000) snprintf(b, n, "%.0f ft", ft); else snprintf(b, n, "%.2f mi", m / 1609.344); }
}
void fmt_temp(char* b, size_t n, float c, bool metric) { snprintf(b, n, "%.0f\xC2\xB0", metric ? c : c * 1.8f + 32); }

// ------------------------------------------------------------------ animation
static void ex_x(void* o, int32_t v) { lv_obj_set_x((lv_obj_t*)o, v); }
static void ex_y(void* o, int32_t v) { lv_obj_set_y((lv_obj_t*)o, v); }
static void ex_w(void* o, int32_t v) { lv_obj_set_width((lv_obj_t*)o, v); }
static void ex_opa(void* o, int32_t v) { lv_obj_set_style_opa((lv_obj_t*)o, (lv_opa_t)v, 0); }
static void ex_scale(void* o, int32_t v) { lv_obj_set_style_transform_scale((lv_obj_t*)o, v, 0); }
static void hide_done(lv_anim_t* a) { lv_obj_add_flag((lv_obj_t*)a->var, LV_OBJ_FLAG_HIDDEN); lv_obj_set_style_opa((lv_obj_t*)a->var, LV_OPA_COVER, 0); }
static void del_done(lv_anim_t* a) { lv_obj_delete((lv_obj_t*)a->var); }
static void base(lv_anim_t& a, lv_obj_t* o, lv_anim_exec_xcb_t cb, int from, int to, uint32_t ms) {
  lv_anim_init(&a); lv_anim_set_var(&a, o); lv_anim_set_exec_cb(&a, cb); lv_anim_set_values(&a, from, to); lv_anim_set_duration(&a, ms);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
}
void anim_x(lv_obj_t* o, int from, int to, uint32_t ms, uint32_t delay, bool del) {
  lv_anim_t a; base(a, o, ex_x, from, to, ms); lv_anim_set_delay(&a, delay); if (del) lv_anim_set_completed_cb(&a, del_done); lv_obj_set_x(o, from); lv_anim_start(&a);
}
void anim_y(lv_obj_t* o, int from, int to, uint32_t ms, lv_anim_path_cb_t path, bool del) {
  lv_anim_t a; base(a, o, ex_y, from, to, ms); lv_anim_set_path_cb(&a, path); if (del) lv_anim_set_completed_cb(&a, del_done); lv_obj_set_y(o, from); lv_anim_start(&a);
}
void anim_w(lv_obj_t* o, int from, int to, uint32_t ms) { lv_anim_t a; base(a, o, ex_w, from, to, ms); lv_anim_start(&a); }
void anim_opa(lv_obj_t* o, int from, int to, uint32_t ms, bool hide) {
  lv_anim_t a; base(a, o, ex_opa, from, to, ms); if (hide) lv_anim_set_completed_cb(&a, hide_done); lv_obj_set_style_opa(o, (lv_opa_t)from, 0); lv_anim_start(&a);
}
void anim_scale(lv_obj_t* o, int from, int to, uint32_t ms) {
  lv_obj_set_style_transform_pivot_x(o, lv_pct(50), 0); lv_obj_set_style_transform_pivot_y(o, lv_pct(50), 0);
  lv_anim_t a; base(a, o, ex_scale, from, to, ms); lv_anim_set_path_cb(&a, lv_anim_path_overshoot); lv_anim_start(&a);
}
struct ColorAnim { uint32_t from, to; };
static void ex_bg(lv_anim_t* a, int32_t v) {
  ColorAnim* c = (ColorAnim*)lv_anim_get_user_data(a);
  lv_obj_set_style_bg_color((lv_obj_t*)a->var, lv_color_mix(C(c->to), C(c->from), (uint8_t)v), 0);
}
void anim_bg(lv_obj_t* o, uint32_t from, uint32_t to, uint32_t ms) {
  static ColorAnim pool[16]; static int k = 0; ColorAnim* c = &pool[k++ & 15]; c->from = from; c->to = to;
  lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a, o); lv_anim_set_custom_exec_cb(&a, ex_bg); lv_anim_set_user_data(&a, c);
  lv_anim_set_values(&a, 0, 255); lv_anim_set_duration(&a, ms); lv_anim_set_path_cb(&a, lv_anim_path_ease_out); lv_anim_start(&a);
}
}}
