# Walking and DEV map builder

Based on main `ec612af` (including the newly pushed Survival PSX and Radiation Workers source packs). This change extends the existing character renderer and Creator registry/controller/map pipeline. No game was launched and existing character GLBs were preserved; two new Radiation Worker rows/models were added.

## Build the map on the phone

Build with `OUTLAND_DEV_TOOLS=ON`, then choose **MAP BUILDER (DEV)** on Home. The asset browser opens immediately.

1. Tap **ALL TYPES** to browse the entire library, or select Buildings, Parts, Nature, Roads, Props, Vehicles, or Gameplay. The pack filter narrows results to Urban, Characters, or Survival. **SEARCH** accepts an X11 keyboard or the touch keyboard; tap DONE when finished. Names wrap on the cards; Previous/Next reaches every page. All 656 imported GLB/glTF models (including 44 newly converted Survival props and two Radiation Workers) and all 45 starter OBJ/LOD meshes are accessible, with existing asset IDs retained. Raw FBX/Blender archives are authoring sources, not additional runtime models.
2. Choose an asset. Its live 3D placement preview appears in the world and the toolbar names it. Left/right sticks move/look. **FLY ON** enables true free flight and collision bypass at 12 m/s; hold **UP/DOWN** to change elevation. FLY OFF restores ground movement. Desktop Space/Ctrl also rise/descend. Touch ownership stays with the contact until release; opening the browser cancels vertical flight input.
3. **NEAR/FAR** adjusts placement reach from 2 to 80 m. **GROUND** toggles terrain placement versus full pitched-camera placement. **LOWER/RAISE** changes the placement height in 0.25 m steps, allowing stacked model components. **GRID** snaps X/Z to a 1 m grid. **ROTATE** turns the preview 15 degrees when no object is selected. Tap **PLACE** to add it.
4. Aim at an existing object and tap **SELECT**. Selection stays on that object while you look at a new destination: **MOVE** moves it to the preview, **ROTATE** turns it, **DUP** copies it, and **DELETE** removes it. Native generated roads now support move/rotate/duplicate as well as deletion; model-backed road pieces use the existing asset path. **UNDO/REDO** retains up to 16 edits, and works for generated-world content too. Picking an asset/hotbar slot, placing, undoing, or loading clears stale selection.
5. **SAVE** writes immediately and reports success/failure on screen. Edits also autosave after one quiet second. Returning to Home flushes pending edits; a save failure keeps you in the builder. Normal shutdown attempts a final save. **LOAD** reloads the durable map, first flushing pending edits. Existing map contents also load when playing outside DEV.

On Termux, the saved map is normally:

```text
~/.local/share/outland/maps/verda_creator.map
```

This lives outside build output, so rebuilding/pulling does not overwrite it. `OUTLAND_SAVE_DIR` can select another user storage root. Native builds without HOME use the application's data directory. At startup, a user map takes precedence; otherwise existing working-directory/packaged `maps/verda_creator.map` files provide the initial world.

To share an authored phone map through Git later (optional):

```sh
cd ~/Outland
cp ~/.local/share/outland/maps/verda_creator.map maps/verda_creator.map
git add maps/verda_creator.map
git commit -m "Save authored Verda map"
git push origin main
```

This shares data created in the editor; it does not require coding the map.

## Persistence and editing architecture

`CreatorSession` owns bounded whole-world undo/redo, dirty status, debounced autosave, explicit save/load, and failure feedback. All button/key world mutations route through it; preview/settings changes do not create history entries. Creator stays DEV-only. Gameplay input, weapons/inventory, AI, and marker pool IDs retain their existing paths. NPC AI pauses in the builder as before.

`CreatorMapIO` now writes **V4 full-world snapshots**: settlement IDs/names/centers/state/population, all buildings/assets/markers, and native roads. This fixes the previous behavior where only `creator_` additions were saved: edits/deletions of generated content now survive restart. Parsing commits only after the whole file succeeds. Existing V3 addition maps still load through their existing merge path. V4 is a new format; binaries from before this patch cannot read it.

The writer preserves float precision, writes a temporary file, then renames it. Failed renames keep the previous good destination intact. Saves are local user data, not automatically committed to Git. Undo/redo history and camera/editor settings remain session-only.

`CreatorTouchUI` retains edge-based stable Android contact IDs and suppresses synthesized mouse input. Browser/search is modal. Editing controls and text scale together from a 720-high logical canvas; hit testing uses the same scale, tested at 960x540, 1280x720, and 1920x1080. Native contact order/dragging cannot retarget held Up/Down. Numeric teleport and preview shortcuts are disabled while browsing/searching.

`generate_starter_catalog.py` adds the previously unreachable starter models/LODs using measured OBJ footprints. Existing registered starter paths/IDs are reused. The former Rock and Wood Fence placeholder palette entries now reuse real catalog geometry with their legacy IDs intact. Urban and character catalogs still come from their existing generators/manifest. No filenames were dumped into Renderer.

The new Survival FBX scene is converted offline into 44 individual GLBs, including camping gear, medical items, containers, tools, weapons and separate assembly parts. All 43 textured objects retain embedded base-color images; the Glass object keeps its source transparent material. The files contain 44 embedded images in total. Missing Windows texture paths were resolved to the supplied texture folder. One auxiliary source image (`Mochila.png.002`) is absent; it was not invented, and exported backpack base-color textures are present. Runtime/source hashes and embedded images are checked by the generated Survival catalog. These are placeable props; placing a rifle/medical item does not add new inventory or weapon mechanics.

`convert_survival_assets.py` runs in offline Blender; committed GLBs work on Termux without Blender. Reconvert with `blender --background --python tools/convert_survival_assets.py`, then run `python tools/generate_survival_catalog.py`.

## Walking animation

The production asset audit finds 84 models, 108 embedded clips, 79 constant unnamed short PSX body clips, and no rebel or Radiation Worker clips. The new heavy/light workers have 57/41 joints and supplied embedded textures, and enter the existing emergency pool through TSV metadata. Their ASCII FBX sources are converted through Assimp and Blender offline; neither tool is required on the phone. Their rigs retain their own bind transforms, and do not share clips with the original PSX bodies. This patch adds an **explicitly authored procedural skeletal fallback**, not fake embedded animations or a second character system.

`SkeletalGait` uses each model's own global bind transforms, bone hierarchy, and Mixamo joint names. It rotates opposing thighs, bends knees, swings lowered arms, and propagates transforms through descendants. Walk and Run use distinct strides/cadences; Idle keeps a relaxed arm pose. Up/forward axes follow the existing Z-up/Y-up normalization and facing offset. Bind transforms are never overwritten. Unsupported/malformed rigs fall back safely.

`CharacterRenderer` selects real compatible clips first and uses this gait for missing Idle/Walk/Run on recognized body rigs. Shared model pose state still restores bind pose for missing Attack/Death. The player switches to walking based on actual movement rather than merely pushing a stick against a wall. NPC Wander/Chase/Flee use their existing animation states.

This is a basic gait, with no motion capture, retargeting, blending, foot IK/locking, authored attack/death clips, or ragdolls. It does not alter movement speeds. The numerical/headless checks prove skeleton motion and safety; appearance still needs a phone check. Model components can be stacked visually, but existing collision remains coarse; native roads and procedural buildings retain their terrain-following rendering conventions.

## Validation

- DEV Debug: **24/24 CTest tests passed**.
- Release: **21/21 CTest tests passed**; includes V4 snapshot loading without Creator APIs.
- ASan/UBSan with leak detection: **9/9 focused suites passed** (gait, real GLB decoding, character renderer, animation controller, model cache, NPC AI, Creator touch, Creator session, runtime map loading). Project code is instrumented; installed raylib is not.
- Existing Creator map/marker persistence, asset/catalog completeness, mobile input, combat, and collision tests remain green.
- New coverage includes opposing skeletal steps, cycle/limb-length preservation, malformed/unknown/dead rig fallback, real PSX skeleton poses, all starter mesh access, search, held vertical touch ownership, scaled button coordinates, free/grid/height placement, stable road selection/move/rotate/duplicate, generated-object edits/deletions, undo/redo/autosave, storage path selection, Release full snapshots, and transactional failure handling.
- `git diff --check`, generated starter/Survival/character catalogs, character dependency and rig audit checks pass. No graphics window/game was launched.

Build in Termux using your existing raylib setup:

```sh
cmake -S . -B build/dev -DOUTLAND_DEV_TOOLS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/dev -j2
ctest --test-dir build/dev --output-on-failure
```

Next coworker should inspect new worker orientation/scale and visual gait/foot sliding and stacked-piece collision on the phone, plus saving/loading an authored map from a different launch directory. The older Downtown pack still lacks its original textures; this patch does not invent replacement materials.

## Changed files

| Files | Purpose |
|---|---|
| `CMakeLists.txt` | Gait/session sources, starter/Survival generators/checks, new tests and expanded input wrappers. |
| `include/outland/characters/SkeletalGait.hpp`, `src/characters/SkeletalGait.cpp` | Procedural body gait and safe bind hierarchy propagation. |
| `include/outland/characters/CharacterRenderer.hpp`, `src/characters/CharacterRenderer.cpp` | Use gait through the existing cached character renderer. |
| `include/outland/creator/CreatorSession.hpp`, `src/creator/CreatorSession.cpp` | History, dirty status, autosave and durable save/load feedback. |
| `include/outland/creator/CreatorController.hpp`, `src/creator/CreatorController.cpp` | Modal-safe shortcuts, pitched/grid/height placement, native road edits. |
| `include/outland/creator/CreatorState.hpp` | Placement height and grid settings. |
| `include/outland/creator/CreatorTouchUI.hpp`, `src/creator/CreatorTouchUI.cpp` | Full searchable browser, touch keyboard, builder actions, scaling and held flight controls. |
| `include/outland/creator/CreatorMapIO.hpp`, `src/creator/CreatorMapIO.cpp` | Stable storage path, V4 snapshots, V3 compatibility and safe rename failure. |
| `include/outland/creator/CreatorAssetRegistry.hpp`, `include/outland/creator/StarterAssetCatalog.hpp`, `tools/generate_starter_catalog.py` | Complete starter mesh access while retaining existing IDs. |
| `tools/convert_survival_assets.py`, `tools/generate_survival_catalog.py`, `include/outland/creator/SurvivalAssetCatalog.hpp` | Offline Survival conversion and reusable checked registry. |
| `assets/verda/survival/runtime/*.glb` (44 files), `assets/verda/survival/runtime_manifest.json` | Embedded-texture runtime props, stable source identities and hashes. |
| `src/engine/render/Renderer.cpp` | Free flight, stable selection/action/session wiring, load user maps, actual-movement animation and uncluttered builder HUD. |
| `src/game/HomeScreen.cpp` | Clear DEV map-builder entry. |
| `tools/audit_character_rigs.py`, `docs/character-rig-compatibility.md`, `docs/character-rig-compatibility.json`, `include/outland/characters/AnimationAssetCatalog.hpp` | Distinguish authored gait from embedded clips, audit new worker rigs. |
| `tools/convert_hazmat_characters.py`, `assets/verda/characters/character_manifest.tsv`, `assets/verda/characters/runtime/production_index.json`, `assets/verda/characters/hazmat/runtime_conversion.json`, `assets/verda/characters/runtime/hazmat/**/*.glb` (2 files), `include/outland/creator/CharacterAssetCatalog.hpp` | Textured Radiation Workers through the existing character registry, emergency pool and builder catalog. |
| `tests/integration/assets/test_characters.cpp` | Updated library/pool counts and existing marker regression checks. |
| `tests/integration/assets/test_skeletal_gait.cpp` | Gait geometry and fallback tests. |
| `tests/integration/assets/test_animation_assets.cpp` | Gait samples on real decoded PSX skeletons. |
| `tests/integration/assets/test_runtime_assets.cpp` | Full snapshots and malformed-file protection in DEV/Release. |
| `tests/integration/creator/test_creator_session.cpp` | Editing history, generated content, roads, autosave, path and failure tests. |
| `tests/integration/creator/test_creator_touch_ui.cpp` | Complete asset reachability, search, held contacts, placement/roads and scaled UI. |
| `tests/integration/creator/test_creator_map_io.cpp` | Valid settlement metadata in the existing persistence fixture. |
| `docs/map-builder-handoff.md` | Workflow, architecture, limits, checks, changed-file handoff. |
