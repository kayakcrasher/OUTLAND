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
echo "Deploy $build/site together; open outland.html over HTTP(S)."
