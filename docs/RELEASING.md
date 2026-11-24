# Releasing

1. Bump `SS_FW_VERSION` (and `SS_FW_VERSION_BCD`) in `firmware/include/board.h`.
2. Update [CHANGELOG.md](../CHANGELOG.md).
3. Tag the commit `v<version>` (for example `v1.1.0`) and push the tag.

The release workflow checks that the tag matches `SS_FW_VERSION`, builds the device firmware, writes `manifest.json` with `tools/make_release.py`, and publishes `firmware.bin` and `manifest.json`. Devices pick the update up from the latest release and verify it with SHA-256.
