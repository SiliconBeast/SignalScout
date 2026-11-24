# Architecture

## Layers

| Layer | Path | Role |
|---|---|---|
| Core | `firmware/lib/ss_core` | Portable logic: pogo protocol, mesh, correlation, battery, geo, UBX, weather, OTA manifest, JSON, calendar |
| Services | `firmware/lib/ss_services` | One per subsystem, running on the HAL |
| UI | `firmware/lib/ss_ui` | SignalOS (LVGL 9): shell, apps, theme |
| Mocks | `firmware/lib/ss_mock` | A mock for every HAL interface |
| Device | `firmware/src/device` | ESP32-S3 drivers and FreeRTOS tasks |
| Simulator | `firmware/src/sim` | Desktop simulator and screenshot renderer |

## Rules

- The HAL (`hal.h`) has one interface per peripheral. Services only see the HAL.
- The UI talks to the rest of the firmware through the message bus: it pops `Event`s and pushes `Cmd`s and never touches a driver or a service.
- The same UI code runs on the device, in the desktop simulator and in the browser build.
