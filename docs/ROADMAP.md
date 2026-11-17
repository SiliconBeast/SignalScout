# Roadmap

What Signal Scout can become with software updates alone, and what would need new hardware. The hardware covers seven capabilities: Wi-Fi and BLE (ESP32-S3, plus dual-band on the UNO Q sled), a broadband RF detector (AD8317), GNSS (MAX-M10S), LoRa (SX1262), motion (LSM6DSO), a barometer (BME280) and haptics (DRV2605L), with a touch AMOLED and a Linux board on the sled. Most of the product's value can therefore ship over the air (see [SIGNALOS.md](SIGNALOS.md#over-the-air-updates)).

★ = first in line.

## Software only (OTA)

### Safety
| | Feature | How |
|---|---|---|
| ★ | **Meshtastic compatibility** | ESP32-S3 + SX1262 is the same core as common Meshtastic boards. A compatibility mode joins the existing community mesh. |
| ★ | **Fall / crash detection with auto-SOS** | IMU sees an impact followed by stillness, then a 30 s haptic countdown before SOS goes out automatically. |
| ★ | **Dead-man check-in** | "If I haven't checked in by 18:00, broadcast my position and track." |
| ★ | **Unknown-tracker detection** | Flags BLE devices that stay with you across locations (AirTag, Tile, cheap BLE trackers). |
| | Group radar | Other units' positions as a compass and distance view. |
| | Find my unit | Ping another unit over the mesh; it beeps and buzzes. |
| | Storm alarm | Rapid pressure drop triggers a notification (builds on the barometer forecast). |

### RF and network tools (Hackerspace)
| | Feature | How |
|---|---|---|
| ★ | **Wi-Fi site survey heat map** | Walk the site; RSSI per AP tagged with GPS (or steps indoors); export CSV/KML from the web console. |
| ★ | **RF fingerprinting by timing** | The AD8317 can't see frequency, but the STM32 can sample burst timing. Microwave ovens, BLE advertising, phone uplink bursts and beacons have distinct patterns, so "unexplained" becomes "looks like a phone transmitting". |
| | Network health check | DHCP/DNS timing, gateway ping, captive-portal detection, throughput, roaming behaviour (when joined). |
| | RF exposure log | Broadband level over a day or a route, plotted. |
| | Learned RF baseline on the sled | The QRB2210 learns a room's normal and flags deviations: on-device ML that serves the instrument rather than a chatbot. |
| | Crowd density | Counts of anonymous probe requests only; nothing stored per device. |

### Outdoor and navigation (Adventure)
| | Feature | How |
|---|---|---|
| ★ | **Breadcrumb track-back** | Record the route and follow it back, not just a straight line to a waypoint. |
| | GPX trip recorder | Tracks and stats on the card. |
| | Offline vector maps | OpenStreetMap extracts on microSD, line rendering in PSRAM. |
| | Haptic navigation | Left/right buzz patterns so the screen stays in your pocket. |
| | Barometric altitude | Blended with GNSS; elevation gain. |
| | Sunrise / sunset | Dusk warnings on the AOD. |

### Platform
| | Feature | How |
|---|---|---|
| ★ | **Phone companion over BLE** | Free-text mesh messages from the phone keyboard; waypoint and log sync. |
| | Installable apps | Small scripting runtime (Berry or Lua) loading apps from the card, so the community can build on it. |
| | LoRaWAN mode | Telemetry through public gateways (The Things Network). |
| | More AOD faces, complications, profiles | e.g. an "Event" profile for AV crews. |

## Needs hardware (sleds or a board revision)

The sled connector is the expansion path; the sled ID EEPROM is already wired so the base can tell which sled is attached.

| Want | Needs |
|---|---|
| True compass heading | Magnetometer |
| Identify frequencies, spectrum view | SDR sled |
| 5 GHz without the sled | ESP32-C5 class base radio |
| NFC, RFID, infrared | Additional sled hardware |
| Voice notes, audio | Microphone and speaker |
| Messaging with no mesh peers in range | Satellite module (brings a subscription back) |

## Out of scope

No deauthentication, jamming or spoofing. Receive-only analysis plus the unit's own LoRa traffic is what makes the instrument credible for installers.

## Suggested demo order

1. Meshtastic compatibility: one device joins an existing network.
2. Fall detection with auto-SOS: safety with no subscription.
3. Wi-Fi site-survey heat map: a pro diagnostic.

None of them needs new hardware.
