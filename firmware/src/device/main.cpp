// Signal Scout base firmware (ESP32-S3). One FreeRTOS task per service; the
// Arduino loop task is the UI task. Tasks only meet at the message bus.
#include <Arduino.h>
#include <lvgl.h>
#include "board.h"
#include "services.h"
#include "ui.h"
#include "hal_device.h"

using namespace ss;

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

static Bus* bus;
static Hal hal;
static Services* svc;
static ui::Ui* gui;

template <class F> static void every(uint32_t period_ms, F f) {
  TickType_t last = xTaskGetTickCount();
  for (;;) { f((uint32_t)millis()); vTaskDelayUntil(&last, pdMS_TO_TICKS(period_ms)); }
}
static void t_control(void*) { every(20, [](uint32_t now) { svc->route(now); svc->store.tick(now); svc->house.tick(now); }); }
static void t_input(void*) { every(10, [](uint32_t now) { svc->input.tick(now); }); }
static void t_scan(void*) { every(50, [](uint32_t now) { svc->scan.tick(now); }); }
static void t_sweep(void*) { every(10, [](uint32_t now) { svc->sweep.tick(now); }); }
static void t_gnss(void*) { every(50, [](uint32_t now) { svc->gnss.tick(now); }); }
static void t_mesh(void*) { every(10, [](uint32_t now) { svc->mesh.tick(now); }); }

static uint32_t tick_cb() { return (uint32_t)millis(); }

void setup() {
  Serial.begin(115200);
  lv_init();
  lv_tick_set_cb(tick_cb);
  uint32_t faults = dev::hal_begin(hal);
  Serial.printf("Signal Scout %s  boot faults: 0x%02lX\n", SS_FW_VERSION, (unsigned long)faults);

  bus = new Bus();
  svc = new Services(*bus, hal);
  gui = new ui::Ui(*bus);
  gui->begin();
  if (faults & ~(uint32_t)dev::F_SD) { // a missing card is normal; anything else is worth a toast
    char b[48]; snprintf(b, sizeof b, "Hardware check failed: 0x%02lX", (unsigned long)faults); gui->toast(b);
  }

  // Core 0 also runs the Wi-Fi/BLE stacks; the UI stays alone on core 1.
  xTaskCreatePinnedToCore(t_control, "control", 6144, nullptr, 3, nullptr, 0);
  xTaskCreatePinnedToCore(t_input, "input", 4096, nullptr, 4, nullptr, 0);
  xTaskCreatePinnedToCore(t_scan, "scan", 8192, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(t_sweep, "sweep", 4096, nullptr, 3, nullptr, 0);
  xTaskCreatePinnedToCore(t_gnss, "gnss", 4096, nullptr, 2, nullptr, 0);
  xTaskCreatePinnedToCore(t_mesh, "mesh", 8192, nullptr, 2, nullptr, 0);
}

void loop() {
  gui->tick((uint32_t)millis());
  uint32_t wait = lv_timer_handler();
  delay(wait > 10 ? 10 : (wait ? wait : 1));
}
