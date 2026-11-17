// Message bus between the UI and the services. The UI never touches a driver
// or a service object: it pops Events and pushes Cmds, nothing else.
#pragma once
#include <string.h>
#include "model.h"
#ifdef ARDUINO
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#else
#include <deque>
#include <mutex>
#endif
namespace ss {

enum EvType : uint8_t {
  EV_KEY, EV_POWER, EV_ENV, EV_GNSS, EV_RF, EV_SLED, EV_LORA, EV_SOS, EV_MSG, EV_SETTINGS, EV_STORAGE,
  EV_WIFI_BEGIN, EV_WIFI_AP, EV_WIFI_END, EV_BLE_BEGIN, EV_BLE_DEV, EV_BLE_END,
  EV_WP_BEGIN, EV_WP_ITEM, EV_WP_END, EV_TOAST,
};
struct LoraStatus { bool ok; uint8_t channel; uint16_t node_id; uint16_t tx_count, rx_count; uint8_t budget_pct; };
struct Event {
  EvType type; uint32_t t_ms;
  union U {
    U() {}
    struct { uint16_t keys, pressed, released; } key;
    Power power; Env env; GnssFix gnss; RfReading rf; LoraStatus lora; MeshText msg; Settings settings;
    WifiAp ap; BleDev ble; Waypoint wp;
    struct { bool linked; uint16_t fw; } sled;
    struct { bool active; } sos;
    struct { bool mounted; } storage;
    char toast[48];
  } u;
  Event() { memset((void*)this, 0, sizeof *this); }
  explicit Event(EvType t, uint32_t now = 0) { memset((void*)this, 0, sizeof *this); type = t; t_ms = now; }
};

enum CmdType : uint8_t {
  CMD_SET_MODE,      // a = Mode
  CMD_SEND_TEXT,     // text
  CMD_SOS,           // a = 1 on, 0 off, 2 toggle
  CMD_WP_SAVE,       // text = name ("" = auto)
  CMD_WP_DELETE,     // a = index
  CMD_SET_SETTINGS,  // settings
  CMD_HAPTIC,        // a = effect (Haptic)
  CMD_SLEEP,
};
enum Haptic : uint8_t { HAP_CLICK = 1, HAP_DOUBLE = 10, HAP_BUZZ = 14, HAP_ALERT = 16 }; // DRV2605 ROM effects
struct Cmd {
  CmdType type; int32_t a; char text[64]; Settings settings;
  Cmd() { memset((void*)this, 0, sizeof *this); }
  explicit Cmd(CmdType t, int32_t a_ = 0) { memset((void*)this, 0, sizeof *this); type = t; a = a_; }
};

#ifdef ARDUINO
template <class T> class SsQueue {
 public:
  explicit SsQueue(int depth = 16) : q_(xQueueCreate(depth, sizeof(T))) {}
  bool push(const T& v) { return xQueueSend(q_, &v, 0) == pdTRUE; }
  bool pop(T& v, uint32_t wait_ms = 0) { return xQueueReceive(q_, &v, pdMS_TO_TICKS(wait_ms)) == pdTRUE; }
 private:
  QueueHandle_t q_;
};
#else
template <class T> class SsQueue {
 public:
  explicit SsQueue(int depth = 16) : depth_((size_t)depth) {}
  bool push(const T& v) { std::lock_guard<std::mutex> l(m_); if (q_.size() >= depth_) return false; q_.push_back(v); return true; }
  bool pop(T& v, uint32_t = 0) { std::lock_guard<std::mutex> l(m_); if (q_.empty()) return false; v = q_.front(); q_.pop_front(); return true; }
 private:
  size_t depth_; std::deque<T> q_; std::mutex m_;
};
#endif

struct Bus {
  SsQueue<Event> events{64}; // services -> UI
  SsQueue<Cmd> cmds{16};     // UI -> services
};
}
