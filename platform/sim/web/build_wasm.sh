#!/bin/sh
# Builds the simulator core as WebAssembly (freestanding: no libc, no WASI, no imports) and, with --html, the
# single-file simulator page.
#   platform/sim/web/build_wasm.sh [--html] [outdir]       default outdir: build-wasm
# Needs clang with the wasm32 target and wasm-ld; for --html also python3.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
html=0
if [ "${1:-}" = "--html" ]; then html=1; shift; fi
out=${1:-"$root/build-wasm"}
mkdir -p "$out"
CC=${CC:-clang}

"$CC" --target=wasm32 -O2 -ffreestanding -nostdlib -fuse-ld=lld \
    -ffp-contract=off -Wall -Wextra -Wpedantic -Wshadow -Wno-unused-parameter \
    -std=c11 -DBK_MINLIBC_ALIASES -DBK_FRAME_MAX_W=336 -DBK_FRAME_MAX_H=536 \
    -I"$here/libc" -I"$root/firmware/include" -I"$root/firmware/games/tetris" -I"$root/firmware/games/snake" -I"$root/platform/sim" \
    -Wl,--no-entry -Wl,--export-dynamic -Wl,-z,stack-size=65536 -Wl,--strip-all \
    -o "$out/brick-sim.wasm" \
    "$here/sim_wasm.c" "$root/platform/sim/sim.c" "$root/firmware/catalog/catalog.c" "$here/libc/libc_min.c" \
    "$root"/firmware/core/*.c "$root"/firmware/games/*/*.c

echo "$out/brick-sim.wasm: $(wc -c < "$out/brick-sim.wasm") bytes"

if [ "$html" = 1 ]; then
    python3 -I "$here/make_page.py" "$here/index.html" "$out/brick-sim.wasm" "$out/brick-sim.html"
fi
