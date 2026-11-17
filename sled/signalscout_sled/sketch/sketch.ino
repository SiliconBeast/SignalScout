// Signal Scout sled - UNO Q microcontroller (STM32U585) side.
//
//  * Samples the AD8317 log detector on A0 at 1 kHz.
//  * Every 50 ms sends an RF_SAMPLE frame to the base over the pogo UART (D0/D1).
//    This path does not depend on Linux, so the meter works as soon as the MCU runs.
//  * Shares readings with the Linux side through the Bridge and forwards its
//    verdicts to the base as VERDICT frames.
//
// Protocol: proto.h (copy of firmware/lib/ss_core/proto.h, kept identical by tools/sync_shared.py).
#include <Arduino_RouterBridge.h>
#include "proto.h"
#include "sled_pins.h"

using namespace ss;

// ---- calibration -------------------------------------------------------------
// AD8317: Vout falls 22 mV per dB; the line crosses 0 V at about +15 dBm
// (datasheet, 900 MHz to 2.2 GHz). FRONT_END_LOSS_DB covers the input pad and trace.
// Calibrate on the bench with a known source and edit these three numbers.
static const float SLOPE_V_PER_DB = 0.022f;
static const float INTERCEPT_DBM = 15.0f;
static const float FRONT_END_LOSS_DB = 0.0f;
static const float ADC_REF_V = 3.3f;
static const int ADC_BITS = 12;

static const uint32_t SAMPLE_US = 1000, WINDOW_MS = 50, LINUX_MS = 200;
static const uint16_t FW_VERSION = 0x0100;

static proto::Parser parser;
static uint8_t seq = 0;
static bool streaming = false;
static float floor_dbm = 0; static bool floor_init = false;
static uint32_t t_sample = 0, t_window = 0, t_linux = 0, t_base_rx = 0;
static float acc = 0, wmin = 0, wmax = 0; static uint32_t count = 0;
static float last_avg = -80;

static float to_dbm(int raw) {
  float v = raw * ADC_REF_V / (float)((1 << ADC_BITS) - 1);
  return INTERCEPT_DBM - v / SLOPE_V_PER_DB + FRONT_END_LOSS_DB;
}
static void send(uint8_t type, const uint8_t* payload, uint8_t len) {
  uint8_t buf[proto::MAX_PAYLOAD + proto::OVERHEAD];
  size_t n = proto::encode(type, seq++, payload, len, buf, sizeof buf);
  if (n) Serial.write(buf, n);
}
static void send_hello() {
  proto::Hello h; h.role = proto::ROLE_SLED; h.proto_ver = proto::VERSION; h.fw_ver = FW_VERSION;
  uint8_t p[4]; send(proto::T_HELLO, p, (uint8_t)proto::pack(h, p));
}

// Called by the Linux side (python/main.py) about once a second.
static void on_verdict(int state, int confidence, int excess_cdb, int expected_cdbm, int n_wifi, int n_ble) {
  proto::Verdict v; v.state = (uint8_t)state; v.confidence = (uint8_t)confidence; v.excess_cdb = (int16_t)excess_cdb;
  v.expected_cdbm = (int16_t)expected_cdbm; v.n_wifi = (uint8_t)n_wifi; v.n_ble = (uint8_t)n_ble;
  uint8_t p[8]; send(proto::T_VERDICT, p, (uint8_t)proto::pack(v, p));
}

static void handle_frame(const proto::Frame& f) {
  t_base_rx = millis();
  switch (f.type) {
    case proto::T_HELLO: send_hello(); break;
    case proto::T_PING: send(proto::T_PONG, nullptr, 0); break;
    case proto::T_WAKE: streaming = true; break;
    case proto::T_SLEEP: streaming = false; break;
    case proto::T_SCAN_SUMMARY: {
      proto::ScanSummary s;
      if (proto::unpack(f.payload, f.len, s)) Bridge.notify("base_scan", (int)s.sum_cdbm, (int)s.n_wifi, (int)s.n_ble);
      break;
    }
    default: break;
  }
}

void setup() {
  Serial.begin(115200);           // D0/D1: pogo UART to the base (through 1 k series resistors)
  pinMode(PIN_UNOQ_WAKE, INPUT);  // D2: high while the base is in Sweep mode
  analogReadResolution(ADC_BITS);
  Bridge.begin();
  Bridge.provide("verdict", on_verdict);
  send_hello();
}

void loop() {
  while (Serial.available() > 0)
    if (parser.feed((uint8_t)Serial.read())) handle_frame(parser.frame);

  uint32_t us = micros(), ms = millis();
  if ((uint32_t)(us - t_sample) >= SAMPLE_US) {
    t_sample = us;
    float d = to_dbm(analogRead(PIN_AD_VOUT));
    if (!count) { wmin = d; wmax = d; }
    if (d < wmin) wmin = d;
    if (d > wmax) wmax = d;
    acc += d; count++;
  }
  if ((uint32_t)(ms - t_window) >= WINDOW_MS && count) {
    float dt = (ms - t_window) / 1000.0f; t_window = ms;
    float avg = acc / count; acc = 0; count = 0; last_avg = avg;
    // Noise floor: follow drops at once, rise 0.05 dB/s (same rule as corr::FloorTracker).
    if (!floor_init || avg < floor_dbm) { floor_dbm = avg; floor_init = true; }
    else { floor_dbm += 0.05f * dt; if (floor_dbm > avg) floor_dbm = avg; }
    bool want = streaming || digitalRead(PIN_UNOQ_WAKE) == HIGH;
    if (want) {
      proto::RfSample s; s.t_ms = ms; s.avg_cdbm = (int16_t)(avg * 100); s.min_cdbm = (int16_t)(wmin * 100);
      s.max_cdbm = (int16_t)(wmax * 100); s.floor_cdbm = (int16_t)(floor_dbm * 100);
      uint8_t p[12]; send(proto::T_RF_SAMPLE, p, (uint8_t)proto::pack(s, p));
    }
  }
  if ((uint32_t)(ms - t_linux) >= LINUX_MS) {
    t_linux = ms;
    Bridge.notify("rf", (int)(last_avg * 100), (int)(floor_dbm * 100));
  }
}
