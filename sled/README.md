# Sled

`signalscout_sled/` is the Arduino App Lab app for the UNO Q: an STM32 sketch that samples the AD8317 detector and streams it to the base, and a Python side on the Linux processor that scans Wi-Fi/BLE and sends a verdict.

`tests/` holds the Python tests. Run `python3 tools/sync_shared.py` after editing `firmware/lib/ss_core/proto.h`.
