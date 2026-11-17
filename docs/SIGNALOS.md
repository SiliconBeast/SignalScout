# SignalOS

SignalOS is the operating shell of Signal Scout. Its job is to make an RF instrument, a GPS/LoRa beacon and a small utility belt feel like one product, and to keep that product improving after it ships.

![Hackerspace](screenshots/sheet_1.png)
![Shell](screenshots/sheet_4.png)
![Adventure](screenshots/sheet_5.png)

## Design principles

1. **Instrument first.** Numbers are big, stable and labelled with units. Lists don't jump around: rows re-sort only when the order really changes, and screens that you watch while walking keep the display on.
2. **Every action works with buttons and with touch.** Gloves, rain and a cracked screen are normal in the field. The D-pad reaches everything; touch and gestures are shortcuts.
3. **Honest about what it knows.** The UI shows where data comes from: barometer or online forecast, verdict computed on the sled or on the base, network time or GPS time. Sweep says "unexplained energy", never "bug found".
4. **Safety can't be locked out.** SOS works from any screen, from the always-on display, with the lock switch on, during an update and on low battery. Updates refuse to install or restart while an SOS is active.
5. **Quiet by default.** One banner per app per 45 s; the rest go to the list. A mesh SOS from someone else always gets a banner and wakes the screen.

## Two profiles

| | Hackerspace | Adventure |
|---|---|---|
| For | Installers, RF and network work, the lab | Hiking, trips, off-grid |
| Look | Black, phosphor green (#39FF88), hairline borders, JetBrains Mono for readouts, `// SECTION` labels | Black, sunset amber (#FF9F1C), soft cards, large Space Grotesk, colour app tiles |
| Launcher order | Wi-Fi, Bluetooth, Channels, Sweep, Console, Mesh, Level, Updates, Settings | Position, Navigate, Mesh, Weather, Torch, Level, Sweep, Updates, Settings |
| Today widgets | RF floor + sparkline, air (networks / best channel), links | Weather, distance and bearing to the target waypoint, last mesh message |
| Defaults | Wi-Fi join available | Wi-Fi off to save power |

You pick the profile on first boot. You can switch from the shade or Settings at any time; the whole shell rebuilds in the new style.

## Navigation model

| Gesture | Button | Result |
|---|---|---|
| Swipe down from the top edge (follows the finger) | Side button on the launcher | Shade: toggles, brightness, quick actions, notifications |
| Swipe up on the shade | B or Side | Close the shade |
| Swipe up from the bottom edge | Side button, or hold B | Home |
| Swipe right from the left edge | B | Back |
| Long-press an app icon | Hold A on an icon | Quick actions sheet |
| Swipe left/right on the launcher | D-pad past the edge | Change launcher page (Today / apps) |
| Swipe up on the lock screen | A | Unlock |
| Tap the always-on display | Any key | Lock screen |
| | Hold SOS 2 s | Start or cancel SOS, from anywhere |
| | Lock switch | Always-on display; keys ignored except SOS |

Page transitions use a parallax push (the new page slides 100 %, the old one 33 %, ease-out, 280 ms). The shade, unlock and home gestures track the finger and settle with velocity-aware snapping. Banners drop in with a slight overshoot; the focus ring springs; meters and needles are eased every frame (Nav arrow, Level bubble, Sweep meter, update progress ring).

## Shell components

- **Status bar.** Clock (GPS or network time with the forecast's time zone), live-activity pill, link glyphs (shown only when relevant: card logging, sled, mesh, GPS, Wi-Fi), drawn battery.
- **Live-activity pill.** Shows one ongoing thing, by priority: SOS, update progress, navigation (waypoint and distance), hotspot (clients), logging. It animates its width like a Dynamic Island.
- **Shade.** 8 toggles (Wi-Fi, Hotspot, Torch, Log, Haptics, Siren, GPS power save, Profile), a brightness slider, quick actions (save waypoint, send OK, sleep), and the notification list.
- **Notifications.** Mesh messages, mesh SOS, battery 20 % and 10 %, sled attached or detached, unexplained RF (once a minute at most), update available or ready, Wi-Fi joined, waypoint reached.
- **Lock screen and always-on display.** Big light clock, date, three complications you choose (weather, waypoint, battery, mesh, satellites, RF level, Wi-Fi), notifications on the lock screen, an unread count on the always-on display. The AOD runs at 12 % brightness, draws nothing else, and moves the clock a few pixels every minute to spread OLED wear.
- **Quick actions.** Per app: save waypoint or check in (Position), canned replies (Mesh), channel view (Wi-Fi), refresh (Weather), start the hotspot (Console), red light (Torch), check now (Updates), always-on and profile (Settings).

## Apps

| App | What it does |
|---|---|
| Wi-Fi / Channels / Bluetooth | Passive scans, 2.4 GHz channel load with the least congested of 1/6/11, AP detail with signal history |
| Sweep | Broadband RF level from the sled compared with what the visible Wi-Fi/BLE explains |
| Position / Navigate / Mesh | GNSS fix, waypoints, eased return-to-waypoint needle, LoRa mesh messages and quick replies |
| Weather | Online forecast (hourly, 3 days) when on Wi-Fi; offline Zambretti forecast from the BME280 pressure trend, always shown |
| Console | Web console hotspot with a Wi-Fi join QR code, station status, endpoint list |
| Torch | White or red (night vision) full-screen light, brightness on Up/Down |
| Level | Bubble level from the LSM6DSO, zero on any surface |
| Updates | Check, download, verify, restart |
| Settings | Grouped: profile, display and AOD complications, connectivity, radio, sound, units and time zone, storage, system |

## Connectivity

Wi-Fi is **off** until you turn it on. When on, the device joins networks saved in `/wifi.txt` on the microSD (one `ssid<TAB>password` per line; add them from the web console) and uses the connection for:

- **Time:** SNTP; the time zone comes from the forecast reply.
- **Weather:** Open-Meteo (no key, CC BY 4.0), refreshed every 30 min using the last GPS fix.
- **Updates:** checked on join and daily.

Scanning stays passive and separate from the station connection.

**Web console.** Starting the hotspot creates `SignalScout-XXXX` (WPA2, password shown on the screen and in the QR code) and serves `http://192.168.4.1`:

| Route | |
|---|---|
| `GET /` | Status, file list, forms |
| `GET /f?p=/path` | Download a file from the card (`/wifi.txt` is never served) |
| `POST /wifi` | Save a network |
| `POST /settings` | Apply settings text (same format as `/settings.txt`) |
| `POST /update` | Upload `firmware.bin`; verified and armed, restart from the device |

The console only runs on the hotspot, never on a joined network.

## Over-the-air updates

1. Each release publishes `firmware.bin` and `manifest.json` (`tools/make_release.py`, `.github/workflows/release.yml`).
2. The device fetches `https://github.com/SiliconBeast/SignalScout/releases/latest/download/manifest.json` over TLS (ESP-IDF CA bundle). It offers the update only if it is newer and the running version is at least `min_version`. Plain-HTTP firmware URLs are rejected.
3. The download streams into the spare app slot (16 MB flash: two 6.5 MB slots). The device checks the size and the SHA-256 against the manifest, then ESP-IDF validates the image.
4. On restart the new image runs in "pending verify". If it crashes before 30 s of UI, the bootloader rolls back automatically. Holding **B** at power-on boots the previous firmware by hand.
5. No install or restart while an SOS is active.

### What OTA can ship

The device is designed so that most of its value can grow after it ships:

| Area | Examples that need only a firmware update |
|---|---|
| Apps | Spectrum waterfall from sled sweeps, Wi-Fi survey heat-map logging, BLE tracker finder, LoRa range-test app, trip recorder (GPX to the card), compass once a magnetometer sled exists |
| Shell | New AOD faces and complications, more quick actions, notification rules, new profiles (e.g. "Event" for AV installers) |
| RF | Detector calibration tables per sled revision, new correlation models, regional LoRa plans (EU 868, AU 915) |
| Mesh | Protocol upgrades with version negotiation, store-and-forward, encrypted direct messages |
| Power | Smarter duty cycles from field logs, GNSS assistance (AssistNow) when online |
| Sled | The UNO Q app updates separately through Arduino App Lab; the pogo protocol carries a version so base and sled can be updated independently |

## Assets

- **Icons.** Drawn for this product in `tools/gen_icons.py` (one vector description per glyph). It outputs colour (Adventure) and phosphor (Hackerspace) PNG tiles in `assets/icons/`, plus A8 alpha masks the firmware recolours at runtime. Nothing is taken from another product; the visual direction (black AMOLED, rounded tiles, playful but precise) is inspired by Kode Dot, but no Kode assets or code are used.
- **Fonts.** Space Grotesk (UI and clock) and JetBrains Mono (data), both SIL OFL 1.1, converted with `tools/gen_fonts.sh`. Licences in `assets/fonts/`.

## Performance notes (ESP32-S3)

- Two 40-row DMA buffers in internal RAM; LVGL renders one while the other is sent over QSPI. Brightness changes are applied between flushes so no other task touches the SPI bus mid-frame.
- Fonts are stored uncompressed (faster glyph rendering; flash is plentiful).
- The AOD hides the app stage, so only the clock block is redrawn.
- Animations are translate-only where possible; transforms are used only on small objects (focus ring, pills).

At 40 MHz QSPI a full-screen redraw is about 20 ms of transfer, so a full-screen slide runs at roughly 30 to 45 fps. Raising `SS_LCD_SPI_HZ` after bring-up (if the flex allows it) is the biggest lever.
