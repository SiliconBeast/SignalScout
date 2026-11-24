# Browser simulator

`index.html` and `signalos.wasm` run the real SignalOS UI, compiled to WebAssembly, inside a clickable device frame.

```sh
python3 -m http.server -d web 8000
```

Open http://localhost:8000. Rebuild the wasm with `tools/build_web.sh` (needs `pip install ziglang`).
