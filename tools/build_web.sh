#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
build=${OUTLAND_WEB_BUILD:-$root/build/web}
command -v emcmake >/dev/null || { echo 'Activate Emscripten 4.0.20: source /path/to/emsdk/emsdk_env.sh' >&2;exit 1; }
extra=()
if [[ -n ${OUTLAND_RAYLIB_SOURCE:-} ]];then extra+=("-DFETCHCONTENT_SOURCE_DIR_RAYLIB=$OUTLAND_RAYLIB_SOURCE");fi
python3 "$root/tools/stage_runtime_assets.py" "$build/runtime"
emcmake cmake -S "$root" -B "$build/native" -DCMAKE_BUILD_TYPE=Release \
 -DOUTLAND_DEV_TOOLS=OFF -DOUTLAND_BUILD_TESTS=OFF -DOUTLAND_RUNTIME_ASSETS="$build/runtime" "${extra[@]}"
cmake --build "$build/native" --target outland -j"${OUTLAND_JOBS:-2}"
mkdir -p "$build/site"
cp "$build/native"/outland.{html,js,wasm,data} "$build/site/"
cp "$build/runtime/runtime-report.json" "$build/site/"
# The app icon (tools/generate_app_icon.py): tab, home screen, install.
cp "$root"/platform/web/icon-{32,180,192,512}.png "$root/platform/web/manifest.webmanifest" "$build/site/"
# The site root (e.g. https://user.github.io/OUTLAND/) opens the game.
cat > "$build/site/index.html" <<'HTML'
<!doctype html><html lang="en"><head><meta charset="utf-8"><title>OUTLAND</title>
<link rel="icon" type="image/png" href="icon-32.png"><link rel="apple-touch-icon" href="icon-180.png">
<meta http-equiv="refresh" content="0; url=outland.html">
<script>location.replace('outland.html'+location.search+location.hash)</script></head>
<body><a href="outland.html">Play OUTLAND</a></body></html>
HTML
echo "Deploy $build/site together; open outland.html over HTTP(S)."
