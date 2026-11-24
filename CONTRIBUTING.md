# Contributing

Signal Scout is a personal hardware and firmware project. Issues and small pull requests are welcome.

## Before you start

- Read the [README](README.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
- The KiCad boards are the source of truth for pins. After a hardware change run `python3 tools/gen_pins.py`. Do not edit `firmware/include/pins.h` or `docs/PINMAP.md` by hand.
- `firmware/lib/ss_core/proto.h` is copied into the sled sketch. After editing it run `python3 tools/sync_shared.py`.

## Checks

From `firmware/`:

```sh
pio test -e native    # unit tests on the host
pio run -e sim        # simulator build
pio run -e device     # ESP32-S3 firmware build
```

From the repository root:

```sh
python3 -m unittest discover -s sled/tests -v
python3 tools/sync_shared.py --check
```

CI runs the same checks on every pull request.

## Scope

The firmware receives and analyses. It has no deauthentication, jamming or spoofing features, and pull requests that add them will not be merged.
