// SignalOS look: two profiles over one layout.
//   Hackerspace  black, phosphor green, hairline borders, monospaced data, dense
//   Adventure    black, sunset amber, soft cards, large type, colour app tiles
// Pages draw with these helpers so a profile switch restyles everything.
#pragma once
#include <lvgl.h>
#include <stdint.h>
#include "board.h"
#include "fonts/fonts.h"
#include "icons_gen.h"
namespace ss { namespace ui {

static const int W = SS_LCD_W, H = SS_LCD_H, BAR_H = 40, TOP = 92, M = 16;
#define C(hex) lv_color_hex(hex)
// neutrals and semantic colours (both profiles)
static const uint32_t BG = 0x000000, CARD = 0x111316, CARD_HI = 0x1D2026, LINE = 0x2A2E36, TXT = 0xF5F7FA, DIM = 0x8A919C, FAINT = 0x3A3F48, MUTE = 0x5C626C,
  BLUE = 0x2F7BFF, VIOLET = 0xB45CFF, GREEN = 0x1FD67A, GREY = 0x9AA0A8, RED = 0xFF453A, AMBER = 0xFFB020, CYAN = 0x00D1FF;

struct Theme {
  uint32_t acc, acc2, acc_ink;     // accent, secondary accent, text on accent
  uint32_t card, card_hi, border;  // surfaces
  int radius, border_w;
  bool mono_data, colour_tiles;
  const char* name;
};
const Theme& theme();
void set_profile(uint8_t profile);
uint8_t profile();

// fonts
extern const lv_font_t *F12, *F14, *F16, *F20, *F24, *F28, *F36, *F48, *M12, *M14, *M20, *M28, *CLK, *CLK64;
inline const lv_font_t* DATA(int px) { // numbers: mono in Hackerspace, sans in Adventure
  bool m = theme().mono_data;
  return px <= 12 ? (m ? M12 : F12) : px <= 14 ? (m ? M14 : F14) : px <= 20 ? (m ? M20 : F20) : px <= 28 ? (m ? M28 : F28) : F48;
}

// primitives
lv_obj_t* box(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color, int radius);
lv_obj_t* card(lv_obj_t* parent, int x, int y, int w, int h);               // themed surface
lv_obj_t* text(lv_obj_t* parent, int x, int y, const char* s, const lv_font_t* f, uint32_t color);
lv_obj_t* text_w(lv_obj_t* parent, int x, int y, int w, const char* s, const lv_font_t* f, uint32_t color, lv_text_align_t al = LV_TEXT_ALIGN_LEFT);
lv_obj_t* pill(lv_obj_t* parent, int x, int y, const char* s, uint32_t bg, uint32_t fg, const lv_font_t* f = nullptr);
void set_pill(lv_obj_t* p, const char* s, uint32_t bg, uint32_t fg);
lv_obj_t* row(lv_obj_t* parent, int y, int h, bool focus, uint32_t accent, int tap_id = -1);
void tap_id(lv_obj_t* o, int id);  // touch target: Page::tap(id) on click
lv_obj_t* glyph(lv_obj_t* parent, const lv_image_dsc_t* img, int x, int y, uint32_t color);
lv_obj_t* arc(lv_obj_t* parent, int cx, int cy, int r, int a0, int a1, int w, uint32_t color);
lv_obj_t* section(lv_obj_t* parent, int y, const char* label);                // "// NETWORKS" or "Networks"
lv_obj_t* app_tile(lv_obj_t* parent, int x, int y, int size, int app);         // themed app icon
void header(lv_obj_t* scr, const char* title, uint32_t accent, const char* const* tabs, int n_tabs, int sel);
void hint(lv_obj_t* scr, const char* s);
void bars(lv_obj_t* parent, int x, int y, int rssi);
uint32_t rssi_color(int rssi);
const char* auth_name(uint8_t a);
const lv_image_dsc_t* wx_icon(uint8_t cond, bool big);
void fmt_dist(char* b, size_t n, double m, bool metric);
void fmt_temp(char* b, size_t n, float c, bool metric);

// animation helpers (all ease-out unless noted; durations in ms)
void anim_x(lv_obj_t* o, int from, int to, uint32_t ms, uint32_t delay = 0, bool del_after = false);
void anim_y(lv_obj_t* o, int from, int to, uint32_t ms, lv_anim_path_cb_t path = lv_anim_path_ease_out, bool del_after = false);
void anim_w(lv_obj_t* o, int from, int to, uint32_t ms);
void anim_opa(lv_obj_t* o, int from, int to, uint32_t ms, bool hide_after = false);
void anim_scale(lv_obj_t* o, int from, int to, uint32_t ms);      // 256 = 1.0, overshoot path
void anim_bg(lv_obj_t* o, uint32_t from, uint32_t to, uint32_t ms);

// apps (launcher order is per profile)
enum AppId { APP_WIFI, APP_BLE, APP_CHANNELS, APP_SWEEP, APP_POSITION, APP_NAVIGATE, APP_MESH, APP_WEATHER, APP_CONSOLE, APP_TORCH, APP_LEVEL, APP_UPDATES, APP_SETTINGS, APP_COUNT };
struct AppInfo { const char* name; const lv_image_dsc_t* icon; uint32_t top, bottom, ink; };
const AppInfo& app_info(int app);
const int* launcher_order(uint8_t profile, int& n);
}}
