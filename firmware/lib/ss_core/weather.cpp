#include "weather.h"
#include "json.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
namespace ss { namespace wx {

Cond from_wmo(int c) { // WMO 4677 codes as used by Open-Meteo
  if (c == 0 || c == 1) return CLEAR;
  if (c == 2) return PARTLY;
  if (c == 3) return CLOUDY;
  if (c == 45 || c == 48) return FOG;
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return RAIN;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return SNOW;
  if (c >= 95 && c <= 99) return STORM;
  return UNKNOWN;
}
const char* cond_name(Cond c) {
  static const char* n[] = {"Clear", "Partly cloudy", "Cloudy", "Fog", "Rain", "Snow", "Storm", "Unknown"};
  return n[c <= UNKNOWN ? c : UNKNOWN];
}
float sea_level_hpa(float p, float alt, float t) {
  return p * powf(1.0f - 0.0065f * alt / (t + 0.0065f * alt + 273.15f), -5.257f);
}

void PressureLog::add(uint32_t t, float hpa) {
  if (n_) { int last = (head_ + N - 1) % N; if (t < t_[last]) { n_ = 0; head_ = 0; } else if (t - t_[last] < PERIOD_S) return; } // clock went back: start over
  t_[head_] = t; p_[head_] = hpa; head_ = (head_ + 1) % N; if (n_ < N) n_++;
}
bool PressureLog::trend(float& dp3h) const {
  if (n_ < 2) return false;
  int first = (head_ + N - n_) % N, last = (head_ + N - 1) % N;
  uint32_t t0 = t_[last] > 3 * 3600 ? t_[last] - 3 * 3600 : 0;
  double sx = 0, sy = 0, sxx = 0, sxy = 0; int k = 0; uint32_t tmin = t_[last];
  for (int i = 0; i < n_; i++) {
    int j = (first + i) % N; if (t_[j] < t0) continue;
    double x = ((double)t_[j] - t_[last]) / 3600.0, y = p_[j];
    sx += x; sy += y; sxx += x * x; sxy += x * y; k++; if (t_[j] < tmin) tmin = t_[j];
  }
  if (k < 2 || t_[last] - tmin < 3600) return false;
  double den = k * sxx - sx * sx; if (fabs(den) < 1e-9) return false;
  dp3h = (float)(3.0 * (k * sxy - sx * sy) / den); return true;
}

Local local_forecast(float p, float dp3h, uint8_t month, bool north) {
  // Zambretti forecaster, condensed to the condition set the UI can draw.
  int trend = dp3h <= -1.6f ? -1 : dp3h >= 1.6f ? 1 : 0;
  bool summer = north ? (month >= 4 && month <= 9) : (month <= 3 || month >= 10);
  // Classic Zambretti: falling Z = 127 - 0.12P (1..9), steady 144 - 0.13P (10..19), rising 185 - 0.16P (20..32).
  float z = trend < 0 ? 127.0f - 0.12f * p : trend > 0 ? 185.0f - 0.16f * p : 144.0f - 0.13f * p;
  if (trend > 0 && !summer) z += 1;
  if (trend < 0 && summer) z -= 1;
  float pos = trend < 0 ? (z - 1) / 8.0f : trend > 0 ? (z - 20) / 12.0f : (z - 10) / 9.0f; // 0 = best, 1 = worst
  static const char* rising[] = {"Settled fine", "Fine weather", "Becoming fine", "Fine, becoming less settled", "Fairly fine, showers later",
                                 "Showery early, improving", "Changeable, mending", "Rather unsettled, clearing", "Unsettled, probably improving", "Unsettled, short fine spells"};
  static const char* steady[] = {"Settled fine", "Fine weather", "Fine, possible showers", "Fairly fine, showers likely", "Showery, bright intervals",
                                 "Changeable, some rain", "Unsettled, rain at times", "Rain at frequent intervals", "Very unsettled, rain", "Stormy, much rain"};
  static const char* falling[] = {"Settled fine", "Fine weather", "Fine, becoming less settled", "Fairly fine, showers later", "Showery, becoming more unsettled",
                                  "Unsettled, rain later", "Rain at times, worse later", "Rain at times, very unsettled", "Very unsettled, rain", "Stormy, much rain"};
  const char* const* tab = trend > 0 ? rising : trend < 0 ? falling : steady;
  int i = (int)lroundf(pos * 9); if (i < 0) i = 0; if (i > 9) i = 9;
  Cond c = i <= 1 ? CLEAR : i <= 3 ? PARTLY : i <= 5 ? CLOUDY : i <= 8 ? RAIN : STORM;
  Local l; l.cond = c; l.trend = (int8_t)trend; l.text = tab[i]; return l;
}

static bool day_hilo(const char* daily, const char* key, double* out) { const char* v = json::find(daily, key); return v && json::nums(v, out, 3) >= 3; }
bool parse_open_meteo(const char* body, Weather& w, int32_t& utc_off) {
  const char* cur = json::find(body, "current"); if (!cur) return false;
  double t, code, rh = NAN, wind = NAN, hpa = NAN, off = 0;
  if (!json::num(json::find(cur, "temperature_2m"), t) || !json::num(json::find(cur, "weather_code"), code)) return false;
  json::num(json::find(cur, "relative_humidity_2m"), rh); json::num(json::find(cur, "wind_speed_10m"), wind); json::num(json::find(cur, "surface_pressure"), hpa);
  json::num(json::find(body, "utc_offset_seconds"), off); utc_off = (int32_t)off;
  w.online = true; w.temp_c = (float)t; w.cond = from_wmo((int)code);
  w.rh = isnan(rh) ? 0 : (uint8_t)rh; w.wind_kmh = isnan(wind) ? 0 : (float)wind; w.hpa = isnan(hpa) ? 0 : (float)hpa;
  snprintf(w.text, sizeof w.text, "%s", cond_name((Cond)w.cond));
  const char* hourly = json::find(body, "hourly");
  if (hourly) {
    double ht[6], hc[6]; int a = json::nums(json::find(hourly, "temperature_2m"), ht, 6), b = json::nums(json::find(hourly, "weather_code"), hc, 6);
    for (int i = 0; i < 6; i++) { w.hourly[i] = (int8_t)(i < a && !isnan(ht[i]) ? lround(ht[i]) : -128); w.hourly_cond[i] = i < b && !isnan(hc[i]) ? from_wmo((int)hc[i]) : UNKNOWN; }
  }
  const char* daily = json::find(body, "daily");
  if (daily) {
    double hi[3], lo[3], dc[3], pop[3] = {0, 0, 0};
    if (day_hilo(daily, "temperature_2m_max", hi) && day_hilo(daily, "temperature_2m_min", lo) && day_hilo(daily, "weather_code", dc)) {
      day_hilo(daily, "precipitation_probability_max", pop);
      for (int i = 0; i < 3; i++) { w.days[i].hi = (int8_t)lround(hi[i]); w.days[i].lo = (int8_t)lround(lo[i]); w.days[i].cond = from_wmo((int)dc[i]); w.days[i].pop = isnan(pop[i]) ? 0 : (uint8_t)pop[i]; }
      w.n_days = 3;
    }
  }
  return true;
}
int open_meteo_url(char* out, int cap, double lat, double lon) {
  return snprintf(out, (size_t)cap,
    "https://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f"
    "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m,surface_pressure"
    "&hourly=temperature_2m,weather_code&forecast_hours=6"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max&forecast_days=3&timezone=auto",
    lat, lon);
}
}}
