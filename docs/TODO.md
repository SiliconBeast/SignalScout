# Needs real hardware to verify

Nothing in this firmware has run on the boards yet. The host tests cover the logic; this list is what only hardware can confirm. Steps refer to [BRINGUP.md](BRINGUP.md).

## Firmware assumptions to confirm

| # | Item | Where | Step |
|---|---|---|---|
| F1 | Panel controller is CO5300, 410 x 502, column offset 22, big-endian RGB565 | `board.h`, `display.cpp` | 4 |
| F2 | QSPI at 40 MHz is clean over the flex jumper | `SS_LCD_SPI_HZ` | 4 |
| F3 | MAX-M10S comes up at 9600 baud and accepts the CFG-VALSET keys | `board.h`, `ubx.h` | 6 |
| F4 | UBX-RXM-PMREQ puts the receiver in backup before deep sleep | `ubx.cpp` | 10 |
| F5 | E22 needs TCXO 1.8 V on DIO3 and DC-DC mode; RXEN polarity on IO1 | `hal_device.cpp` | 7 |
| F6 | AW9523B register setup: key inputs, LED current-sink mode, LED current range | `i2c_dev.cpp` | 5 |
| F7 | Lock switch polarity (low = locked is assumed) | `hal_device.cpp` | 5 |
| F8 | DRV2605L LRA settings suit the 8 mm LRA (auto-calibration is not run) | `i2c_dev.cpp` | 5 |
| F9 | PAM8904 tone frequencies (2.6 and 3.1 kHz) match the piezo's resonance | `services.cpp` | 5 |
| F10 | Wi-Fi scan, BLE scan and LoRa coexist without starving the UI | `main.cpp` task priorities | all |
| F11 | Deep-sleep wake from the expander interrupt (IO15) and SOS (IO21) | `hal_device.cpp` | 10 |
| F12 | UNO Q: `Serial` is the D0/D1 UART, `analogReadResolution(12)` works, Bridge calls behave as written | `sketch.ino` | 9 |
| F13 | UNO Q Linux: `nmcli`/`iw` and `bleak` are usable inside the App Lab container | `scanners.py` | 9 |
| F14 | AD8317 slope, intercept and front-end loss | `sketch.ino` | 9 |
| F15 | The 6 dB "explained" margin gives sensible verdicts in real rooms | `correlate.h` | 9 |

## Not implemented

- Touch input (the panel's touch controller is on the I2C bus and `TP_INT` is wired, but unused).
- Free-text messages (quick replies only).
- Sled ID EEPROM and sled fuel gauge on the second I2C bus (`SLED_SDA`/`SLED_SCL`) are not read.
- IMU is initialised but not used for anything yet.
- No firmware update over the air; flash over USB-C.

## Hardware observations from reading the netlist

These came up while deriving the pin map. None blocks bring-up, but each is worth a look before the next board revision.

| # | Observation | Effect |
|---|---|---|
| H1 | The sled UART is on UART0 (IO43/IO44). The ESP32-S3 boot ROM prints on IO43 at every reset. | The sled receives a burst of noise at base boot. The protocol parser skips it. |
| H2 | `SD_CS` is on IO0, shared with the BOOT button. | Pressing BOOT while running selects the SD card mid-transfer. Do not press BOOT during normal use. |
| H3 | The RGB LED cathodes go straight to the AW9523B with no series resistors. | Safe once the firmware selects current-sink mode. Before that (blank board, bootloader), the pins' reset state decides. If the LED is on hard at power-up, add resistors. |
| H4 | No VBUS-sense or power-good signal reaches the MCU; only `CHG_N`. | "USB plugged in, battery full" looks the same as "unplugged". |
| H5 | The MAX-M10S supply and backup pins are tied to +3V3 with no switch. | Deep-sleep current depends on the receiver accepting the software backup command (F4). |
| H6 | The LSM6DSO interrupt pins are not connected. | No wake-on-motion or tap detection. |
| H7 | `HAPTIC_EN` (IO45) and `SIREN_EN` (IO46) are strapping pins. | Fine as long as R40/R41 keep them low at reset, which the netlist shows. |
| H8 | The sled's charger input comes from the base's VBUS through the pogo pins. | The sled battery only charges while docked to a base that is on USB. |
