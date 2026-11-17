#include "hal_device.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <RadioLib.h>
#include <esp_sleep.h>
#include "pins.h"
#include "board.h"
#include "battery.h"
#include "i2c_dev.h"
#include "display.h"

using namespace ss;
namespace dev {

static Expander expander;
static FuelGauge gauge_ic;
static Haptic haptic_ic;
static Bme280 bme;
static Imu imu_ic;

// ---- buttons: seven keys + lock on the expander, SOS straight on a GPIO --------
struct Buttons : IButtons {
  uint16_t read() override {
    uint16_t in = expander.inputs(), k = 0; // keys pull the line low
    if (!(in & (1u << EXP_KEY_UP))) k |= KEY_UP;
    if (!(in & (1u << EXP_KEY_DOWN))) k |= KEY_DOWN;
    if (!(in & (1u << EXP_KEY_LEFT))) k |= KEY_LEFT;
    if (!(in & (1u << EXP_KEY_RIGHT))) k |= KEY_RIGHT;
    if (!(in & (1u << EXP_KEY_A))) k |= KEY_A;
    if (!(in & (1u << EXP_KEY_B))) k |= KEY_B;
    if (!(in & (1u << EXP_KEY_SIDE))) k |= KEY_SIDE;
    if (!(in & (1u << EXP_KEY_LOCK))) k |= KEY_LOCK;
    if (!expander.ok()) k = 0;
    if (digitalRead(PIN_KEY_SOS) == LOW) k |= KEY_SOS; // SOS never depends on the I2C bus
    return k;
  }
};
struct Led : ILed { void set(uint8_t r, uint8_t g, uint8_t b) override { if (r != r_ || g != g_ || b != b_) { r_ = r; g_ = g; b_ = b; expander.led(r, g, b); } } uint8_t r_ = 1, g_ = 1, b_ = 1; };
struct Backlight : IBacklight { void set(uint8_t pct) override { display_brightness(pct); } };
struct Gauge : IFuelGauge {
  bool read(float& v, float& soc, float& rate) override {
    uint16_t a, b, c; if (!gauge_ic.read(a, b, c)) return false;
    v = battery::vcell_volts(a); soc = battery::soc_percent(b); rate = battery::crate_pct_per_hr(c); return true;
  }
};
// The board has no VBUS-sense line: "USB present" is inferred from the charger status output.
struct Charger : ICharger {
  bool charging() override { return expander.ok() && !(expander.inputs() & (1u << EXP_CHG_N)); }
  bool usb_present() override { return charging(); }
};
struct Haptics : IHaptics { void play(uint8_t e) override { haptic_ic.play(e); } };
struct Siren : ISiren {
  bool on_ = false;
  void set(bool on, uint16_t hz) override {
    if (on) { if (!on_) digitalWrite(PIN_SIREN_EN, HIGH); ledcWriteTone(0, hz); }
    else if (on_) { ledcWriteTone(0, 0); digitalWrite(PIN_SIREN_EN, LOW); }
    on_ = on;
  }
};
struct ImuDev : IImu { bool read(float& x, float& y, float& z) override { return imu_ic.read(x, y, z); } };
struct EnvDev : IEnv { bool read(float& t, float& h, float& p) override { return bme.read(t, h, p); } };

// ---- Wi-Fi / BLE: passive receive only --------------------------------------------
struct Wifi : IWifiScanner {
  void start() override { WiFi.scanNetworks(true /*async*/, true /*hidden*/, true /*passive*/, 110 /*ms per channel*/); }
  int poll(WifiAp* out, int max) override {
    int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return -1;
    if (n < 0) return 0; // failed: report an empty scan, the service retries
    int k = 0;
    for (int i = 0; i < n && k < max; i++) {
      WifiAp& a = out[k++]; memset(&a, 0, sizeof a);
      strlcpy(a.ssid, WiFi.SSID(i).c_str(), sizeof a.ssid);
      const uint8_t* b = WiFi.BSSID(i); if (b) memcpy(a.bssid, b, 6);
      a.channel = (uint8_t)WiFi.channel(i); a.rssi = (int8_t)WiFi.RSSI(i);
      switch (WiFi.encryptionType(i)) {
        case WIFI_AUTH_OPEN: a.auth = AUTH_OPEN; break;
        case WIFI_AUTH_WEP: a.auth = AUTH_WEP; break;
        case WIFI_AUTH_WPA_PSK: a.auth = AUTH_WPA; break;
        case WIFI_AUTH_WPA2_ENTERPRISE: a.auth = AUTH_ENT; break;
        case WIFI_AUTH_WPA3_PSK: case WIFI_AUTH_WPA2_WPA3_PSK: a.auth = AUTH_WPA3; break;
        default: a.auth = AUTH_WPA2; break;
      }
    }
    WiFi.scanDelete();
    return k;
  }
};
struct Ble : IBleScanner {
  uint16_t ms_ = 0; bool pending_ = false;
  void start(uint16_t ms) override { ms_ = ms; pending_ = true; }
  int poll(BleDev* out, int max) override { // runs in the scan task, so blocking for the scan window is fine
    if (!pending_) return 0;
    pending_ = false;
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(false); scan->setInterval(97); scan->setWindow(67);
    NimBLEScanResults res = scan->start((uint32_t)((ms_ + 999) / 1000), false);
    int k = 0;
    for (int i = 0; i < res.getCount() && k < max; i++) {
      NimBLEAdvertisedDevice d = res.getDevice((uint32_t)i);
      BleDev& o = out[k++]; memset(&o, 0, sizeof o);
      strlcpy(o.name, d.getName().c_str(), sizeof o.name);
      const uint8_t* a = d.getAddress().getNative(); for (int j = 0; j < 6; j++) o.addr[j] = a[5 - j];
      o.rssi = (int8_t)d.getRSSI();
    }
    scan->clearResults();
    return k;
  }
};

// ---- GNSS (UART1) and sled link (UART0) --------------------------------------------
struct Gnss : IGnss {
  int read(uint8_t* buf, int max) override { int n = Serial1.available(); if (n > max) n = max; return n > 0 ? (int)Serial1.readBytes(buf, (size_t)n) : 0; }
  void write(const uint8_t* buf, int n) override { Serial1.write(buf, (size_t)n); }
  void reset() override { expander.out(EXP_GNSS_RST, false); delay(20); expander.out(EXP_GNSS_RST, true); }
};
struct SledLink : ISledLink {
  int read(uint8_t* buf, int max) override { int n = Serial0.available(); if (n > max) n = max; return n > 0 ? (int)Serial0.readBytes(buf, (size_t)n) : 0; }
  void write(const uint8_t* buf, int n) override { Serial0.write(buf, (size_t)n); }
  void wake(bool on) override { expander.out(EXP_SLED_WAKE, on); }
};

// ---- LoRa: SX1262 inside the E22-900M22S -------------------------------------------
static SX1262 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_NRST, PIN_LORA_BUSY, SPI);
struct LoRa : ILoRa {
  bool ok_ = false;
  bool begin(const LoraConfig& c) override {
    // E22: 1.8 V TCXO on DIO3, DC-DC regulator, DIO2 drives TXEN (hard-wired), RXEN on a GPIO.
    int st = radio.begin(c.freq_mhz, c.bw_khz, c.sf, c.cr, c.sync, c.power_dbm, c.preamble, 1.8f, false);
    if (st != RADIOLIB_ERR_NONE) { ok_ = false; return false; }
    radio.setDio2AsRfSwitch(true);
    radio.setRfSwitchPins(PIN_LORA_RXEN, RADIOLIB_NC);
    radio.setCRC(2);
    radio.setCurrentLimit(140.0f);
    ok_ = radio.startReceive() == RADIOLIB_ERR_NONE;
    return ok_;
  }
  bool send(const uint8_t* buf, size_t n) override {
    if (!ok_) return false;
    int st = radio.transmit(const_cast<uint8_t*>(buf), n);
    radio.startReceive();
    return st == RADIOLIB_ERR_NONE;
  }
  int receive(uint8_t* buf, size_t max, float& rssi, float& snr) override {
    if (!ok_ || digitalRead(PIN_LORA_DIO1) == LOW) return 0; // DIO1 goes high on RX done
    size_t n = radio.getPacketLength();
    int st = (n > 0 && n <= max) ? radio.readData(buf, n) : RADIOLIB_ERR_PACKET_TOO_LONG;
    rssi = radio.getRSSI(); snr = radio.getSNR();
    radio.startReceive();
    return st == RADIOLIB_ERR_NONE ? (int)n : 0;
  }
};

// ---- microSD on the shared SPI bus ---------------------------------------------------
struct Storage : IStorage {
  bool mounted_ = false; uint32_t t_try_ = 0;
  bool mounted() override {
    if (mounted_) { if (SD.cardType() == CARD_NONE) { SD.end(); mounted_ = false; } return mounted_; }
    if (millis() - t_try_ < 3000 && t_try_) return false;
    t_try_ = millis() | 1; mounted_ = SD.begin(PIN_SD_CS, SPI, 20000000);
    return mounted_;
  }
  bool append(const char* path, const char* line) override {
    if (!mounted_) return false;
    File f = SD.open(path, FILE_APPEND); if (!f) return false;
    f.println(line); f.close(); return true;
  }
  int read(const char* path, char* buf, int max) override {
    if (!mounted_) return -1;
    File f = SD.open(path, FILE_READ); if (!f) return -1;
    int n = (int)f.read((uint8_t*)buf, (size_t)max); f.close(); return n;
  }
  bool write(const char* path, const char* data) override {
    if (!mounted_) return false;
    File f = SD.open(path, FILE_WRITE); if (!f) return false;
    f.print(data); f.close(); return true;
  }
};

struct System : ISystem {
  uint16_t node_id() override { // low 16 bits of the factory MAC (bytes 4 and 5)
    uint64_t mac = ESP.getEfuseMac(); uint16_t id = (uint16_t)((((mac >> 32) & 0xFF) << 8) | ((mac >> 40) & 0xFF));
    return (id == 0 || id == 0xFFFF) ? 0x5353 : id;
  }
  void deep_sleep() override {
    display_sleep();
    expander.out(EXP_DISP_PWR_EN, false); expander.out(EXP_SLED_WAKE, false); expander.led(0, 0, 0);
    radio.sleep();
    digitalWrite(PIN_HAPTIC_EN, LOW); digitalWrite(PIN_SIREN_EN, LOW);
    expander.inputs(); // reading the inputs releases EXP_INT
    // Wake on any key (expander interrupt, active low) or the SOS button.
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_EXP_INT, 0);
    esp_sleep_enable_ext1_wakeup(1ULL << PIN_KEY_SOS, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
  }
};

static Buttons buttons; static Led led; static Backlight backlight; static Gauge gauge; static Charger charger; static Haptics haptics;
static Siren siren; static ImuDev imu; static EnvDev env; static Wifi wifi; static Ble ble; static Gnss gnss; static LoRa lora;
static SledLink sled; static Storage storage; static System sys;

uint32_t hal_begin(Hal& hal) {
  uint32_t faults = 0;
  pinMode(PIN_KEY_SOS, INPUT); pinMode(PIN_EXP_INT, INPUT); pinMode(PIN_DISP_TE, INPUT); pinMode(PIN_TP_INT, INPUT);
  pinMode(PIN_SIREN_EN, OUTPUT); digitalWrite(PIN_SIREN_EN, LOW);
  ledcSetup(0, 2700, 8); ledcAttachPin(PIN_SIREN_PWM, 0); ledcWriteTone(0, 0);
  // Keep every chip-select on the shared SPI bus high before any of them is initialised.
  pinMode(PIN_LORA_NSS, OUTPUT); digitalWrite(PIN_LORA_NSS, HIGH);
  pinMode(PIN_SD_CS, OUTPUT); digitalWrite(PIN_SD_CS, HIGH);
  pinMode(PIN_LORA_DIO1, INPUT);

  i2c_begin();
  if (!expander.begin()) faults |= F_EXPANDER; // also switches the display rail on
  delay(30);
  if (!display_begin()) faults |= F_DISPLAY;
  if (!gauge_ic.begin()) faults |= F_GAUGE;
  if (!haptic_ic.begin()) faults |= F_HAPTIC;
  if (!bme.begin()) faults |= F_BME;
  if (!imu_ic.begin()) faults |= F_IMU;

  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  if (!storage.mounted()) faults |= F_SD;

  Serial1.setRxBufferSize(1024);
  Serial1.begin(SS_GNSS_BAUD, SERIAL_8N1, PIN_GNSS_TXD /* module TXD -> our RX */, PIN_GNSS_RXD /* our TX -> module RXD */);
  Serial0.setRxBufferSize(2048);
  Serial0.begin(SS_SLED_BAUD, SERIAL_8N1, PIN_SLED_RX, PIN_SLED_TX);
  gnss.reset();

  WiFi.mode(WIFI_STA); WiFi.disconnect(); // station mode only so that scans work; never connects or transmits data
  NimBLEDevice::init("");

  hal.buttons = &buttons; hal.led = &led; hal.backlight = &backlight; hal.gauge = &gauge; hal.charger = &charger; hal.haptics = &haptics;
  hal.siren = &siren; hal.imu = &imu; hal.env = &env; hal.wifi = &wifi; hal.ble = &ble; hal.gnss = &gnss; hal.lora = &lora;
  hal.sled = &sled; hal.storage = &storage; hal.sys = &sys;
  return faults;
}
}
