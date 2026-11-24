# Testing

| What | Command |
|---|---|
| Firmware unit tests (host) | `cd firmware && pio test -e native` |
| Sled Python tests | `python3 -m unittest discover -s sled/tests -v` |
| Shared protocol copy matches | `python3 tools/sync_shared.py --check` |
| Test vectors up to date | `python3 tools/gen_vectors.py && git diff --exit-code` |
| Every screen renders | `cd firmware && .pio/build/sim/program --shots shots` |

Test vectors in `tests/vectors/` are generated from the Python reference implementations and shared by the firmware and sled tests. Regenerate them with `tools/gen_vectors.py` rather than editing them.
