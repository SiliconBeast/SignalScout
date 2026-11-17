// Weather: offline forecast from the BME280 barometer (Zambretti-style),
// online forecast from Open-Meteo when Wi-Fi is up, and the shared condition
// set both map to (one icon per condition).
#pragma once
#include <stdint.h>
#include "model.h"
namespace ss { namespace wx {

enum Cond : uint8_t { CLEAR = 0, PARTLY, CLOUDY, FOG, RAIN, SNOW, STORM, UNKNOWN };
Cond from_wmo(int code);
const char* cond_name(Cond c);

// Barometric pressure reduced to sea level (hypsometric, standard lapse rate).
float sea_level_hpa(float station_hpa, float alt_m, float temp_c);

// Pressure history: one sample every 10 min, 3 h trend by least squares.
class PressureLog {
 public:
  static const int N = 24;                         // 4 h
  static const uint32_t PERIOD_S = 600;
  void add(uint32_t t_s, float hpa);               // ignored if < PERIOD_S since the last sample
  bool trend(float& dp3h) const;                   // hPa change per 3 h; needs >= 1 h of data
  int count() const { return n_; }
 private:
  uint32_t t_[N] = {0}; float p_[N] = {0}; int n_ = 0, head_ = 0;
};

// Local forecast: 'what is the next 12 h likely to bring', from pressure and trend.
struct Local { Cond cond; int8_t trend; const char* text; };
Local local_forecast(float sea_hpa, float dp3h, uint8_t month, bool northern);

// Open-Meteo /v1/forecast response -> Weather (current, 6 hourly, 3 daily, utc offset).
bool parse_open_meteo(const char* body, Weather& out, int32_t& utc_offset_s);
// Request URL for a position (needs ~260 bytes).
int open_meteo_url(char* out, int cap, double lat, double lon);
}}
