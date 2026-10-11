# Multiplatform handoff and validation

Base: `b82077ee7fc36a96446a6de90eebea3cbde8b200` from current main. Existing commits and production assets are preserved. No game was launched, including in a browser or emulator.

## Verified builds

- Native ARM64 Android release built with CMake 4.4.4, NDK r28c (`28.2.13676358`), Android platform/build-tools 35 and a full Java 21 JDK. APK signing v2/v3, ZIP 16 KiB alignment, ELF load-segment 16 KiB alignment, ARM64 machine type and exported `ANativeActivity_onCreate` all verified.
- Emscripten 4.0.20/raylib 6 release built into HTML/JS/WASM/data. WASM magic, complete preload byte count, IndexedDB and frame hook checks passed. A separate compiled Node fixture executed three yielded frames, retaining local state and validating viewport resize/save hooks, without starting the renderer/game.
- Desktop DEV compiled: **59/59 CTest tests passed**. Desktop release compiled: **50/50 passed**, including the existing world, loot, NPC, vehicles, combat, animation, performance and Creator/layout tests applicable to each configuration.
- Seven platform unittest cases: four package checks, two compiled raylib platform adapter checks, one actual Android Runtime.cpp fixture. Android extraction/update/save isolation and the two C adapter fixtures also passed AddressSanitizer + UndefinedBehaviorSanitizer.
- Both production compile-command sets checked: DevLab, CreatorController, CreatorTouchUI, CreatorSession and BuildVerdaWorld sources absent. The shared published-map loader remains.
- Production package: **1,069 files / 146,670,688 bytes**. Unicode model/texture names are preserved as UTF-8 ZIP entries. Every manifest entry was found inside the APK. Download archives, staging files, Blender and FBX sources are excluded; production licenses/notices are included.

## Built artifacts

Local-install-signed APK: https://files.catbox.moe/ujwfow.apk

- 127,799,057 bytes
- SHA256 `b43d2800890c292bc9e815ed9e63d3f8eac33f107f9289e8d544b876f534fc2d`

Deployable Safari bundle ZIP: https://files.catbox.moe/rljaoa.zip

- 122,765,543 bytes
- SHA256 `827a2171beda788881ec092192807c356a593cc0f8fa79ff69ac20df08b266b9`

These are downloadable build artifacts; no hosted gameplay URL was deployed in this task. Read [build and device instructions](multiplatform-build.md) before rebuilding, installing updates or hosting the Web bundle. APK updates require the same private signing identity. Browser saves are per origin and may be evicted; first-load asset memory/download size remains substantial. Hardware touch/audio/GL behavior, lifecycle interruptions and sustained FPS still require device inspection.

## Architecture and changed files

The existing renderer/game loop and serializers are reused. Native Android uses NativeActivity, GLES3, private extracted production files and a separate private saves tree. The browser uses Asyncify/requestAnimationFrame, bounded backing resolution, user-triggered startup/audio, CSS-scaled touches and IDBFS. Existing desktop performance/culling/streaming options remain. Release Dev Lab is removed from compilation/menu; its world-layout tests link it only into the test executable. The pinned raylib dependency receives narrow Web contact and Android EGL-context fixes, plus GLESv3 linkage.

- `.github/workflows/platform-builds.yml`
- `.gitignore`
- `CMakeLists.txt`
- `README.md`
- `docs/multiplatform-build.md`
- `docs/multiplatform-validation.md`
- `include/outland/platform/Runtime.hpp`
- `platform/android/AndroidManifest.xml`
- `platform/android/java/org/outland/game/GameActivity.java`
- `platform/web/shell.html`
- `platform/web/storage.js`
- `src/engine/render/Renderer.cpp`
- `src/game/HomeScreen.cpp`
- `src/input/PointerEvents.cpp`
- `src/main.cpp`
- `src/platform/Runtime.cpp`
- `tests/integration/modes/test_play_ui.cpp`
- `tests/platform/browser_fixture.js`
- `tests/platform/test_android_runtime.py`
- `tests/platform/test_browser_boundary.cpp`
- `tests/platform/test_runtime_package.py`
- `tests/platform/test_web_contacts.py`
- `tools/build_android.sh`
- `tools/build_web.sh`
- `tools/package_android.py`
- `tools/patch_raylib_platform.py`
- `tools/stage_runtime_assets.py`
- `tools/test_web_boundary.sh`
- `tools/validate_platform_artifacts.py`
