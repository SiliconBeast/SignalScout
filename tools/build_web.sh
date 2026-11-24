#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Browser simulator: SignalOS compiled to WebAssembly with Zig's clang
# (pip install ziglang). Output: web/signalos.wasm next to web/index.html.
# Serve web/ with any static server (python3 -m http.server -d web).
set -euo pipefail
cd "$(dirname "$0")/.."
LVGL=${LVGL_DIR:?set LVGL_DIR to an LVGL v9.2.2 checkout}
ZIG="python3 -m ziglang"; OUT=build-web; mkdir -p $OUT web
FLAGS="-target wasm32-wasi -Os -DLV_CONF_INCLUDE_SIMPLE -Ifirmware/include -I$LVGL -Ifirmware/lib/ss_core -Ifirmware/lib/ss_services -Ifirmware/lib/ss_mock -Ifirmware/lib/ss_ui"
SRC=$(ls firmware/lib/ss_core/*.cpp firmware/lib/ss_services/*.cpp firmware/lib/ss_ui/*.cpp firmware/lib/ss_ui/*.c firmware/lib/ss_ui/fonts/*.c firmware/src/web/main.cpp; find $LVGL/src -name '*.c')
printf '%s\n' $SRC | xargs -P "$(nproc)" -I{} sh -c '
  f="{}"; o="'$OUT'/$(echo "$f" | tr "/." "__").o"
  [ -f "$o" ] && [ "$o" -nt "$f" ] && exit 0
  case "$f" in *.cpp) '"$ZIG"' c++ '"$FLAGS"' -std=c++17 -fno-exceptions -fno-rtti -c "$f" -o "$o";; *) '"$ZIG"' cc '"$FLAGS"' -c "$f" -o "$o";; esac'
$ZIG c++ -target wasm32-wasi -Os -fno-exceptions -mexec-model=reactor -Wl,--export=malloc -Wl,--initial-memory=33554432 -Wl,--max-memory=268435456 $OUT/*.o -o web/signalos.wasm
ls -la web/signalos.wasm
