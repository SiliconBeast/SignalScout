# Screenshots

Rendered by the desktop simulator from the same UI code that runs on the device:

```sh
cd firmware
pio run -e sim
.pio/build/sim/program --shots ../docs/screenshots
```

`tools/contact_sheet.py` tiles them into `sheet_N.png` (ignored by git).
