#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
build=${OUTLAND_WEB_BUILD:-$root/build/web}
raylib=${OUTLAND_RAYLIB_SOURCE:-$build/native/_deps/raylib-src}
mkdir -p "$build/boundary-test"
em++ -std=c++20 -O2 -UNDEBUG -I"$root/include" -I"$raylib/src" \
 "$root/src/platform/Runtime.cpp" "$root/tests/platform/test_browser_boundary.cpp" \
 -sASYNCIFY=1 -sASYNCIFY_IGNORE_INDIRECT=1 -sASYNCIFY_STACK_SIZE=262144 \
 -sEXIT_RUNTIME=1 --pre-js "$root/tests/platform/browser_fixture.js" \
 -o "$build/boundary-test/test.js"
node "$build/boundary-test/test.js"
node "$root/tests/platform/test_web_bootstrap.js" "$root/platform/web/shell.html"
