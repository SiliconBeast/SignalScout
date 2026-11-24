# GNSS

The u-blox MAX-M10S is driven with UBX only. At boot the firmware sends CFG-VALSET to enable NAV-PVT at 1 Hz, GPS + Galileo + BeiDou, and optional power save. The M10 has no legacy CFG-* messages.

Parsing and configuration live in `firmware/lib/ss_core/ubx.*`.

The navigation arrow uses GPS course over ground, so it only settles while moving (there is no magnetometer).
