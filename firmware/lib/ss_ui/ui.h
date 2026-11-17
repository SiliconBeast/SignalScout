// Signal Scout UI (LVGL 9). Talks to the rest of the firmware through Bus
// only: pops Events, pushes Cmds. Same code on the device and in the simulator.
#pragma once
#include <lvgl.h>
#include "bus.h"
namespace ss { namespace ui {

enum Refresh : uint32_t { R_WIFI = 1, R_BLE = 2, R_RF = 4, R_GNSS = 8, R_MSG = 16, R_WP = 32, R_SET = 64, R_POWER = 128, R_LINK = 256, R_TICK = 512 };

struct ApHist { uint8_t bssid[6]; int8_t rssi[40]; uint8_t n; };

struct Vm { // everything the screens draw, rebuilt from events
  Power power; Env env; GnssFix gnss; Settings settings; LoraStatus lora; RfReading rf;
  bool sled_linked = false, sos = false, locked = false, sd = false, have_rf = false;
  WifiAp aps[32]; int n_ap = 0; WifiAp aps_in[32]; int n_ap_in = 0;
  BleDev bles[32]; int n_ble = 0; BleDev bles_in[32]; int n_ble_in = 0;
  ApHist hist[32]; int n_hist = 0;
  float rf_hist[120], rf_exp[120]; int rf_n = 0;
  MeshText msgs[12]; int n_msg = 0;
  Waypoint wps[16]; int n_wp = 0; Waypoint wps_in[16]; int n_wp_in = 0;
};

class Ui;
struct Page {
  Ui& ui; lv_obj_t* scr = nullptr;
  explicit Page(Ui& u) : ui(u) {}
  virtual ~Page() {}
  virtual void build() = 0;
  virtual void refresh(uint32_t what) { (void)what; }
  virtual bool key(uint16_t pressed) { (void)pressed; return false; }
  virtual Mode mode() const { return MODE_HOME; }
};

enum PageId { PG_HOME, PG_FIELD, PG_AP, PG_SWEEP, PG_BEACON, PG_NAV, PG_REPLY, PG_SETTINGS };

class Ui {
 public:
  explicit Ui(Bus& bus) : bus(bus) {}
  void begin();                 // call once after lv_init() and display creation
  void tick(uint32_t now_ms);   // drain events, update widgets
  void go(PageId id, int dir, int arg = 0); // dir: +1 forward, -1 back, 0 none
  void cmd(const Cmd& c) { bus.cmds.push(c); }
  void haptic(uint8_t e = HAP_CLICK) { bus.cmds.push(Cmd(CMD_HAPTIC, e)); }
  void toast(const char* text);
  Bus& bus; Vm vm; uint32_t now = 0;
  int field_tab = 0, beacon_tab = 0, home_sel = 0, nav_target = 0; uint8_t ap_bssid[6] = {0};
 private:
  void on_event(const Event& e, uint32_t& what);
  void status_build(); void status_update();
  Page* page_ = nullptr; PageId page_id_ = PG_HOME;
  lv_obj_t *bar_ = nullptr, *l_time_ = nullptr, *icons_[4] = {nullptr}, *l_batt_ = nullptr, *toast_ = nullptr, *l_toast_ = nullptr, *lock_ = nullptr, *l_lock_time_ = nullptr;
  uint32_t toast_until_ = 0, t_tick_ = 0, repeat_at_ = 0; uint16_t held_ = 0;
};
}}
