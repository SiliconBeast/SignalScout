# First power-on checklist

Work through this in order with the base board alone, then add the sled. Each step names what to expect and where to look if it fails. Net and GPIO names match [PINMAP.md](PINMAP.md).

You need a bench supply or a charged 1S LiPo, a multimeter, a USB-C cable, and ideally a second LoRa node or an SDR for step 7.

## 0. Before applying power

1. Check for shorts with a meter: `VBAT`, `VSYS`, `+3V3` and `VBUS` to `GND` (test points TP1 to TP4). None should read under a few hundred ohms.
2. Fit both antennas. Never transmit LoRa without the SMA antenna attached.
3. Leave the display flex and the sled disconnected for steps 1 to 3.

## 1. Power rails

| Check | Expected |
|---|---|
| USB-C in, no battery: `VBUS` (TP2) | 4.75 to 5.25 V |
| `VSYS` | about 4.4 V on USB, battery voltage on battery |
| `+3V3` (TP1) | 3.25 to 3.35 V |
| Battery fitted, USB in: `VBAT` (TP3) | rising towards 4.2 V |
| Current from USB with a blank ESP32 | under 100 mA plus charge current |

If `+3V3` is low or missing, stop and check the TPS63001 area (U12, L1) before going on.

**RGB LED**: note whether the LED lights before any firmware runs. The LED pins have no series resistors and rely on the expander's current-sink mode, which the firmware sets at boot (see TODO.md, item H3).

## 2. Flash and serial log

1. `cd firmware && pio run -e device -t upload`. If the port does not appear, hold BOOT, tap EN, release BOOT.
2. `pio device monitor`. Expect one line:

   `Signal Scout 1.0.0  boot faults: 0x40`

`0x40` means "no microSD card", which is normal. Other bits name a part that did not answer:

| Bit | Part | I2C address |
|---|---|---|
| 0x01 | AW9523B expander | 0x58 |
| 0x02 | MAX17048 fuel gauge | 0x36 |
| 0x04 | DRV2605L haptic driver | 0x5A |
| 0x08 | BME280 | 0x76 |
| 0x10 | LSM6DSO | 0x6A |
| 0x20 | Display SPI setup | - |
| 0x40 | microSD | - |

## 3. I2C scan

All five addresses in the table above should be present on `I2C_SDA`/`I2C_SCL` (IO4/IO5), plus the touch controller once the display flex is connected. If one is missing, check its supply and the 2.2 k pull-ups (R10, R11). The MAX17048 only answers with a battery connected.

The expander matters most: keys, the display power switch, the GNSS reset line and the LED all hang off it. With it missing, only the SOS button works.

## 4. Display

Connect the flex and reboot.

| Check | Expected |
|---|---|
| `VCI` rail after boot | 3.3 V (the firmware sets `DISP_PWR_EN` high) |
| Screen | Home launcher: four tiles, status bar on top |

If the screen stays black:
1. Confirm `DISP_RESET` (IO8) pulses low at boot and `LCD_SCK` (IO12) toggles.
2. Confirm the panel controller. `firmware/include/board.h` assumes **CO5300, 410 x 502, column offset 22**. Check the TOQ206XVT-01 datasheet and adjust `SS_LCD_W`, `SS_LCD_H`, `SS_LCD_COL_OFFSET` and the init sequence in `src/device/display.cpp`.
3. Check the `DISP_IM0`/`DISP_IM1` straps select QSPI.

If the picture is shifted sideways or shows a noisy strip at one edge, the column offset is wrong. If colours look wrong, remove the byte swap in `flush_cb`. If it works but glitches, lower `SS_LCD_SPI_HZ` to 20 MHz.

## 4b. Touch

1. The serial boot line should not include fault bit `0x80` (touch not found). The driver probes I2C `0x38` (FT3168 class) and `0x15` (CST816 class).
2. Tap an app icon: it should open. If touches land mirrored or rotated, swap or invert the axes in `src/device/touch.cpp`.
3. Swipe down from the very top: the shade should follow your finger. Swipe up from the bottom edge: home.

## 5. Keys, haptics, siren, LED

1. Each D-pad key moves the focus ring; A opens an app; B goes back; the side button returns home (and opens the shade from the launcher). You should feel a click on each move.
2. Slide the lock switch: the always-on display appears. If it is inverted, flip the `EXP_KEY_LOCK` test in `hal_device.cpp`.
3. Plug in USB: the battery icon turns into a charge symbol and the LED glows dim orange.
4. Hold SOS for 2 seconds: the status bar turns red, the siren alternates two tones and the LED blinks red. Hold again to cancel. Turn the siren off in Settings first if you are indoors.

## 6. GNSS

Go outdoors or to a window and open **Beacon**.

| Check | Expected |
|---|---|
| Within a few seconds | "SEARCHING" with a satellite count that changes |
| Within about a minute, open sky | "3D FIX", coordinates, time in the status bar |

If the count stays at 0 and no time appears, the receiver is not talking:
1. Scope `GNSS_TXD` (IO2): there should be data every second.
2. Try 38400 baud (`SS_GNSS_BAUD` in `board.h`). 9600 is the M10 factory default.
3. Check `GNSS_RST` (expander P1_4) is high.

Press A on the Position tab to save a waypoint, walk 50 m, and check the Waypoints tab shows a sensible distance and bearing.

## 7. LoRa loopback

1. Position app: the LoRa line should read `node XXXX` and a frequency. "Radio not responding" means the SX1262 did not initialise: check `LORA_BUSY` (IO39), `LORA_NRST` (IO41), the shared SPI lines, and that the module really has a 1.8 V TCXO on DIO3 (set in `hal_device.cpp`).
2. With a second unit: send a quick reply from one (Mesh app, A) and confirm it appears on the other with RSSI and hop count. Then trigger SOS on one and confirm the other shows it.
3. With one unit and an SDR: tune to 903.0 MHz (channel 1) and confirm a 500 kHz wide burst when you send a message.
4. With neither: the "sent" counter increasing only proves the radio accepted the packet.

## 8. microSD

Insert a FAT32 card. The card icon lights when "Log to microSD" is on in Settings. After a minute with a fix, the card should hold `track.csv`; `settings.txt` and `waypoints.csv` appear when you change a setting or save a waypoint.

## 9. Sled

1. Flash the App Lab app in `sled/signalscout_sled` to the UNO Q.
2. Mate the sled and open **Sweep**. The pill should leave "NO SLED" within a second and the level should move.
3. Sanity check: the reading should rise when a phone held next to the antenna is uploading, and fall back afterwards.
4. The footer shows where the comparison runs. "Compared on the sled" confirms the Linux side is scanning and sending verdicts.
5. Calibrate: feed a known level into the U.FL input and adjust `SLOPE_V_PER_DB`, `INTERCEPT_DBM` and `FRONT_END_LOSS_DB` in `sketch.ino`.

If it stays on "NO SLED": check `SLED_TX`/`SLED_RX` continuity through the pogo pins and the 1 k series resistors, and that the sketch's `Serial` really is the D0/D1 UART on your UNO Q core.

## 10. Wi-Fi, web console and updates

1. Open **Console** and press A. The QR code should join a phone to `SignalScout-XXXX`; `http://192.168.4.1` should list the card's files.
2. Add your network in the console, then turn on **Wi-Fi** in the shade. Within about 15 s the status bar should show the Wi-Fi glyph, the clock should match network time, and Weather should show an online forecast (it needs one GPS fix first).
3. Publish a test release with a higher version (tag `v1.1.1`). Updates should offer it, download it, verify it and restart. Check that the serial line shows the new version.
4. Rollback check: flash a build that crashes at boot through OTA. The board should come back on the previous version by itself. Holding B at power-on should also boot the previous slot.
5. Start an SOS during a download: the install must not restart the device until the SOS is cancelled.

## 11. Sleep current

Settings, Sleep now. The screen goes dark; any key wakes the unit (it reboots). Measure battery current while asleep and record it; see TODO.md for what stays powered.
