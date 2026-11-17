// Plain data shared by services, UI and tests. No behaviour here.
#pragma once
#include <stdint.h>
namespace ss {

enum Mode : uint8_t { MODE_HOME = 0, MODE_FIELD, MODE_SWEEP, MODE_BEACON, MODE_SETTINGS, MODE_LEVEL, MODE_AOD };
enum Profile : uint8_t { PROFILE_HACKER = 0, PROFILE_ADVENTURE = 1 };

enum Key : uint16_t {
  KEY_UP = 1 << 0, KEY_DOWN = 1 << 1, KEY_LEFT = 1 << 2, KEY_RIGHT = 1 << 3,
  KEY_A = 1 << 4, KEY_B = 1 << 5, KEY_SIDE = 1 << 6, KEY_SOS = 1 << 7, KEY_LOCK = 1 << 8,
};

enum WifiAuth : uint8_t { AUTH_OPEN = 0, AUTH_WEP, AUTH_WPA, AUTH_WPA2, AUTH_WPA3, AUTH_ENT };

struct WifiAp { char ssid[33]; uint8_t bssid[6]; uint8_t channel; int8_t rssi; uint8_t auth; };
struct BleDev { char name[24]; uint8_t addr[6]; int8_t rssi; };

struct GnssFix {
  bool valid = false, time_valid = false; uint8_t fix_type = 0, sats = 0;
  double lat = 0, lon = 0; float alt_m = 0, hacc_m = 0, speed_mps = 0, heading_deg = 0;
  uint16_t year = 0; uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
};

struct Power { float volts = 0, pct = 0; int minutes = -1; bool charging = false, usb = false, gauge_ok = false; };

struct RfReading {
  float dbm = -80, min_dbm = -80, max_dbm = -80, floor_dbm = -80;
  uint8_t state = 0, confidence = 0; float excess_db = 0, expected_dbm = -80;
  bool from_sled_verdict = false;
};

struct Waypoint { char name[16]; double lat, lon; };

struct MeshText {
  uint16_t src; char text[64]; float rssi, snr; uint8_t hops; bool mine, sos, has_pos;
  double lat, lon; uint8_t batt; uint32_t t_ms;
};

struct Env { float temp_c = 0, rh = 0, hpa = 0; bool ok = false; };

// Always-on display complications (Settings > Always-on).
enum Complication : uint8_t { CX_NONE = 0, CX_WEATHER, CX_WAYPOINT, CX_BATTERY, CX_MESH, CX_SATS, CX_RF, CX_NET, CX_COUNT };

struct Settings {
  uint8_t brightness = 70;  // %
  bool haptics = true, siren = true, metric = true, gnss_psm = false, logging = false;
  uint8_t lora_channel = 0; // index into lora::CHANNELS
  uint8_t lora_power = 20;  // dBm at the SX1262 (E22 max 22)
  // SignalOS
  uint8_t profile = PROFILE_HACKER;
  bool setup_done = false;
  bool aod = true;                 // always-on display when locked / idle
  uint8_t aod_cx[3] = {CX_WEATHER, CX_WAYPOINT, CX_BATTERY};
  uint8_t screen_off_s = 30;       // idle -> AOD (or off)
  bool wifi = false;               // join saved networks (weather, time, updates)
  bool auto_update = true;         // check for updates when online
  int16_t tz_min = -420;           // UTC offset in minutes; replaced by the forecast's offset when online
  uint8_t wp_target = 0;           // waypoint shown on the AOD / Navigate
};

// ---- connectivity / weather / updates
enum NetState : uint8_t { NET_OFF = 0, NET_SCANNING, NET_CONNECTING, NET_ONLINE, NET_NO_NETWORK, NET_FAILED };
struct NetStatus { uint8_t state; bool ap; uint8_t ap_clients; int8_t rssi; char ssid[33]; char ip[16]; char ap_ssid[20]; char ap_pass[12]; };

struct WxDay { int8_t hi, lo; uint8_t cond, pop; };
struct Weather {
  bool online;                 // from the forecast service (else local barometer only)
  uint8_t cond, rh, n_days; int8_t trend;  // trend: -1 falling, 0 steady, +1 rising pressure
  float temp_c, wind_kmh, hpa, dp3h;
  int8_t hourly[6]; uint8_t hourly_cond[6];
  WxDay days[3];
  char text[34];
};

enum OtaState : uint8_t { OTA_IDLE = 0, OTA_CHECKING, OTA_UP_TO_DATE, OTA_AVAILABLE, OTA_DOWNLOADING, OTA_VERIFYING, OTA_READY, OTA_ERROR };
struct OtaStatus { uint8_t state, progress; uint32_t size; char version[16]; char notes[60]; };

namespace lora {
// 915 MHz ISM (Canada RSS-247 / US FCC 15.247). 500 kHz channels qualify as
// digital modulation (no hopping needed). Centre frequencies in MHz.
static const float CHANNELS[] = {903.0f, 906.0f, 909.0f, 912.0f, 915.0f, 918.0f, 921.0f, 924.0f};
static const int N_CHANNELS = 8;
static const float BW_KHZ = 500.0f; static const int SF = 9, CR = 5, PREAMBLE = 8; static const uint8_t SYNC = 0x5C;
}
}
