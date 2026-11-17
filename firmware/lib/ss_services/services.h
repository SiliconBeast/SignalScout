// Services: all device behaviour that is not drawing. Each service has a
// tick(now_ms) that is cheap and non-blocking (except LoRa send). On the
// device every service runs in its own FreeRTOS task; in the simulator and
// the tests they are ticked from one loop. They talk to the UI via Bus only.
#pragma once
#include "bus.h"
#include "hal.h"
#include "proto.h"
#include "mesh.h"
#include "ubx.h"
#include "correlate.h"
#include "weather.h"
#include "ota.h"
namespace ss {

// Small cross-service blackboard. Single-word fields only, so plain reads and
// writes are safe across tasks on the ESP32.
struct Shared {
  volatile uint8_t mode = MODE_HOME;
  volatile int32_t lat_e7 = 0, lon_e7 = 0; volatile uint8_t fix = 0;
  volatile uint8_t batt_pct = 0;
  volatile int32_t last_lat_e7 = 0, last_lon_e7 = 0; volatile bool have_pos = false; // last good fix (weather)
  volatile float alt_m = 0; volatile int16_t brightness_override = -1;
  volatile float known_sum_dbm = -200, strongest_wifi = -200, strongest_ble = -200;
  volatile uint8_t n_wifi = 0, n_ble = 0;
  volatile bool sos = false; volatile uint8_t rf_state = 0;
  Settings settings; // written only by StoreSvc
  volatile uint32_t settings_rev = 0;
};

struct LogLine { char path[20]; char line[140]; };

class InputSvc {
 public:
  InputSvc(Bus& b, Hal& h, Shared& s) : bus_(b), hal_(h), sh_(s) {}
  void tick(uint32_t now);
  static const uint32_t SOS_HOLD_MS = 2000;
 private:
  Bus& bus_; Hal& hal_; Shared& sh_;
  uint16_t stable_ = 0, last_raw_ = 0; uint32_t sos_since_ = 0; bool sos_fired_ = false, first_ = true;
};

class HouseSvc { // power, environment, status LED, backlight
 public:
  HouseSvc(Bus& b, Hal& h, Shared& s) : bus_(b), hal_(h), sh_(s) {}
  void tick(uint32_t now);
 private:
  Bus& bus_; Hal& hal_; Shared& sh_;
  uint32_t t_power_ = 0, t_env_ = 0, t_led_ = 0, t_wx_ = 0, t_tilt_ = 0, rev_ = 0xFFFFFFFF; bool started_ = false;
  int16_t bl_ = -2; float pitch_ = 0, roll_ = 0; Env env_{};
 public:
  wx::PressureLog plog; // public so the simulator can preload history
  void publish_local_weather(uint32_t now);
};

class ScanSvc {
 public:
  ScanSvc(Bus& b, Hal& h, Shared& s) : bus_(b), hal_(h), sh_(s) {}
  void tick(uint32_t now);
  static const int MAX_AP = 32, MAX_BLE = 32;
 private:
  Bus& bus_; Hal& hal_; Shared& sh_;
  enum { IDLE, WIFI, BLE } st_ = IDLE; uint32_t next_ = 0;
  WifiAp aps_[MAX_AP]; BleDev bles_[MAX_BLE]; float wifi_sum_mw_ = 0;
};

class SweepSvc {
 public:
  SweepSvc(Bus& b, Hal& h, Shared& s, SsQueue<LogLine>& log) : bus_(b), hal_(h), sh_(s), log_(log) {}
  void tick(uint32_t now);
  bool linked() const { return linked_; }
 private:
  void send(uint8_t type, const uint8_t* p, uint8_t n);
  Bus& bus_; Hal& hal_; Shared& sh_; SsQueue<LogLine>& log_;
  proto::Parser parser_; uint8_t seq_ = 0;
  bool linked_ = false, streaming_ = false, have_verdict_ = false;
  uint32_t last_rx_ = 0, last_verdict_ = 0, t_ping_ = 0, t_summary_ = 0, t_log_ = 0; uint16_t sled_fw_ = 0;
  proto::Verdict verdict_{};
};

class GnssSvc {
 public:
  GnssSvc(Bus& b, Hal& h, Shared& s, SsQueue<LogLine>& log) : bus_(b), hal_(h), sh_(s), log_(log) {}
  void tick(uint32_t now);
 private:
  void configure();
  Bus& bus_; Hal& hal_; Shared& sh_; SsQueue<LogLine>& log_;
  ubx::Parser parser_; uint32_t t_cfg_ = 0, last_pvt_ = 0, t_log_ = 0; bool cfg_sent_ = false, psm_ = false, got_pvt_ = false;
};

class MeshSvc {
 public:
  MeshSvc(Bus& b, Hal& h, Shared& s, SsQueue<LogLine>& log);
  void tick(uint32_t now);
  SsQueue<Cmd> inbox{8};
  static const uint32_t SOS_PERIOD_MS = 20000;
 private:
  void start_radio(); void status(uint32_t now); void deliver(const mesh::RxResult& r, float rssi, float snr, uint32_t now);
  void sos_packet(uint32_t now, bool clear);
  Bus& bus_; Hal& hal_; Shared& sh_; SsQueue<LogLine>& log_;
  mesh::Node node_; mesh::DutyLimiter duty_;
  bool radio_ok_ = false, started_ = false; uint32_t rev_ = 0, t_status_ = 0, t_sos_ = 0, t_siren_ = 0;
  uint16_t tx_ = 0, rx_ = 0; bool siren_on_ = false;
};

class StoreSvc { // settings, waypoints, CSV logs on microSD
 public:
  StoreSvc(Bus& b, Hal& h, Shared& s, SsQueue<LogLine>& log) : bus_(b), hal_(h), sh_(s), log_(log) {}
  void tick(uint32_t now);
  SsQueue<Cmd> inbox{8};
  static const int MAX_WP = 16;
 private:
  void load(); void save_settings(); void save_waypoints(); void publish_waypoints(uint32_t now); void publish_settings(uint32_t now);
  Bus& bus_; Hal& hal_; Shared& sh_; SsQueue<LogLine>& log_;
  bool loaded_ = false, mounted_ = false; uint32_t t_mount_ = 0;
  Waypoint wps_[MAX_WP]; int n_wp_ = 0;
};

class NetSvc { // Wi-Fi join, clock sync, forecast, updates, web console hotspot
 public:
  NetSvc(Bus& b, Hal& h, Shared& s) : bus_(b), hal_(h), sh_(s) {}
  void tick(uint32_t now);
  SsQueue<Cmd> inbox{8};
  static const uint32_t JOIN_TIMEOUT_MS = 12000, RETRY_MS = 120000, WX_PERIOD_MS = 30 * 60000, OTA_PERIOD_MS = 24 * 3600000u;
  static int load_networks(const char* text, char ssid[][33], char pass[][64], int max);
 private:
  void publish(uint32_t now); void ota_status(uint32_t now, uint8_t st, uint8_t prog = 0);
  void fetch_weather(uint32_t now); void check_update(uint32_t now);
  Bus& bus_; Hal& hal_; Shared& sh_;
  bool want_ = false, ap_ = false, synced_ = false; uint8_t state_ = NET_OFF, last_pub_ = 255; int try_ = 0, n_nets_ = 0;
  uint32_t t_join_ = 0, t_retry_ = 0, t_pub_ = 0, t_wx_ = 0, t_ota_ = 0; bool wx_due_ = true, ota_due_ = true;
  char ssid_[4][33], pass_[4][64], ap_ssid_[20] = {0}, ap_pass_[12] = {0};
  ota::Manifest man_{}; OtaStatus ota_{}; bool installing_ = false;
  char buf_[6144];
};

// Owns everything and routes UI commands to the right inbox.
class Services {
 public:
  Services(Bus& bus, Hal& hal);
  void route(uint32_t now);          // drain bus.cmds (control task)
  void tick_all(uint32_t now);       // simulator / tests: tick every service once
  Bus& bus; Hal& hal; Shared shared; SsQueue<LogLine> log{32};
  InputSvc input; HouseSvc house; ScanSvc scan; SweepSvc sweep; GnssSvc gnss; MeshSvc mesh; StoreSvc store; NetSvc net;
};

// Settings <-> "key=value\n" text (used by StoreSvc, unit-tested).
int settings_to_text(const Settings& s, char* out, int cap);
void settings_from_text(const char* text, Settings& s);
}
