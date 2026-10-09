# Creator builder repair — based on e76bae5

## What was broken

DEV entered with building_mode=false. Input disabled Creator before polling the buttons,
then a later renderer block enabled it again unconditionally. This drew a working-looking
builder whose taps did nothing and cleared object selections every frame. DEV tools were
also skipped while the initial asset drawer was open.

DevLab now owns build/inspect mode and starts in BUILD WORLD. Renderer uses one
creator_active value throughout input, world editing and rendering. DEV tools stay
reachable above the asset drawer. Opening/closing tools blocks Creator and gameplay
for that frame, tracking touch IDs so held contacts cannot invoke newly exposed buttons.
Focus loss also blocks Creator actions. Numeric travel shortcuts only run in inspection
mode, leaving builder hotbar keys alone.

Toolbar hit testing and drawing share logical coordinates. Scaling fits both width and
height, including portrait and short landscape windows; the DEV travel panel also fits
short windows. Native contacts retain stable IDs and desktop mouse clicks remain supported.

## Device workflow

1. Build the DEV executable with OUTLAND_DEV_TOOLS=ON, then select MAP BUILDER (DEV).
2. Use ASSETS, pack/type filters and search. Tap an asset card: it selects the asset and
   closes the drawer. CLOSE exits the drawer without choosing another asset.
3. Aim at the desired position and tap PLACE. A blocked/red placement preview prevents
   placement; move the ghost to clear space. NEAR/FAR, LOWER/RAISE, GROUND and GRID
   adjust placement. FLY ON plus movement/look and UP/DOWN lets you survey the map.
4. Aim at an existing object and tap SELECT. MOVE applies the current placement ghost;
   ROTATE turns the selected object, DUP duplicates it, DELETE removes it. With no selected
   object ROTATE changes the placement ghost. UNDO/REDO retain the existing 16-edit history.
5. SAVE (F5) writes the whole map. LOAD restores it. Quiet edits also autosave after one
   second. Close the asset drawer before using world-edit/save controls.
6. EXPORT (F6) saves first, then writes a separate production map. Check the EXPORTED status.
7. DEV TOOLS -> INSPECT / DRIVE hides builder controls and enables normal movement/driving.
   DEV TOOLS -> BUILD WORLD returns to building. Travel and SPAWN HATCHBACK remain available.

Rotate the phone to landscape for larger controls. If Termux:X11 shows only part of a
large desktop, set its resolution/scaling to fit the phone. In trackpad input mode a tap
may move the cursor instead of clicking: use direct touch/click input for the builder.
Device visual inspection is still needed; this change was tested without launching the game.

## Save and publish

Default personal save: $HOME/.local/share/outland/maps/verda_creator.map.
Default export: $HOME/.local/share/outland/exports/verda_world.map.
OUTLAND_SAVE_DIR overrides the root containing maps/ and exports/.
SAVE and EXPORT use the existing atomic V6 full-world serializer, preserving geography,
settlements, buildings, roads, assets, gameplay markers and vehicle state. Existing
V3/V4/V5 readers are unchanged. No new parallel world format or character system.

After building the world in-game and pressing EXPORT, run in Termux:

```sh
cd ~/Outland &&
cp "${OUTLAND_SAVE_DIR:-$HOME/.local/share/outland}/exports/verda_world.map" maps/verda_world.map &&
git add maps/verda_world.map &&
git commit -m "Publish authored Verda world" &&
git push origin main
```

DEV and release load personal edits first, then maps/verda_world.map from the project
working directory or beside the executable, then legacy packaged verda_creator.map.
The build copies project maps on every build, including data-only changes.
Fresh installs therefore receive the published world. Existing personal saves intentionally
continue to override it. To inspect only the published map without deleting your work,
launch using a new OUTLAND_SAVE_DIR such as "$TMPDIR/outland-published-preview".

Publishing the map does not upload new assets automatically. Models already in the project
are referenced by their existing relative paths. Commit any genuinely new runtime assets
separately; do not add downloaded source archives or private save/profile files.

## Install this repair on Termux

Download outland-builder-repair-e76bae5.patch.gz to /sdcard/Download, then run:

```sh
(
set -e
cd ~/Outland
git pull --ff-only
gzip -t /sdcard/Download/outland-builder-repair-e76bae5.patch.gz
gzip -dc /sdcard/Download/outland-builder-repair-e76bae5.patch.gz > "$TMPDIR/outland-builder-repair.patch"
git apply --check "$TMPDIR/outland-builder-repair.patch"
git apply --index "$TMPDIR/outland-builder-repair.patch"
# Use the installed raylib 6 headers AND runtime library, without the old 5.5 override.
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
git commit -m "Repair DEV builder input and add published world export"
git push origin main
)
```

This only applies/builds/tests/pushes; it does not start the game. Afterward use your
existing Termux:X11 launch procedure with DISPLAY=:0, LD_LIBRARY_PATH="$PREFIX/lib"
and LD_PRELOAD=, launching build/dev-raylib6/outland for map construction.

## Files changed

- include/outland/dev/DevLab.hpp and src/dev/DevLab.cpp: shared mode ownership,
  initial builder entry, inspect/build labels, shortcut separation and responsive panel.
- src/engine/render/Renderer.cpp: remove contradictory mode toggle, route modal input,
  handle export and select the published startup map.
- include/outland/creator/CreatorTouchUI.hpp and src/creator/CreatorTouchUI.cpp:
  focus/modal quarantine, EXPORT, responsive coordinates and shared hit rectangles.
- include/outland/creator/CreatorSession.hpp and src/creator/CreatorSession.cpp:
  save-first atomic export and actionable success/failure statuses.
- include/outland/creator/CreatorMapIO.hpp and src/creator/CreatorMapIO.cpp:
  explicit startup precedence for personal/published/legacy maps.
- CMakeLists.txt: refresh all packaged maps for data-only builds; input mock linkage.
- tests/integration/creator/test_creator_touch_ui.cpp: default builder, inspect isolation,
  drawer/DEV tools routing, placement and persistent selection, export clicks,
  held-contact quarantine and portrait/letterboxed/desktop viewport coverage.
- tests/integration/creator/test_creator_session.cpp: export/reload, startup priority,
  personal-save protection and export destination failure.
- tests/integration/vehicles/test_dev_travel.cpp: default/reset mode, hotbar shortcut
  separation and short-window CLOSE access.
- docs/creator-builder-repair-handoff.md: this workflow and verification report.

## Verification

Using upstream raylib 6.0: DEV build and 34/34 tests; release build and 29/29 tests;
AddressSanitizer + UndefinedBehaviorSanitizer DEV build and 34/34 tests. No game launch.
No new rig/animation changes or save-version migration. Next coworker should inspect
actual Termux:X11 direct taps, multi-finger move/look plus toolbar taps, building placement
and existing-object edits, SAVE/restart, and a clean-profile release load of an exported map.
