# Sweep

Sweep shows the broadband RF level from the sled's AD8317 detector as a number, a meter and a 30-second history. The level is compared with the Wi-Fi/BLE emitters the unit can see:

- Quiet: at the noise floor.
- Explained: consistent with the visible Wi-Fi/BLE activity.
- Unexplained: clearly more energy than the visible emitters account for.

It reports how much energy is present, not what the source is. The detector cannot tell frequency or identify a transmitter, and is uncalibrated until the sled sketch constants are set against a known source. If the sled's Linux side is not running, the base runs the same comparison on its own scan.
