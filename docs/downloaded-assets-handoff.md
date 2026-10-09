# Downloaded asset integration history

Current character implementation: [production character handoff](production-character-handoff.md).
The production system supersedes the earlier player fallback described by historical work.

The phone pushes landed as `a885de8` (urban pack) and `2f88f71` (character pack).
The working branch now follows `3fae7f0`, preserving the mobile-control changes.

**All 483 new urban GLBs are registered, selectable, placeable and textured.**
Together with the previous 45 Downtown models, **610 imported models** are usable
through the existing Creator/model-backed world flow. The new pack has 667 embedded
images and no missing external texture references. Matching FBX and Blender files
remain source assets; runtime rendering uses the GLB versions.

The older Downtown glTF pack still lacks **26 original PNG filenames across 51
relative paths**. Its geometry loads with default materials, but original textured
appearance cannot be restored from the unrelated urban textures.

## Integration and files

- `include/outland/creator/CreatorAssetRegistry.hpp`: register the three earlier
  Downtown complete buildings and the full generated urban catalogue. Retain
  stable existing IDs and width/depth/height ordering. Add upstream Warehouse as
  `{16,12,6}` in that order. The upstream swaps on existing procedural footprints
  were not adopted because they exchange the defined depth/height axes.
- `include/outland/creator/UrbanAssetCatalog.hpp`: generated entries for all 483
  GLBs, unique IDs derived from relative paths, pack/category tags, and measured
  scene bounds including node transforms. Categories map to Buildings, Parts,
  Roads or Props without adding another editor architecture.
- `tools/gltf_import.py`, `tools/generate_urban_catalog.py`: standard-library GLB
  reader and transformed bounds/catalogue generator. CMake regenerates when
  models change; `--check` verifies the committed generated header is current.
- `include/outland/creator/CreatorTouchUI.hpp`, `src/creator/CreatorTouchUI.cpp`:
  Previous/Next paging, page counts/clamping and an Urban Only filter. All assets
  remain reachable through their existing categories/hotbar.
- `src/creator/CreatorController.cpp`: model-backed complete buildings follow
  imported placement, preserving procedural building handling. Roads/markings and
  flat ground details do not become circular movement blockers.
- `include/outland/assets/ModelCache.hpp`, `src/assets/ModelCache.cpp`: lazy cache
  of at most 64 models, with least-recently-used eviction. Resolve assets beside
  the executable, falling back to the working directory. Normalize exported
  pivots to ground centre. Release each owned material texture once on eviction
  and shutdown, preserving raylib's shared default texture.
- `src/engine/render/Renderer.cpp`: use the bounded cache for existing world and
  preview rendering; clear it before closing the graphics context. Load an
  existing saved `maps/verda_creator.map` at startup in gameplay and DEV builds.
- `src/world/VerdaRegion.cpp`: model-backed trees do not also draw procedural
  trees. Generated procedural nature remains intact.
- `include/outland/creator/CreatorMapIO.hpp`, `src/creator/CreatorMapIO.cpp`,
  `include/outland/world/VerdaRegion.hpp`: expose the existing map loader to normal
  builds through restricted friend access. Saving and editor/controller APIs
  remain DEV-only; the map format is unchanged.
- `CMakeLists.txt`: catalogue generation/validation, cache and runtime loading
  tests, graphics smoke target and strict urban asset completeness check. Assets
  and maps are packaged alongside the executable.
- `tools/audit_downloaded_assets.py`: catalogue, GLB/glTF buffers, embedded images
  and texture-reference audit. `--pack urban --strict-textures` passes; full-pack
  strict validation identifies the earlier Downtown texture gap.
- `tests/integration/creator/test_creator_touch_ui.cpp`: category/page selection,
  urban filter, all 610 model placements and nonblocking roads.
- `tests/integration/assets/test_runtime_assets.cpp`: idempotent saved-model
  loading in DEV and normal builds, including paths with spaces and ampersands.
- `tests/integration/assets/test_model_cache.cpp`: cache hits/LRU, capacity,
  ground pivots, duplicate material texture IDs, default texture preservation and
  cleanup without double frees.
- `tests/integration/assets/smoke_imported_models.cpp`: real graphics loading of
  every model, texture presence for GLBs, normalized pivots and bounded caching.
- This handoff and `docs/mobile-controls-handoff.md`: review/continuation notes.

See the mobile handoff for the preceding input/HUD/combat changes retained here.
The new source pack files from a885de8 were not modified or converted.

## Verification

Repeated GCC 14/C++20/raylib 5.5 builds:

```sh
cmake -S . -B build-dev -DOUTLAND_DEV_TOOLS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-dev -j4
ctest --test-dir build-dev --output-on-failure
cmake -S . -B build-release -DOUTLAND_DEV_TOOLS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j4
ctest --test-dir build-release --output-on-failure
```

**DEV 14/14 passing; non-DEV Release 12/12 passing.** This worker uses
`/tmp/outland-build-dev` and `/tmp/outland-build-release`, configured with
`PKG_CONFIG_PATH=/tmp/outland-raylib-install/lib/pkgconfig`.

Real raylib/OpenGL under Xvfb:

```sh
DISPLAY=:99 /tmp/outland-build-dev/outland_asset_model_smoke assets/verda/urban 483
DISPLAY=:99 /tmp/outland-build-dev/outland_asset_model_smoke assets/verda/creator/downtown 45
```

**Urban: 483/483 loaded with working textures. Downtown: 45/45 loaded, texture
sources still absent.** Both runs verified bounded caching and normalized pivots.
X11 UI smoke verified filtering, selection, textured placement and rendering in
normal gameplay; screenshots are `/workspace/outland-urban-catalogue.png` and
`/workspace/outland-urban-gameplay.png`. No temporary authored scene was saved.

The model-cache test also passed AddressSanitizer + UndefinedBehaviorSanitizer:

```sh
c++ -std=c++20 -g -fsanitize=address,undefined -fno-omit-frame-pointer -UNDEBUG \
  -I include -I /tmp/outland-raylib-install/include \
  tests/integration/assets/test_model_cache.cpp src/assets/ModelCache.cpp \
  -o /tmp/outland-model-cache-sanitized
/tmp/outland-model-cache-sanitized
```

Strict new-pack resource audit passes:

```sh
python tools/audit_downloaded_assets.py --pack urban --strict-textures
python tools/generate_urban_catalog.py --check
```

Full `--strict-textures` remains red for the missing Downtown PNGs, not for the new
urban pack. `git diff --check` passes. Changes remain uncommitted and unpushed.

## Phone use and remaining checks

Build with `OUTLAND_DEV_TOOLS=ON`; open **DEV LAB -> ASSETS -> URBAN ONLY**, choose a
category/page/model, then **PLACE** and **SAVE**. Gameplay loads the saved world
from the working directory's `maps/verda_creator.map`, or from `maps` beside the
executable. Rebuilding packages a saved map with the game. The supplied combined
patch targets the phone's **2f88f71** checkout and includes earlier mobile work.

Physical Android/Termux GPU, multitouch, background/resume and asset memory/LOD
profiling remain device checks. The cache is a model-count cap, not a byte budget;
scenes with more than 64 visible unique models can cause repeated loading. Imported
buildings retain the existing simplified collision proxies; enterable interiors,
interactive doors and the two authored model animations are not implemented by
this import. The original Downtown texture files are still needed to finish that
older pack's appearance.

## Character cargo (2f88f71)

All 80 FBX character bodies now have raylib-compatible GLBs under
`assets/verda/characters/runtime/`. The two supplied first-person arm GLBs are
also registered: 82 entries, 83 embedded images, 108 preserved animation clips.
Original FBX/Blend/textures/manifest remain unchanged. `Character_Female_01`
has no corresponding source PNG and renders with its supplied material colors.
The original TSV repeats IDs for FBX/GLB arm variants; catalogue IDs derive from
relative runtime paths and are unique.

Added files:
- `tools/convert_character_assets.py`: Blender batch import/export; repairs texture
  filenames using shipped PNGs, explicitly attaches PSX diffuse textures. Run
  `blender -b --python tools/convert_character_assets.py` to create missing outputs;
  remove a specific generated GLB first to regenerate it.
- `tools/generate_character_catalog.py`,
  `include/outland/creator/CharacterAssetCatalog.hpp`: all GLBs registered as Props
  with measured footprints and default height 1.85m. Arms are available as static
  preview/placement assets; this is not a first-person weapon rig integration.
- `tools/audit_character_assets.py`: verifies source conversion coverage, finite
  bounds, embedded buffers/images and the one known untextured model.
- `assets/verda/characters/runtime/**/*.glb`: 80 generated runtime body models,
  approximately 31MiB. Do not omit these when transferring the code changes.

Updated registry, CMake, Creator drawer and placement regression tests include the
new pack. The pack button cycles ALL ASSETS -> URBAN ONLY -> CHARACTERS; selecting
CHARACTERS switches to Props. Models use existing preview, hotbar, placement,
map persistence, normal-game drawing and bounded GPU cache. Character placements
are static scenery with existing prop collision; NPC AI and animation playback
are not implemented. The placeholder player is now retired; see the production character handoff.

Real OpenGL/X11 smoke loaded **82/82 character GLBs, 81 textured** and exercised
character filtering/selection/placement. Screenshot: `/workspace/outland-character-preview.png`.
Command:
```sh
DISPLAY=:100 /tmp/outland-build-dev/outland_asset_model_smoke assets/verda/characters 82 --allow-untextured
python tools/audit_character_assets.py
python tools/generate_character_catalog.py --check
```

The combined patch now includes generated character GLBs, is based on `2f88f71`,
and is also provided compressed as `/workspace/outland-controls-and-assets.patch.gz`.
On Termux, with a clean checkout at that checkpoint and the downloaded patch in
Downloads:
```sh
cd ~/Outland
git pull --ff-only
gzip -dc /sdcard/Download/outland-controls-and-assets.patch.gz > /tmp/outland-controls-and-assets.patch
git apply --check /tmp/outland-controls-and-assets.patch
git apply /tmp/outland-controls-and-assets.patch
```
If main advances, run the check against the newer commit before applying. Do not
reset or discard phone-side edits. Rebuild using your existing Termux setup and
verify real Android simultaneous movement/look/actions; no physical Android test
was available here. Next coworker should inspect animation/weapon attachment
alignment before making the uploaded characters the live player or NPCs.

## Production character follow-up

See [production-character-handoff.md](production-character-handoff.md). The old
VerdanCharacter files are removed; shared manifest-driven bodies serve the player,
NpcSpawn actors and Creator previews. Current tests: DEV 16/16, Release 14/14.
The production patch is based on 3fae7f0. Earlier patch/download commands above
are historical and should not be used for the new production integration.
