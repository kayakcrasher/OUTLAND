# Production character integration

Base: `3fae7f0`, the phone's pushed Creator checkpoint. Work remains local on
`codex/mobile-controls`; no GitHub push was made. Earlier mobile/urban integration
is retained in this working tree and in the combined transfer patch.

## Result and architecture

`assets/verda/characters/character_manifest.tsv` is the authority for model IDs,
names, categories, roles, source files and explicit diffuse texture paths.
84 TSV rows coalesce to 82 assets: 80 body characters and 2 arm rigs. Runtime
parsing and offline conversion both reject conflicting IDs and unsafe paths;
FBX/GLB duplicates of the same arm rig prefer the supplied GLB. The original TSV,
source FBX/Blend/textures, map files and marker schema are unchanged.

Body pools: civilian 45, emergency 20 (police/firefighter/doctor subcategories
retained), hostile 9, creature 6. Arm rigs never enter player/NPC body pools.
The player uses the first civilian in stable ID order, currently Character_01.
No model filenames are enumerated in Renderer.cpp.

CharacterRegistry is metadata-only. CharacterRenderer is shared by player/NPC
bodies; Creator previews and persisted character props use its same preparation
function. FBX bodies are converted offline to embedded-texture GLBs because
raylib does not load FBX. Source geometry orientation is normalized using the
pack's source metadata: PSX Z-up/+X-facing, rebel Y-up/+Z-facing. Bodies are
1.85m tall, centred horizontally, and grounded at their feet. A rotation-safe
AABB helper handles raylib 5.5's two-corner bounding-box limitation.

GPU uploads are lazy and bounded to 64 body models. Preparation runs once per
actual load, including reloads after eviction. Missing/invalid body loads log
once per actor definition and do not draw a substitute. Material textures are
released once per unique GPU ID; default raylib textures survive. Actor renderer
resources die when Renderer::run exits, before the window/context closes.

The old `VerdanCharacter.hpp/.cpp` procedural implementation is deleted and
removed from CMake. There is no capsule/cube player body and no fallback body.
Player movement, camera, weapons and input still use their existing systems.

## NPC marker behavior and save compatibility

NpcSystem derives runtime actors from enabled `NpcSpawn` markers each update.
A stable settlement/marker key selects a model deterministically from its pool.
Reloading the same map does not duplicate actors or shuffle models. Moving or
rotating markers moves/rotates their actor; disabling/deleting them removes it.
Duplicate IDs within one settlement are suppressed; equal IDs in different
settlements remain distinct. Non-finite positions/rotations are ignored.

Existing `creator_marker_npc_spawn_N` IDs select civilian characters. Creator's
Gameplay category now also has Emergency NPC Spawn, Hostile NPC Spawn and
Creature NPC Spawn. These generate `creator_marker_npc_spawn_emergency_N`,
`..._hostile_N`, and `..._creature_N` IDs. They remain ordinary type-2 NpcSpawn
records in the unchanged V3 format. Old V1/V2/V3 readers/tests still pass.
No runtime actor records or generated state are written into current saves.

All characters within 160m of the camera are rendered in normal and DEV modes.
NPCs are standing visual actors, not a new AI/health/combat/pathfinding system.
Hostile/creature labels select appearance pools; they do not imply aggression.
ZombieSpawn/vehicle/loot and existing combat entities keep their existing flow.

## Files for this character work

- `include/outland/characters/CharacterRegistry.hpp`, `src/characters/CharacterRegistry.cpp`:
  transactional TSV loader, body pools, stable selection, runtime path resolution.
- `include/outland/characters/CharacterRenderer.hpp`, `src/characters/CharacterRenderer.cpp`:
  shared player/NPC loading/drawing, source profiles, normalized dimensions, failure handling.
- `include/outland/characters/NpcSystem.hpp`, `src/characters/NpcSystem.cpp`:
  marker reconciliation and transient runtime actor data.
- `include/outland/assets/ModelCache.hpp`, `src/assets/ModelCache.cpp`:
  per-load preparation callback, rotation-safe bounds and GPU ownership/cache tests.
- `src/engine/render/Renderer.cpp`: load registry from packaged/cwd assets, reconcile
  NPCs after world edits, draw NPC/player bodies, use shared Creator preparation;
  saved model-backed world drawing also runs outside DEV.
- `include/outland/creator/CreatorAssetRegistry.hpp`, `src/creator/CreatorController.cpp`:
  three NPC pool variants use the existing marker placement/persistence path.
- `include/outland/creator/CharacterAssetCatalog.hpp`: generated Creator entries
  from TSV metadata with correct body footprints and stable existing asset IDs.
- `tools/character_manifest.py`: canonical offline TSV interpretation.
- `tools/convert_character_assets.py`: manifest-selected Blender conversion,
  explicit PSX diffuse images, repair rebel artist-machine texture paths, cache inputs.
- `tools/generate_character_catalog.py`: manifest-driven editor catalogue generator.
- `tools/audit_character_assets.py`: headless source/runtime hashes, buffers,
  embedded images, finite bounds, joint references and paired weight attributes.
- `assets/verda/characters/runtime/**/*.glb`: 80 generated body models, about 31MiB;
  supplied arm GLBs remain at their original paths.
- `assets/verda/characters/runtime/production_index.json`: source/texture/runtime
  SHA-256 index for reproducibility and stale-output detection, including the
  rebel textures referenced inside FBX even when the TSV texture field is empty.
- `CMakeLists.txt`: compile character modules, drop old body, regenerate catalogue
  on manifest changes, add headless character tests.
- `tests/integration/assets/test_characters.cpp`: real manifest/pools, bad input,
  unchanged V3 loading, stable spawn identities, edit/remove/disable behavior,
  multi-settlement identities and invalid coordinates.
- `tests/integration/assets/test_character_renderer.cpp`: GPU mocks exercise real
  preparation/cache/draw path, PSX/rebel scale/facing, invalid loads, eviction and cleanup.
- `tests/integration/creator/test_creator_touch_ui.cpp`: place all imported models,
  touch/category filtering and all four NPC marker types through CreatorController.
- Deleted: `include/outland/player/VerdanCharacter.hpp`, `src/player/VerdanCharacter.cpp`.
- Documentation: this handoff, downloaded-assets handoff updates, full changed-file
  inventory `docs/production-character-files.txt`.

The full inventory includes carried-forward mobile/urban work. Its explanations
are in `docs/mobile-controls-handoff.md` and `docs/downloaded-assets-handoff.md`.
Latest checkpoint Warehouse rendering and starter assets were preserved while
carrying forward the tested unified InputSystem mobile controls.

## Verification (no game launch)

GCC 14 / C++20 / raylib 5.5; CMake builds with OUTLAND_DEV_TOOLS ON (Debug) and
OFF (Release). **DEV 16/16 CTests pass; Release 14/14 pass.** Tests cover input,
weapons, world collision, Creator marker/save compatibility, lazy model resources,
manifest integrity, model geometry/texture dependencies and NPC reconciliation.
The headless character renderer also passes AddressSanitizer + UndefinedBehaviorSanitizer.
A second Blender conversion skipped all unchanged outputs; index/hash/catalogue
checks remained green. Blender background conversion is not a game launch.
No game executable, graphics smoke, Xvfb, or physical Android run was launched
for this task; GPU behavior is tested with mocks. Real-device validation remains.

```sh
cmake -S . -B build-dev -DOUTLAND_DEV_TOOLS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-dev -j4
ctest --test-dir build-dev --output-on-failure
cmake -S . -B build-release -DOUTLAND_DEV_TOOLS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j4
ctest --test-dir build-release --output-on-failure
blender -b --python tools/convert_character_assets.py
python tools/audit_character_assets.py
python tools/generate_character_catalog.py --check
```

Worker build directories: `/tmp/outland-build-dev`, `/tmp/outland-build-release`;
raylib prefix `/tmp/outland-raylib-install` via PKG_CONFIG_PATH. No NDK/APK build.

## Rig/animation limitations for the next coworker

- 82 runtime GLBs have single skins, 30–65 joints; 83 embedded images total.
- The 79 PSX body clips are ~0.067s static exports, not walk/run/idle cycles.
  Rebel has no clips. The 2 arm rigs carry 29 more clips (108 total library clips).
  Animations are preserved in GLBs but not played by this body renderer.
- Blender reports >4 joint influences in 47 exports; the highest four were
  retained/renormalized for the glTF/raylib path. Inspect deformation when usable
  animation clips are added. Do not assume cross-rig animation compatibility.
- `character_female_01` has no diffuse image in the source/manifest; supplied
  material colors render without inventing or borrowing a skin.
- No weapon-grip, crouch or locomotion body posing is supplied. Camera crouch,
  movement and the existing weapon renderer still function independently.
  Next work: author compatible clips, rig attachments and NPC AI/combat/collision.
- Existing Downtown textures remain missing (26 original filenames/51 paths);
  this task does not fabricate replacements or change that older pack.
