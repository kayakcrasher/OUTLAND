#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
sdk=${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}
: "${sdk:?Set ANDROID_SDK_ROOT to an Android SDK with platform 35, build-tools 35.0.0, NDK 28.2.13676358}"
ndk=${ANDROID_NDK_HOME:-$sdk/ndk/28.2.13676358}
build=${OUTLAND_ANDROID_BUILD:-$root/build/android-arm64}
tools="$sdk/build-tools/35.0.0"
platform="$sdk/platforms/android-35/android.jar"
for path in "$ndk/build/cmake/android.toolchain.cmake" "$platform" "$tools/aapt2" "$tools/d8" "$tools/apksigner"; do
    test -f "$path" || { echo "Missing Android tool: $path" >&2; exit 1; }
done
for command in javac java keytool cmake python3; do command -v "$command" >/dev/null || { echo "Missing host tool: $command (install a full JDK, not just a JRE)" >&2;exit 1; };done
mkdir -p "$build"
extra=()
if [[ -n ${OUTLAND_RAYLIB_SOURCE:-} ]];then extra+=("-DFETCHCONTENT_SOURCE_DIR_RAYLIB=$OUTLAND_RAYLIB_SOURCE");fi
cmake -S "$root" -B "$build/native" -DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_static \
    -DCMAKE_BUILD_TYPE=Release -DOUTLAND_DEV_TOOLS=OFF -DOUTLAND_BUILD_TESTS=OFF "${extra[@]}"
cmake --build "$build/native" --target outland -j"${OUTLAND_JOBS:-2}"
python3 "$root/tools/stage_runtime_assets.py" "$build/runtime"
mkdir -p "$build/classes" "$build/dex" "$build/package/lib/arm64-v8a"
javac -source 8 -target 8 -bootclasspath "$platform" -d "$build/classes" "$root/platform/android/java/org/outland/game/GameActivity.java"
"$tools/d8" --lib "$platform" --min-api 26 --output "$build/dex" "$build/classes/org/outland/game/GameActivity.class"
"$tools/aapt2" link -I "$platform" --manifest "$root/platform/android/AndroidManifest.xml" -o "$build/resources.apk"
cp "$build/native/liboutland.so" "$build/package/lib/arm64-v8a/"
"$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" --strip-unneeded "$build/package/lib/arm64-v8a/liboutland.so"
python3 "$root/tools/package_android.py" "$build/resources.apk" "$build/runtime" \
    "$build/package/lib/arm64-v8a/liboutland.so" "$build/dex/classes.dex" "$build/unsigned.apk"
"$tools/zipalign" -P 16 -f 4 "$build/unsigned.apk" "$build/aligned.apk"
# Local installation signing; keep this key to allow APK upgrades without uninstalling/saves loss.
keystore=${OUTLAND_KEYSTORE:-$build/outland-local.keystore}
alias=${OUTLAND_KEY_ALIAS:-outland}
if [[ ! -f "$keystore" ]]; then
    if [[ -n ${OUTLAND_KEYSTORE:-} ]]; then echo 'Production keystore does not exist' >&2;exit 1;fi
    keytool -genkeypair -keystore "$keystore" -storepass android -keypass android -alias "$alias" -keyalg RSA -keysize 2048 -validity 10000 -dname 'CN=OUTLAND Local Build' -noprompt
fi
export OUTLAND_SIGNING_PASSWORD=${OUTLAND_SIGNING_PASSWORD:-android}
"$tools/apksigner" sign --ks "$keystore" --ks-key-alias "$alias" --ks-pass env:OUTLAND_SIGNING_PASSWORD --key-pass env:OUTLAND_SIGNING_PASSWORD --out "$build/OUTLAND.apk" "$build/aligned.apk"
"$tools/apksigner" verify --verbose "$build/OUTLAND.apk"
"$tools/zipalign" -c -P 16 4 "$build/OUTLAND.apk"
echo "Installable ARM64 APK: $build/OUTLAND.apk"
