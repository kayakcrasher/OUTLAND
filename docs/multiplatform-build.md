# OUTLAND: Android APK and iPhone Safari

The C++20 game, raylib 6 renderer, world, combat, BR, Explore, Zombie, vehicles, NPCs, loot and save serializers are shared. `src/platform/Runtime.cpp` supplies filesystem setup, browser pacing and storage synchronization. These are production builds: Creator/Dev Lab are excluded, including the release menu entry. Desktop DEV remains available with `OUTLAND_DEV_TOOLS=ON`.

## Android build (Linux x86_64 / WSL2)

Install Java 21, CMake >=3.25, Python 3, Git, unzip and Android command-line tools. Set `ANDROID_SDK_ROOT` to the SDK directory, then:

```sh
sdkmanager 'platforms;android-35' 'build-tools;35.0.0' 'ndk;28.2.13676358'
tools/build_android.sh
python3 tools/validate_platform_artifacts.py --apk build/android-arm64/OUTLAND.apk
```

The script cross-compiles the shared game with the NDK/CMake, compiles the small NativeActivity window wrapper with javac/D8, packages with aapt2, packages Unicode asset names as UTF-8 ZIP entries, aligns native libraries for 16 KiB devices, and signs an installable `build/android-arm64/OUTLAND.apk`. No Gradle project or second gameplay implementation is needed. Targets ARM64 Android 8+ and OpenGL ES 3, SDK 35; tested compilation with NDK r28c. Production assets are staged after generators complete. The APK includes maps, models, textures, manifests, fonts and audio, excluding source archives, Blender and FBX editor files.

Default signing is **local installation signing**, not a Play Store release identity. Keep `build/android-arm64/outland-local.keystore` privately: subsequent builds use it so APK updates retain app data. For your own release signing:

```sh
OUTLAND_KEYSTORE=/private/outland.keystore OUTLAND_KEY_ALIAS=outland \
OUTLAND_SIGNING_PASSWORD='your-password' tools/build_android.sh
```

The script does not create a missing explicit production keystore. Never commit keys/passwords. CI generates an ephemeral local key per run, so CI APKs from different runs cannot upgrade one another without a consistent signing key. Configure a persistent signing identity before distributing updates.

Install through Samsung Files (allow installation from that source), or `adb install -r build/android-arm64/OUTLAND.apk`. No Termux/X11 server or DISPLAY variable is needed for the native app. The Activity requests sensor landscape, hides navigation/status bars, and keeps the screen awake. Android system gesture edges still belong to Android.

The first run extracts its versioned production package into private `files/runtime`, while showing a loading message and pumping lifecycle events. C++ streams and raylib then use the same real files. Saves are in private `files/saves/maps/...`; they survive package-content updates and APK upgrades signed with the same key. Uninstalling removes private data. Existing Termux saves are not automatically imported into the APK sandbox. Budget additional storage for the extracted package.

### Termux

Google's Linux NDK and aapt2 host binaries are x86_64; they cannot run directly in ARM64 Termux. Do not downgrade raylib or pretend those binaries are native phone tools. From Termux, push the source and use GitHub **Actions → Android and Safari builds → Run workflow**, download the Android artifact and open the APK in Samsung Files. Or use `ssh` from Termux to a Linux build machine and run the command above there. The existing `tools/run_termux.sh` desktop/X11 development flow remains unchanged. SDK license acceptance and Android tooling installation happen on the build host.

## Web / iPhone Safari

Install and activate the pinned toolchain:

```sh
git clone https://github.com/emscripten-core/emsdk.git
./emsdk/emsdk install 4.0.20
./emsdk/emsdk activate 4.0.20
. ./emsdk/emsdk_env.sh
tools/build_web.sh
python3 tools/validate_platform_artifacts.py --web build/web/site
python3 -m http.server 8080 --directory build/web/site
```

Open `http://localhost:8080/outland.html` for development. Deploy **all** `build/web/site` files together to static HTTPS hosting: `outland.html`, `outland.js`, `outland.wasm`, `outland.data`, and the package report. `outland.data` is the production preload, about 140 MiB on this source revision before HTTP compression. Do not open the HTML with `file://`. Configure `.wasm` as `application/wasm`, gzip/Brotli transport for JS/WASM/data, and normal cache invalidation so a new JS/WASM pair is never served with an old data file. No cross-origin-isolation headers or SharedArrayBuffer are required; the Web target is single-threaded. Hosting deployment is deliberately not automatic. For GitHub Pages, set repository **Settings → Pages → Source → GitHub Actions**, then run **Android and Safari builds** with **publish_web** checked. The optional deployment job publishes only after that explicit workflow selection; ordinary pushes only build artifacts. Open `https://kayakcrasher.github.io/OUTLAND/outland.html` after a successful Pages deployment. This URL is an expected deployment destination, not a claim that this task published it.

Safari: open the hosted URL, rotate to landscape, wait for the asset download, then tap **PLAY**. The gate ensures user activation precedes startup; later canvas touches also resume suspended audio. Use the same dual sticks/action buttons as Android. Desktop mouse/keyboard remain available. The canvas uses CSS-sized touch coordinates, avoids scrolling/zoom gestures and caps backing resolution at 1280×720 before the existing dynamic 3D resolution scaling. Pointer ownership follows identifiers, including release/cancel and touch-array reordering.

Safari may refuse the Full screen button; landscape browser mode remains usable, and **Add to Home Screen** hides more browser chrome. Browser restrictions are reported without blocking play. Private browsing or disabled storage may prevent durable saves: a visible warning explains when a session uses temporary saves.

The existing game loop yields once per frame using `requestAnimationFrame` and Asyncify, preserving its local session state. It never blocks the browser with frame-rate busy waits. `ASYNCIFY_IGNORE_INDIRECT` is safe because suspension is only in the direct `main → Renderer::run → platform::begin_frame` chain; do not add a yielding function inside callbacks without revisiting this setting. Narrow pinned-raylib patches compact active Web touches (including cancel/multiple releases) and request an actual ES3 Android context; upstream otherwise requests version 2 even when selecting ES3 graphics. Both adapters have compiled compatibility tests.

IndexedDB is mounted at `/saves` and restored before PLAY becomes available. Auto-persistence plus explicit sync preserve the existing file-based serializers. Gameplay checkpoints run every 30 seconds and on the next frame after backgrounding, except BR's existing nonpersistent session rules. Vehicles retain their existing periodic saves. **Return Home before closing** for a completed session save. An OS-killed process/tab cannot guarantee a final asynchronous write. Saves belong to the hosting origin and can be removed by browser storage clearing/eviction; different origins do not share saves. There is no cloud account/save synchronization.

## Reproducibility and inspection

Both cross targets fetch immutable raylib 6.0 commit `dbc56a87da87d973a9c5baa4e7438a9d20121d28`. Set `OUTLAND_RAYLIB_SOURCE=/path/to/raylib` to build offline from a matching checkout; Web applies the documented touch patch to that checkout. `OUTLAND_JOBS`, `OUTLAND_ANDROID_BUILD` and `OUTLAND_WEB_BUILD` customize resource use/output paths. Cross builds require `OUTLAND_BUILD_TESTS=OFF` and `OUTLAND_DEV_TOOLS=OFF`; run shared headless tests in desktop configurations, not as device executables on the build host.

Device inspection after installing/hosting:

1. Confirm landscape layout and select BR, Explore and Zombie. Verify there is no Creator/Dev Lab entry in these releases.
2. Move and look simultaneously; press Fire/Aim/Interact with another finger. Release either stick while the other remains held, cancel a gesture, background and return, and check controls recover.
3. In Explore, collect/use/drop loot, buy supplies, drive and exit a hatchback, and visit coastal settlements. Check weapon audio starts after interaction and does not multiply after backgrounding.
4. Return Home, close/reopen, and verify persistent inventory/vehicle state. Reinstall an APK with the same signature to verify save retention. On Safari repeat on the same HTTPS origin.
5. Check FPS/frame-time HUD and compare sustained play/thermal behavior on actual Samsung/iPhone hardware. A successful compile/package cannot establish device FPS, GPU skinning support, browser memory headroom or audio quality.

The large asset preload is a first-production limitation on Safari; all assets remain available rather than quietly dropping gameplay. A future measured pass can stream asset packs instead of keeping the entire preload in MEMFS. No game launch is part of the automated build validation.
