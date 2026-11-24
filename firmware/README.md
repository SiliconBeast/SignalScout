# Firmware

PlatformIO project for the ESP32-S3 base unit, with a desktop simulator and host unit tests.

| Path | Contents |
|---|---|
| `lib/ss_core` | Portable logic |
| `lib/ss_services` | Services, one per subsystem |
| `lib/ss_ui` | SignalOS (LVGL 9) |
| `lib/ss_mock` | HAL mocks |
| `src/device` | Device drivers and tasks |
| `src/sim`, `src/web` | Simulator and browser build |
| `test/` | Host unit tests |

Build commands are in [docs/BUILDING.md](../docs/BUILDING.md).
