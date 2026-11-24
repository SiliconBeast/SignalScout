# Tools

| Script | Purpose |
|---|---|
| `gen_pins.py` | Generate `firmware/include/pins.h` and `docs/PINMAP.md` from the KiCad boards |
| `sync_shared.py` | Copy the shared protocol source into the sled sketch (`--check` to verify) |
| `gen_vectors.py` | Regenerate `tests/vectors/*.csv` from the Python reference implementations |
| `gen_icons.py` | Render the SignalOS icon set |
| `gen_fonts.sh` | Convert fonts to LVGL C files (needs `lv_font_conv`) |
| `contact_sheet.py` | Tile screenshots into contact sheets |
| `make_release.py` | Write the OTA manifest for a release |
| `build_web.sh` | Build the browser simulator (needs `ziglang`) |
