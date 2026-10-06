#!/data/data/com.termux/files/usr/bin/bash

set -e

echo "================================"
echo "       OUTLAND X11 LAUNCH       "
echo "================================"

export DISPLAY=:0

if ! pgrep -f "termux-x11" >/dev/null 2>&1; then
    echo "[OUTLAND] Starting Termux:X11 server..."

    termux-x11 :0 >/dev/null 2>&1 &

    sleep 2
fi

echo "[OUTLAND] DISPLAY=$DISPLAY"

if [ ! -x "./build/outland" ]; then
    echo "[OUTLAND] ERROR: build/outland does not exist."
    echo "[OUTLAND] Build the project first."
    exit 1
fi

echo "[OUTLAND] Launching..."
echo

./build/outland
