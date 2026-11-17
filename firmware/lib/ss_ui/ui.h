// SignalOS (LVGL 9). Talks to the rest of the firmware through Bus only:
// pops Events, pushes Cmds. Same code on the device and in the simulator.
//
// Layers, bottom to top:
//   stage      the current app page (pages slide in/out with a parallax push)
//   status     time, live-activity pill, link glyphs, battery
//   banner     notification banners that drop in from the top
//   shade      pull-down controls + notification list
//   lock       lock screen / always-on display
#pragma once
#include <lvgl.h>
#include "bus.h"
#include "clock.h"
#include "theme.h"
namespace ss { namespace ui {

enum Refresh : uint32_t { R_WIFI = 1, R_BLE = 2, R_RF = 4, R_GNSS = 8, R_MSG = 16, R_WP = 32, R_SET = 64, R_POWER = 128, R_LINK = 256, R_TICK = 512,
  R_NET = 1024, R_WX = 2048, R_OTA = 4096, R_TILT = 8192, R_TIME = 16384, R_NOTIF = 32768 };

struct ApHist { uint8_t bssid[6]; int8_t rssi[40]; uint8_t n; };
enum NotifKind : uint8_t { N_MSG, N_SOS, N_BATTERY, N_SLED, N_RF, N_UPDATE, N_NET, N_ARRIVED, N_INFO };
struct Notif { uint8_t kind, app; char title[32]; char body[60]; uint32_t t_ms; };

struct Vm { // everything the screens draw, rebuilt from events
  Power power; Env env; GnssFix gnss; Settings settings; LoraStatus lora; RfReading rf;
  bool sled_linked = false, sos = false, locked = false, sd = false, have_rf = false;
  WifiAp aps[32]; int n_ap = 0; WifiAp aps_in[32]; int n_ap_in = 0;
  BleDev bles[32]; int n_ble = 0; BleDev bles_in[32]; int n_ble_in = 0;
  ApHist hist[32]; int n_hist = 0;
  float rf_hist[120], rf_exp[120]; int rf_n = 0;
  MeshText msgs[12]; int n_msg = 0;
  Waypoint wps[16]; int n_wp = 0; Waypoint wps_in[16]; int n_wp_in = 0;
  // SignalOS
  NetStatus net{}; OtaStatus ota{}; Weather wx_online{}, wx_local{}; bool have_wx_online = false, have_wx_local = false; uint32_t wx_online_ms = 0;
  float pitch = 0, roll = 0;
  uint32_t utc = 0, utc_ms = 0; uint8_t time_src = 0;   // utc at local ms utc_ms; 0 = unknown
  Notif notifs[12]; int n_notif = 0; uint8_t unread = 0;
  const Weather* weather() const { return have_wx_online ? &wx_online : have_wx_local ? &wx_local : nullptr; }
};

class Ui;
struct Page {
  Ui& ui; lv_obj_t* scr = nullptr;
  explicit Page(Ui& u) : ui(u) {}
  virtual ~Page() {}
  virtual void build() = 0;
  virtual void refresh(uint32_t what) { (void)what; }
  virtual bool key(uint16_t pressed) { (void)pressed; return false; }
  virtual bool tap(int idx) { (void)idx; return false; }          // touch on a row (index) or tab (1000 + tab)
  virtual void hold() {}                                           // A held: quick actions
  virtual bool holdable() const { return false; }                  // A acts on release so it can be held
  virtual void tick() {}                                           // every UI frame (smooth needles etc.)
  virtual Mode mode() const { return MODE_HOME; }
  virtual bool home() const { return false; }
};

enum PageId { PG_HOME, PG_FIELD, PG_AP, PG_SWEEP, PG_BEACON, PG_NAV, PG_REPLY, PG_SETTINGS, PG_WEATHER, PG_CONSOLE, PG_TORCH, PG_LEVEL, PG_UPDATES, PG_SETUP };
enum UiState : uint8_t { ST_ACTIVE, ST_LOCK, ST_AOD };

// Page factories (apps_*.cpp)
Page* new_home(Ui&); Page* new_field(Ui&); Page* new_ap(Ui&); Page* new_sweep(Ui&); Page* new_beacon(Ui&); Page* new_nav(Ui&); Page* new_reply(Ui&);
Page* new_settings(Ui&); Page* new_weather(Ui&); Page* new_console(Ui&); Page* new_torch(Ui&); Page* new_level(Ui&); Page* new_updates(Ui&); Page* new_setup(Ui&);
lv_obj_t* page_root(Ui& ui);   // a fresh full-screen container on the stage

class Ui {
 public:
  explicit Ui(Bus& bus) : bus(bus) {}
  void begin();                 // call once after lv_init() and display creation
  void tick(uint32_t now_ms);   // drain events, update widgets, run gestures
  void go(PageId id, int dir);  // dir: +1 push, -1 pop, 0 replace
  void open_app(int app);
  void cmd(const Cmd& c) { bus.cmds.push(c); }
  void haptic(uint8_t e = HAP_CLICK) { bus.cmds.push(Cmd(CMD_HAPTIC, e)); }
  void toast(const char* text);
  void notify(uint8_t kind, uint8_t app, const char* title, const char* body, bool banner = true);
  void apply(const Settings& s);                 // change settings (UI + store)
  bool local_time(clk::Civil& c) const;          // false until GNSS or network time
  void quick_actions(int app);                   // popover for an app (long-press / hold A)
  void shade(bool open);
  bool shade_open() const { return shade_open_; }
  UiState state() const { return st_; }
  void wake();                                   // any input: AOD -> lock, reset idle timer
  Page* page() { return page_; }
  PageId page_id() const { return page_id_; }

  Bus& bus; Vm vm; uint32_t now = 0; lv_obj_t* stage = nullptr;
  int field_tab = 0, beacon_tab = 0, home_page = 1, home_sel = 0, nav_target = 0, settings_sel = 0; uint8_t ap_bssid[6] = {0};
  bool nav_active = false, torch_red = false;

 private:
  void on_event(const Event& e, uint32_t& what);
  void status_build(); void status_update(); void live_update();
  void shade_build(); void shade_draw(); void shade_key(uint16_t k); void shade_set_y(int y);
  void banner_show(const Notif& n); void lock_build(); void lock_update(); void set_state(UiState s);
  void menu_key(uint16_t k); void menu_close();
  void gestures();
  void key(uint16_t pressed, uint16_t keys);
  Page* page_ = nullptr; PageId page_id_ = PG_HOME; UiState st_ = ST_ACTIVE;
  lv_obj_t *bar_ = nullptr, *l_time_ = nullptr, *live_ = nullptr, *l_live_ = nullptr, *d_live_ = nullptr, *ic_[5] = {nullptr}, *batt_ = nullptr, *batt_fill_ = nullptr, *l_batt_ = nullptr;
  lv_obj_t *toast_ = nullptr, *l_toast_ = nullptr, *banner_ = nullptr, *home_bar_ = nullptr;
  lv_obj_t *shade_ = nullptr, *shade_body_ = nullptr, *lock_ = nullptr, *lk_time_ = nullptr, *lk_date_ = nullptr, *lk_cx_ = nullptr, *lk_list_ = nullptr, *lk_hint_ = nullptr, *lk_clock_box_ = nullptr;
  lv_obj_t *menu_ = nullptr; int menu_app_ = -1, menu_sel_ = 0, menu_n_ = 0;
  bool shade_open_ = false; int shade_sel_ = 0, shade_y_ = 0;
  uint32_t toast_until_ = 0, banner_until_ = 0, t_tick_ = 0, repeat_at_ = 0, idle_at_ = 0, t_status_ = 0, t_lock_ = 0, a_down_ = 0, b_down_ = 0;
  uint16_t held_ = 0, keys_ = 0; bool a_held_ = false, b_held_ = false; char live_txt_[40] = {0};
  // touch gesture tracking
  int g_mode_ = 0; lv_point_t g_start_{}, g_last_{}; uint32_t g_t0_ = 0;
};
}}
