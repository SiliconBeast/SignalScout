// Hardware abstraction: one interface per peripheral. Device implementations
// live in src/device/, mocks in lib/ss_mock/. Services only see these.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "model.h"
namespace ss {

struct IButtons { virtual ~IButtons() {} virtual uint16_t read() = 0; };            // Key bitmask, 1 = pressed / lock on
struct ILed { virtual ~ILed() {} virtual void set(uint8_t r, uint8_t g, uint8_t b) = 0; };
struct IBacklight { virtual ~IBacklight() {} virtual void set(uint8_t pct) = 0; };  // AMOLED brightness, 0 = panel off
struct IFuelGauge { virtual ~IFuelGauge() {} virtual bool read(float& volts, float& soc_pct, float& rate_pct_hr) = 0; };
struct ICharger { virtual ~ICharger() {} virtual bool charging() = 0; virtual bool usb_present() = 0; };
struct IHaptics { virtual ~IHaptics() {} virtual void play(uint8_t effect) = 0; };
struct ISiren { virtual ~ISiren() {} virtual void set(bool on, uint16_t hz) = 0; };
struct IImu { virtual ~IImu() {} virtual bool read(float& ax, float& ay, float& az) = 0; };
struct IEnv { virtual ~IEnv() {} virtual bool read(float& temp_c, float& rh, float& hpa) = 0; };
// Scanners are asynchronous: start(), then poll() returns -1 while running, else the count.
struct IWifiScanner { virtual ~IWifiScanner() {} virtual void start() = 0; virtual int poll(WifiAp* out, int max) = 0; };
struct IBleScanner { virtual ~IBleScanner() {} virtual void start(uint16_t ms) = 0; virtual int poll(BleDev* out, int max) = 0; };
struct IGnss {
  virtual ~IGnss() {}
  virtual int read(uint8_t* buf, int max) = 0;
  virtual void write(const uint8_t* buf, int n) = 0;
  virtual void reset() = 0;
};
struct LoraConfig { float freq_mhz, bw_khz; uint8_t sf, cr, sync; int8_t power_dbm; uint8_t preamble; };
struct ILoRa {
  virtual ~ILoRa() {}
  virtual bool begin(const LoraConfig& c) = 0;
  virtual bool send(const uint8_t* buf, size_t n) = 0;                         // blocking for the airtime
  virtual int receive(uint8_t* buf, size_t max, float& rssi, float& snr) = 0;  // 0 if nothing
};
struct ISledLink {
  virtual ~ISledLink() {}
  virtual int read(uint8_t* buf, int max) = 0;
  virtual void write(const uint8_t* buf, int n) = 0;
  virtual void wake(bool on) = 0; // SLED_WAKE line
};
struct IStorage {
  virtual ~IStorage() {}
  virtual bool mounted() = 0;
  virtual bool append(const char* path, const char* line) = 0;
  virtual int read(const char* path, char* buf, int max) = 0; // bytes read, -1 if missing
  virtual bool write(const char* path, const char* data) = 0;
};
struct ISystem { virtual ~ISystem() {} virtual uint16_t node_id() = 0; virtual void deep_sleep() = 0; };

struct Hal {
  IButtons* buttons; ILed* led; IBacklight* backlight; IFuelGauge* gauge; ICharger* charger; IHaptics* haptics;
  ISiren* siren; IImu* imu; IEnv* env; IWifiScanner* wifi; IBleScanner* ble; IGnss* gnss; ILoRa* lora;
  ISledLink* sled; IStorage* storage; ISystem* sys;
};
}
