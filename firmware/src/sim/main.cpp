// Desktop simulator: the real UI and services on mock drivers.
//   sim                    interactive SDL window (build with -DSS_SDL)
//   sim --shots <dir>      headless: walk every screen and write PNGs
// Keys (SDL): arrows = D-pad, Z = A, X = B, H = side (home), S = SOS (hold), L = lock switch.
#include <lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <functional>
#include "board.h"
#include "services.h"
#include "mocks.h"
#include "ui.h"
#ifdef SS_SDL
#include <SDL2/SDL.h>
#endif
using namespace ss;

static uint16_t fb[SS_LCD_W * SS_LCD_H];
static void flush_cb(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

// Minimal PNG writer (stored deflate blocks, no compression library needed).
static uint32_t crc_tab[256];
static uint32_t crc32(uint32_t c, const uint8_t* p, size_t n) {
  if (!crc_tab[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t k = i; for (int j = 0; j < 8; j++) k = (k & 1) ? 0xEDB88320u ^ (k >> 1) : k >> 1; crc_tab[i] = k; }
  c = ~c; for (size_t i = 0; i < n; i++) c = crc_tab[(c ^ p[i]) & 0xFF] ^ (c >> 8); return ~c;
}
static void chunk(FILE* f, const char* type, const std::vector<uint8_t>& d) {
  uint8_t len[4] = {(uint8_t)(d.size() >> 24), (uint8_t)(d.size() >> 16), (uint8_t)(d.size() >> 8), (uint8_t)d.size()};
  fwrite(len, 1, 4, f); fwrite(type, 1, 4, f); if (!d.empty()) fwrite(d.data(), 1, d.size(), f);
  uint32_t c = crc32(0, (const uint8_t*)type, 4); if (!d.empty()) c = crc32(c, d.data(), d.size());
  uint8_t cb[4] = {(uint8_t)(c >> 24), (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c}; fwrite(cb, 1, 4, f);
}
static bool write_png(const std::string& path) {
  const int w = SS_LCD_W, h = SS_LCD_H; std::vector<uint8_t> raw; raw.reserve((size_t)h * (w * 3 + 1));
  for (int y = 0; y < h; y++) {
    raw.push_back(0);
    for (int x = 0; x < w; x++) { uint16_t p = fb[y * w + x]; uint8_t r = (p >> 11) & 31, g = (p >> 5) & 63, b = p & 31;
      raw.push_back((uint8_t)((r << 3) | (r >> 2))); raw.push_back((uint8_t)((g << 2) | (g >> 4))); raw.push_back((uint8_t)((b << 3) | (b >> 2))); }
  }
  std::vector<uint8_t> z = {0x78, 0x01}; uint32_t a = 1, b2 = 0; size_t pos = 0;
  while (pos < raw.size()) {
    size_t n = raw.size() - pos; if (n > 65535) n = 65535; bool last = pos + n == raw.size();
    z.push_back(last ? 1 : 0); z.push_back((uint8_t)n); z.push_back((uint8_t)(n >> 8)); z.push_back((uint8_t)~n); z.push_back((uint8_t)(~n >> 8));
    for (size_t i = 0; i < n; i++) { uint8_t v = raw[pos + i]; z.push_back(v); a = (a + v) % 65521; b2 = (b2 + a) % 65521; }
    pos += n;
  }
  uint32_t ad = (b2 << 16) | a; z.push_back((uint8_t)(ad >> 24)); z.push_back((uint8_t)(ad >> 16)); z.push_back((uint8_t)(ad >> 8)); z.push_back((uint8_t)ad);
  FILE* f = fopen(path.c_str(), "wb"); if (!f) return false;
  fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
  std::vector<uint8_t> ih = {(uint8_t)(w >> 24), (uint8_t)(w >> 16), (uint8_t)(w >> 8), (uint8_t)w, (uint8_t)(h >> 24), (uint8_t)(h >> 16), (uint8_t)(h >> 8), (uint8_t)h, 8, 2, 0, 0, 0};
  chunk(f, "IHDR", ih); chunk(f, "IDAT", z); chunk(f, "IEND", {}); fclose(f); return true;
}

struct Sim {
  mock::Rig rig; Bus bus; Services svc{bus, rig.hal}; ui::Ui ui{bus};
  void step(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 10) { rig.clk.ms += 10; svc.tick_all(rig.clk.ms); ui.tick(rig.clk.ms); lv_tick_inc(10); lv_timer_handler(); } }
  void press(uint16_t k, uint32_t settle = 500) { rig.buttons.mask = (uint16_t)((rig.buttons.mask & KEY_LOCK) | k); step(60); rig.buttons.mask &= KEY_LOCK; step(settle); }
  bool until(const std::function<bool()>& cond, uint32_t max_ms = 40000) { for (uint32_t t = 0; t < max_ms; t += 50) { if (cond()) return true; step(50); } return cond(); }
};

static int shots(const char* dir) {
  Sim s; s.ui.begin(); int fails = 0;
  auto shot = [&](const char* name) { s.step(40); std::string p = std::string(dir) + "/" + name + ".png"; if (!write_png(p)) { fprintf(stderr, "cannot write %s\n", p.c_str()); fails++; } else printf("wrote %s\n", p.c_str()); };
  // Seed a couple of waypoints and a card, as on a unit that has been used.
  s.rig.storage.files["/waypoints.csv"] = "Truck,53.5310000,-113.5210000\nCamp,53.5120000,-113.5610000\nLookout,53.5290000,-113.5330000\n";
  s.step(1500); shot("01_home_searching");
  s.press(KEY_A); s.until([&] { return s.ui.vm.n_ap > 0 && s.ui.vm.n_ble > 0; }); s.step(8000); shot("02_field_networks");
  s.press(KEY_DOWN); s.press(KEY_DOWN); s.press(KEY_A); s.step(90000); shot("03_field_ap_detail");
  s.press(KEY_B); s.press(KEY_RIGHT); shot("04_field_channels");
  s.press(KEY_RIGHT); shot("05_field_bluetooth");
  s.press(KEY_B); shot("06_home");
  s.press(KEY_RIGHT); s.press(KEY_A);
  s.until([&] { return s.ui.vm.have_rf && s.ui.vm.rf.state == corr::EXPLAINED && s.ui.vm.rf_n > 100; }, 60000); shot("07_sweep_explained");
  s.until([&] { return s.ui.vm.rf.state == corr::UNEXPLAINED; }); s.step(2500); shot("08_sweep_unexplained");
  s.rig.sled.attached = false; s.rig.sled.out.clear(); s.step(6000); shot("09_sweep_no_sled"); s.rig.sled.attached = true;
  s.press(KEY_B); s.press(KEY_DOWN); s.press(KEY_LEFT); s.press(KEY_A); s.step(1200); shot("10_beacon_position");
  s.press(KEY_A, 1200); shot("11_beacon_waypoint_saved"); s.step(2500);
  s.press(KEY_RIGHT); shot("12_beacon_waypoints");
  s.press(KEY_DOWN); s.press(KEY_A); s.step(3000); shot("13_beacon_navigate");
  s.press(KEY_B); s.press(KEY_RIGHT); s.press(KEY_A); s.press(KEY_DOWN); shot("14_beacon_quick_reply");
  s.press(KEY_A); s.until([&] { return s.ui.vm.n_msg >= 4; }, 30000); s.step(3000); shot("15_beacon_messages");
  s.press(KEY_LEFT); s.press(KEY_LEFT);
  s.rig.buttons.mask = KEY_SOS; s.step(2300); s.rig.buttons.mask = 0; s.step(3200); shot("16_sos_active");
  s.rig.buttons.mask = KEY_SOS; s.step(2300); s.rig.buttons.mask = 0; s.step(3200);
  s.press(KEY_B); s.press(KEY_RIGHT); s.press(KEY_A); s.press(KEY_DOWN); s.press(KEY_DOWN); s.press(KEY_DOWN); s.press(KEY_DOWN); shot("17_settings");
  s.rig.buttons.mask = KEY_LOCK; s.step(400); shot("18_locked"); s.rig.buttons.mask = 0; s.step(200);
  s.rig.charger.chg = true; s.rig.charger.usb = true; s.rig.gauge.soc = 12; s.press(KEY_B); s.step(2500); shot("19_home_charging");
  return fails;
}

int main(int argc, char** argv) {
  lv_init();
  if (argc >= 3 && !strcmp(argv[1], "--shots")) {
    lv_display_t* d = lv_display_create(SS_LCD_W, SS_LCD_H);
    lv_display_set_buffers(d, fb, nullptr, sizeof fb, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d, flush_cb);
    return shots(argv[2]);
  }
#ifdef SS_SDL
  lv_sdl_window_create(SS_LCD_W, SS_LCD_H);
  Sim s; s.ui.begin(); bool lock = false, l_prev = false; uint32_t last = SDL_GetTicks();
  for (;;) {
    SDL_PumpEvents(); const Uint8* k = SDL_GetKeyboardState(nullptr);
    if (k[SDL_SCANCODE_ESCAPE]) break;
    if (k[SDL_SCANCODE_L] && !l_prev) lock = !lock; l_prev = k[SDL_SCANCODE_L];
    uint16_t m = 0;
    if (k[SDL_SCANCODE_UP]) m |= KEY_UP; if (k[SDL_SCANCODE_DOWN]) m |= KEY_DOWN; if (k[SDL_SCANCODE_LEFT]) m |= KEY_LEFT; if (k[SDL_SCANCODE_RIGHT]) m |= KEY_RIGHT;
    if (k[SDL_SCANCODE_Z]) m |= KEY_A; if (k[SDL_SCANCODE_X]) m |= KEY_B; if (k[SDL_SCANCODE_H]) m |= KEY_SIDE; if (k[SDL_SCANCODE_S]) m |= KEY_SOS;
    if (lock) m |= KEY_LOCK;
    s.rig.buttons.mask = m;
    uint32_t t = SDL_GetTicks(); while ((uint32_t)(t - last) >= 10) { last += 10; s.step(10); }
    SDL_Delay(4);
  }
  return 0;
#else
  fprintf(stderr, "usage: %s --shots <output dir>   (interactive window needs an SDL build)\n", argv[0]);
  return 2;
#endif
}
