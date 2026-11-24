## What changed

<!-- One or two sentences. -->

## Checks

- [ ] `pio test -e native` passes
- [ ] `pio run -e sim` and `pio run -e device` build (if firmware changed)
- [ ] `python3 tools/gen_pins.py` and `python3 tools/sync_shared.py --check` leave no diff (if hardware or `proto.h` changed)
- [ ] Docs updated if behaviour changed
