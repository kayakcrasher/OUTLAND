# OUTLAND creative builder — follow-up to 75f9a4a

## Gameplay/editor flow

Select CREATIVE BUILDER (DEV). It starts with free flight/noclip and a one-metre grid.
The asset browser contains the existing unlimited asset catalog: urban, industrial,
survival, characters, vehicles and procedural buildings/parts/roads/gameplay markers.
No items are consumed while building.

- ASSETS [B] at the top centre is always reachable. B, Tab or I also opens/closes it.
  The original right-hand ASSETS/CLOSE button remains supported.
- Browse existing category/pack filters and pages, or use SEARCH. Choosing a card assigns
  that model to the current hotbar slot, closes the browser and enters PLACE mode.
- Hotbar taps or keys 1–8 choose an asset. Builder hotbar keys do not teleport the player.
- In PLACE mode, tap a visible existing building/model/road/marker to select it. Tap clear
  ground to place the current hotbar model there. Aiming at the sky will not place it on
  imaginary ground. Ground taps target terrain within 120 metres.
- SELECT plus a direct tap lets you select without placing. The original SELECT and PLACE
  buttons still support aiming at the central crosshair.
- Select an object, press MOVE, then tap its destination. Entering MOVE does not move it
  immediately. ROTATE, DUP and DELETE continue to edit the selected whole object.
- GRID toggles metre snapping. GROUND toggles terrain/free placement; NEAR/FAR and
  LOWER/RAISE remain available. UP/DOWN plus the existing movement/look sticks fly.
- UNDO/REDO retain the existing session history. SAVE/F5 and quiet autosave preserve the
  world. EXPORT/F6 writes the same production snapshot as the preceding repair.
- DEV TOOLS -> INSPECT / DRIVE restores navigation/driving; BUILD WORLD returns to editing.

Navigation fingers, browser taps, tool taps, modal transitions, resizing, and focus loss
cannot become placement taps. A contact must start in the world editing area. Holding
it does not place repeatedly. There is no new character, asset, inventory or map system.

## Why the extra input work was necessary

The preceding patch corrected contradictory build/inspect ownership, but selection still
used only a ray through the camera centre. A user's tap on a visible building was not
used for picking. This follow-up uses GetScreenToWorldRayEx with the actual tap coordinate.

Raylib's GLFW desktop backend stores button states rather than a press-event queue.
A short Termux:X11 mouse press followed by release within the same event poll can leave
both states up; IsMouseButtonPressed then reports false. PointerEvents optionally attaches
to the current external GLFW context and retains a bounded eight-press queue. It chains
raylib's original callback for every event and restores it on teardown. UI consumes at most
one buffered click per frame. No additional GLFW library or physics dependency is loaded.
Native touch retains stable contact IDs. When external GLFW symbols are unavailable,
existing native/raylib polling remains supported. B/Tab, hotbar and save/export shortcuts
also read raylib's existing bounded key queue, handling short software-keyboard key presses.

Startup logs show either "Desktop click capture enabled" or the polling fallback.
Actual phone/X11 inspection remains necessary; tests do not replace that check. Direct
X11 clicks must reach the game window and it must have focus. If trackpad mode only moves
its pointer, switch Termux:X11 to direct touch/click input. Landscape gives larger controls.

The hardcoded training cube/target display is hidden while building because it is not
editable/persisted map data. It remains available outside the builder. An editor crosshair
is now drawn when the browser and DEV tools are closed.

## Save/publish (unchanged V6 format)

SAVE: ${OUTLAND_SAVE_DIR:-$HOME/.local/share/outland}/maps/verda_creator.map.
EXPORT: ${OUTLAND_SAVE_DIR:-$HOME/.local/share/outland}/exports/verda_world.map.
After pressing EXPORT in-game:

```sh
cd ~/Outland &&
cp "${OUTLAND_SAVE_DIR:-$HOME/.local/share/outland}/exports/verda_world.map" maps/verda_world.map &&
git add maps/verda_world.map &&
git commit -m "Publish authored Verda world" &&
git push origin main
```

Fresh DEV/release installs load the published map. Existing personal saves retain priority.
No save migration or deletion is performed. Buildings, roads, markers, assets, settlement
identities, vehicle state and the coastal layout retain their existing persistence paths.

## Install on Termux

This patch is based on 75f9a4a. Download outland-creative-builder-75f9a4a.patch.gz to
/sdcard/Download. Do not reapply the previous e76bae5 repair.

```sh
(
set -e
cd ~/Outland
git pull --ff-only
gzip -t /sdcard/Download/outland-creative-builder-75f9a4a.patch.gz
gzip -dc /sdcard/Download/outland-creative-builder-75f9a4a.patch.gz > "$TMPDIR/outland-creative-builder.patch"
git apply --check "$TMPDIR/outland-creative-builder.patch"
git apply --index "$TMPDIR/outland-creative-builder.patch"
export LD_LIBRARY_PATH="$PREFIX/lib"
export LD_PRELOAD=
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$PREFIX/share/pkgconfig"
cmake -S . -B build/dev-raylib6 -DOUTLAND_DEV_TOOLS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/dev-raylib6 -j2
ctest --test-dir build/dev-raylib6 --output-on-failure
cmake -S . -B build/release-raylib6 -DOUTLAND_DEV_TOOLS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/release-raylib6 -j2
ctest --test-dir build/release-raylib6 --output-on-failure
git commit -m "Add creative world taps and reliable Termux builder clicks"
git push origin main
)
```

The install commands do not launch the game. Afterward use the running Termux:X11 session:

```sh
cd ~/Outland
DISPLAY=:0 LD_LIBRARY_PATH="$PREFIX/lib" LD_PRELOAD= ./build/dev-raylib6/outland
```

For device inspection: verify the click-capture startup message, open ASSETS with a tap
and with B, choose a model, place it on clear ground, tap it to select, MOVE it, undo/redo,
SAVE and restart, then EXPORT and inspect a published map with a fresh save directory.
Also test a rotated existing building, simultaneous move/look plus toolbar taps, and
DEV tools open/close without accidental placement. If clicks still fail, report the startup
capture message and whether a brief held click or B works.

## Architecture and files

- include/outland/input/PointerEvents.hpp, src/input/PointerEvents.cpp: optional callback
  capture, bounded frame snapshot, chaining and teardown. No mesh/model work in callbacks.
- include/outland/input/TouchLayout.hpp: shared DEV/Creator canvas scaling to prevent
  the ASSETS button overlapping DEV TOOLS in short/portrait X11 windows.
- include/outland/input/InputSystem.hpp, src/input/InputSystem.cpp: reuse the current stick
  geometry for navigation ownership, including custom layouts.
- include/outland/creator/CreatorController.hpp, src/creator/CreatorController.cpp: bounded
  terrain-point targeting with snapping/free-placement and shared collision validation.
- include/outland/creator/CreatorTouchUI.hpp, src/creator/CreatorTouchUI.cpp: central browser
  access, queued keyboard shortcuts, hotbar keys, world taps, picking/tool resolution,
  ownership and editing hints. Existing asset registry/filter/search code is retained.
- src/dev/DevLab.cpp: buffered desktop clicks through the existing DEV tool routing.
- src/game/HomeScreen.cpp: creative label and buffered menu clicks.
- src/engine/render/Renderer.cpp: input buffer lifecycle, default grid, ray dispatch into
  Creator, navigation/audio reservation, crosshair and training-only display isolation.
  The existing CreatorSession performs every actual world edit/save.
- CMakeLists.txt: input source/system dl linkage and headless creative integration test.
- tests/integration/creator/test_creative_builder.cpp: reproduce a complete press/release
  between polls; catalog/card selection, actual screen rays, rotated building/model selection,
  placement, move/undo/redo, native/navigation/modal/focus ownership, queued B, hotbar,
  SAVE/export/reload, invalid rays, free placement, bounded buffering, callback restoration
  and unavailable-backend fallback.
- tests/integration/vehicles/test_dev_travel.cpp: shared-canvas DEV button position in
  a short window.
- docs/creative-builder-handoff.md: this handoff.

Verification: raylib 6 DEV build and 35/35 tests; release build and 29/29 tests; ASan+UBSan
DEV build and 35/35 tests. No game launch. Existing combat/loot/NPC/animation/vehicle/world
and Creator tests remain green. Device rendering/input verification is the next check.
