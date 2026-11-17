// Browser simulator: the real SignalOS UI and services on mock drivers,
// compiled to WebAssembly (WASI reactor). tools/build_web.sh builds it;
// web/index.html draws the frame buffer and feeds buttons and touch.
#include <lvgl.h>
#include <string.h>
#include "board.h"
#include "services.h"
#include "mocks.h"
#include "ui.h"
using namespace ss;

#define EXPORT(name) extern "C" __attribute__((export_name(name)))

static uint16_t fb[SS_LCD_W * SS_LCD_H];
static uint32_t rgba[SS_LCD_W * SS_LCD_H];
static bool dirty = true;
static void flush_cb(lv_display_t* d, const lv_area_t*, uint8_t*) { dirty = true; lv_display_flush_ready(d); }
static struct { int x = 0, y = 0; bool down = false; } finger;
static void touch_read(lv_indev_t*, lv_indev_data_t* d) { d->point.x = finger.x; d->point.y = finger.y; d->state = finger.down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED; }

struct Sim { mock::Rig rig; Bus bus; Services svc{bus, rig.hal}; ui::Ui ui{bus}; };
static Sim* sim = nullptr;

EXPORT("ss_init") void ss_init() {
  lv_init();
  lv_display_t* d = lv_display_create(SS_LCD_W, SS_LCD_H);
  lv_display_set_buffers(d, fb, nullptr, sizeof fb, LV_DISPLAY_RENDER_MODE_DIRECT);
  lv_display_set_flush_cb(d, flush_cb);
  lv_indev_t* in = lv_indev_create(); lv_indev_set_type(in, LV_INDEV_TYPE_POINTER); lv_indev_set_read_cb(in, touch_read);
  sim = new Sim();
  // a unit that has been used: waypoints, a saved network, three hours of pressure history
  sim->rig.storage.files["/waypoints.csv"] = "Truck,53.5310000,-113.5210000\nCamp,53.5120000,-113.5610000\nLookout,53.5290000,-113.5330000\n";
  sim->rig.storage.files["/wifi.txt"] = "HackerspaceYEG\tbe-excellent\n";
  for (uint32_t t = 0; t <= 3 * 3600; t += 600) sim->svc.house.plog.add(t, 1009.5f - t / 4000.0f);
  sim->rig.clk.ms = 3 * 3600 * 1000 + 5000;
  sim->ui.begin();
}
// Advance simulated time (call with the real elapsed ms, capped by the page).
EXPORT("ss_step") void ss_step(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 10) { sim->rig.clk.ms += 10; sim->svc.tick_all(sim->rig.clk.ms); sim->ui.tick(sim->rig.clk.ms); lv_tick_inc(10); }
  lv_timer_handler();
}
EXPORT("ss_keys") void ss_keys(uint32_t mask) { sim->rig.buttons.mask = (uint16_t)mask; }
EXPORT("ss_touch") void ss_touch(int x, int y, int down) { finger.x = x; finger.y = y; finger.down = down != 0; }
EXPORT("ss_sled") void ss_sled(int attached) { sim->rig.sled.attached = attached != 0; if (!attached) sim->rig.sled.out.clear(); }
EXPORT("ss_charging") void ss_charging(int on) { sim->rig.charger.chg = on != 0; sim->rig.charger.usb = on != 0; }
EXPORT("ss_led") uint32_t ss_led() { return ((uint32_t)sim->rig.led.r << 16) | ((uint32_t)sim->rig.led.g << 8) | sim->rig.led.b; }
EXPORT("ss_haptic_count") int ss_haptic_count() { return sim->rig.haptics.plays; }
EXPORT("ss_siren") int ss_siren() { return sim->rig.siren.on ? sim->rig.siren.hz : 0; }
EXPORT("ss_brightness") int ss_brightness() { return sim->rig.backlight.pct; }
EXPORT("ss_width") int ss_width() { return SS_LCD_W; }
EXPORT("ss_height") int ss_height() { return SS_LCD_H; }
// RGBA8888 copy of the frame, or 0 if nothing changed since the last call.
EXPORT("ss_frame") uint32_t* ss_frame() {
  if (!dirty) return nullptr;
  dirty = false;
  for (int i = 0; i < SS_LCD_W * SS_LCD_H; i++) {
    uint16_t p = fb[i]; uint32_t r = (p >> 11) & 31, g = (p >> 5) & 63, b = p & 31;
    r = (r << 3) | (r >> 2); g = (g << 2) | (g >> 4); b = (b << 3) | (b >> 2);
    rgba[i] = 0xFF000000u | (b << 16) | (g << 8) | r;
  }
  return rgba;
}
