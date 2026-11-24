// SPDX-License-Identifier: MIT
// Sweep-mode correlation: is the broadband detector reading explained by the
// WiFi/BLE emitters we can see? Mirrors sled/signalscout_sled/python/correlate.py
// (both are checked against tests/vectors/correlate.json).
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace ss { namespace corr {

enum State : uint8_t { QUIET = 0, EXPLAINED = 1, UNEXPLAINED = 2 };

struct Config {
  float quiet_margin_db = 3.0f;    // within this of the noise floor = quiet
  float explain_margin_db = 6.0f;  // excess over expected allowed before flagging
  float activity_factor = 1.0f;    // scales the known-emitter sum (duty cycle guess)
};

struct Result { State state; float excess_db; float expected_dbm; uint8_t confidence; };

double dbm_to_mw(double dbm);
double mw_to_dbm(double mw);
// Power sum of emitter RSSIs (dBm). Returns -200 for an empty list.
float sum_dbm(const float* rssi_dbm, size_t n);

// measured/floor: detector reading and its rolling noise floor (dBm).
// known_sum_dbm: sum_dbm() over all visible WiFi + BLE emitters.
Result classify(float measured_dbm, float floor_dbm, float known_sum_dbm, const Config& c = Config());

// Rolling noise floor: slow-rising minimum tracker.
class FloorTracker {
 public:
  explicit FloorTracker(float rise_db_per_s = 0.05f) : rise_(rise_db_per_s) {}
  float update(float dbm, float dt_s);
  float value() const { return v_; }
 private:
  float rise_, v_ = 0; bool init_ = false;
};
}}
