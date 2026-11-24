# Building

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html), then from `firmware/`:

```sh
pio run -e device                 # build
pio run -e device -t upload       # flash over USB-C
pio device monitor                # serial log (USB CDC)
pio test -e native                # unit tests on the host
pio run -e sim                    # simulator (needs libsdl2-dev)
```

If the board does not enter the bootloader by itself, hold BOOT, tap EN, release BOOT.

A plain CMake build of the tests and simulator is also available (`firmware/CMakeLists.txt`).

## Web simulator

```sh
pip install ziglang
tools/build_web.sh
python3 -m http.server -d web 8000
```

## Sled

Open `sled/signalscout_sled` in Arduino App Lab and run it on the UNO Q. Run `python3 tools/sync_shared.py` first if you changed `proto.h`.
