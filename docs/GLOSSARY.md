# Glossary

| Term | Meaning |
|---|---|
| Base | The main board: ESP32-S3, AMOLED display, LoRa, GNSS, sensors, inputs |
| Sled | The add-on board that carries the Arduino UNO Q and the AD8317 detector |
| Pogo link | The spring-pin connection between base and sled (UART, I2C, wake line) |
| SignalOS | The LVGL-based user interface and shell |
| Sweep | The broadband RF level mode, compared against visible Wi-Fi/BLE emitters |
| Mesh | The LoRa peer-to-peer message network (managed flooding, no subscription) |
| SOS | Hold the SOS button for 2 seconds to broadcast position and sound the siren |
| AOD | Always-on display |
| HAL | Hardware abstraction layer: one interface per peripheral (`hal.h`) |
| OTA | Over-the-air firmware update from GitHub releases |
