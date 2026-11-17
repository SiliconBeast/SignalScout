# Pogo-link protocol (base to sled)

The base (ESP32-S3) and the sled (UNO Q microcontroller) talk over the pogo UART: 115200 baud, 8N1, 3.3 V. Base `SLED_TX` (IO43) goes to UNO Q `D0`, and UNO Q `D1` goes to base `SLED_RX` (IO44).

The single source of truth is `firmware/lib/ss_core/proto.h`. The sled sketch compiles a copy of the same file (`tools/sync_shared.py` keeps it identical, CI checks it). `sled/signalscout_sled/python/ssproto.py` mirrors it for tools and tests. Both are tested against `tests/vectors/proto.csv`.

## Frame

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 2 | Start | `0xA5 0x5A` |
| 2 | 1 | Version | `1`. Frames with another version are dropped. |
| 3 | 1 | Type | See below |
| 4 | 1 | Sequence | Per-sender counter, wraps at 255 |
| 5 | 1 | Length | Payload bytes, 0 to 64 |
| 6 | n | Payload | Little-endian fields |
| 6+n | 2 | CRC | CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over Version to end of payload, low byte first |

A receiver that sees a bad CRC, a length over 64 or an unknown version discards the frame and searches for the next start bytes. There are no acknowledgements; every message is periodic or idempotent.

## Messages

Power values are in centi-dBm (dBm x 100) as signed 16-bit.

| Type | Name | Direction | Payload |
|---|---|---|---|
| 0x01 | HELLO | both | `role u8` (1 base, 2 sled), `proto_ver u8`, `fw_ver u16` |
| 0x02 | PING | base to sled | none. Sent every second while not in Sweep. |
| 0x03 | PONG | sled to base | none |
| 0x10 | RF_SAMPLE | sled to base | `t_ms u32`, `avg i16`, `min i16`, `max i16`, `floor i16`. Every 50 ms while streaming. |
| 0x11 | VERDICT | sled to base | `state u8` (0 quiet, 1 explained, 2 unexplained), `confidence u8` (0 to 100), `excess_cdB i16`, `expected_cdBm i16`, `n_wifi u8`, `n_ble u8`. About once a second, from the Linux side. |
| 0x20 | SCAN_SUMMARY | base to sled | `n_wifi u8`, `n_ble u8`, `strongest_wifi i16`, `strongest_ble i16`, `sum i16`. Every 2 s in Sweep. |
| 0x30 | SLEEP | base to sled | none. Stop streaming. |
| 0x31 | WAKE | base to sled | none. Start streaming. |

## Behaviour

- The base raises `SLED_WAKE` and sends WAKE then HELLO when the user opens Sweep, and SLEEP when they leave. The sled streams while either the WAKE frame or the wake line says so.
- The base treats the sled as linked while valid frames keep arriving, and as detached after 2.5 s of silence.
- If no VERDICT arrives for 3 s, the base classifies each sample itself with the same algorithm (`corr::classify`) and its own scan. The UI says which side made the comparison.
- At reset the ESP32-S3 boot ROM prints its log on IO43. The sled sees that as noise, which the parser skips.

## Verdict algorithm

Inputs: detector level `m`, rolling noise floor `f`, and the power sum `k` of every visible Wi-Fi and BLE emitter's RSSI (all dBm).

1. `expected = 10 log10(10^(f/10) + 10^(k/10))`
2. If `m - f < 3 dB`: **quiet**.
3. Else if `m - expected <= 6 dB`: **explained**.
4. Else: **unexplained**, with confidence rising to 100 % at 20 dB over expected.

RSSI at the scanning radio is only a rough stand-in for what the detector antenna receives, so the 6 dB margin is deliberately wide. Tune it in `corr::Config` and `correlate.Config` (keep them equal and regenerate the vectors with `tools/gen_vectors.py`).

# LoRa mesh packet

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | Magic `0x53` |
| 1 | 1 | Version (high nibble, `1`), hops so far (low nibble) |
| 2 | 1 | Type: 1 text, 2 position, 3 SOS, 4 SOS clear |
| 3 | 1 | TTL (hops left) |
| 4 | 2 | Source node ID |
| 6 | 2 | Destination (`0xFFFF` = everyone) |
| 8 | 2 | Message ID |
| 10 | n | Payload, up to 96 bytes |
| 10+n | 2 | CRC-16 over the packet with hops and TTL zeroed (they change at each relay) |

Position payload (types 2 to 4): `lat i32` and `lon i32` in 1e-7 degrees, `battery u8` %, `fix u8`.

Radio: 500 kHz bandwidth, SF9, CR 4/5, 8-symbol preamble, sync word `0x5C`, eight channels from 903 to 924 MHz. A node relays each new packet once after a random delay (0.2 to 1.5 s, SOS 50 to 300 ms) while TTL is above 1. Text uses TTL 3 and SOS uses TTL 5.
