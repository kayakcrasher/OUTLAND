#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mode="${1:-release}"
case "$mode" in
    release) binary=build/release-mobile-performance/outland ;;
    dev) binary=build/dev-mobile-performance/outland ;;
    *) echo 'Usage: bash tools/run_termux.sh [release|dev]' >&2; exit 2 ;;
esac
if [[ ! -x "$binary" ]]; then
    echo "Build $binary first; see docs/mobile-performance-handoff.md." >&2
    exit 1
fi
if [[ -z "${DISPLAY:-}" ]]; then
    echo 'Start Termux:X11, then set DISPLAY to its display (usually export DISPLAY=:0).' >&2
    exit 1
fi
if [[ "${LIBGL_ALWAYS_SOFTWARE:-}" == 1 ]]; then
    echo 'LIBGL_ALWAYS_SOFTWARE=1: rendering is forced onto the CPU; this may limit FPS.' >&2
fi
# Keep the raylib 6 runtime paired with its headers; avoid the old local raylib 5.5.
exec env LD_LIBRARY_PATH="${PREFIX:?Termux PREFIX is missing}/lib" LD_PRELOAD= "$binary"
