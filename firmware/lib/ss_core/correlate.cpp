// SPDX-License-Identifier: MIT
#include "correlate.h"
#include <math.h>
namespace ss { namespace corr {
double dbm_to_mw(double d) { return pow(10.0, d / 10.0); }
double mw_to_dbm(double m) { return m <= 0 ? -200.0 : 10.0 * log10(m); }
float sum_dbm(const float* r, size_t n) {
  double s = 0;
  for (size_t i = 0; i < n; i++) s += dbm_to_mw(r[i]);
  return (float)mw_to_dbm(s);
}
Result classify(float m, float fl, float known, const Config& c) {
  Result r;
  double exp_mw = dbm_to_mw(fl) + c.activity_factor * (known <= -199.0f ? 0.0 : dbm_to_mw(known));
  r.expected_dbm = (float)mw_to_dbm(exp_mw);
  r.excess_db = m - r.expected_dbm;
  if (m - fl < c.quiet_margin_db) { r.state = QUIET; r.confidence = 0; return r; }
  if (r.excess_db <= c.explain_margin_db) {
    r.state = EXPLAINED;
    float k = (c.explain_margin_db - r.excess_db) / c.explain_margin_db; // 0 at the edge
    r.confidence = (uint8_t)(k >= 1 ? 100 : (k <= 0 ? 0 : lroundf(k * 100)));
    return r;
  }
  r.state = UNEXPLAINED;
  float k = (r.excess_db - c.explain_margin_db) / 14.0f; // 20 dB over expected = 100 %
  r.confidence = (uint8_t)(k >= 1 ? 100 : lroundf(k * 100));
  return r;
}
float FloorTracker::update(float d, float dt) {
  if (!init_) { v_ = d; init_ = true; return v_; }
  if (d < v_) v_ = d; else { v_ += rise_ * dt; if (v_ > d) v_ = d; }
  return v_;
}
}}
