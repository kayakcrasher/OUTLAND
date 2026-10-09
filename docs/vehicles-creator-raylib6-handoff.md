# Vehicles, DEV building and raylib 6 handoff

Base: GitHub main `78338d62ff5b878fdb6e582cce63d30c4a447a0f`.
No game window was opened or game launched. This handoff supersedes historical
raylib 5.5 build instructions in earlier handoff documents.

## Architecture and definitions

`VehicleRegistry` reads `assets/verda/vehicles/vehicle_manifest.tsv`. Definitions
carry body/wheel paths, physical dimensions, four derived wheel anchors/radius,
seat/exit offsets, acceleration/braking/reverse/resistance/top speed, steering,
health tuning, a fuel hook, chase-camera tuning and maximum terrain slope.
Registry loading validates paths/numeric limits transactionally. Rendered body
length is normalized from transformed mesh bounds; wheel diameter is normalized
separately, with a centered rotation pivot.

**One production definition is present: `hatchback` / Classic hatchback.** Its
length is 4.2 m, steering limit 30 degrees, nominal top speed 28 m/s (100.8 km/h),
reverse limit 7 m/s, and slope limit 28 degrees. These are initial game tuning,
not a claim about a particular real-world car.

Current main contains the hatchback GLB and GGBot OGG sounds, **but zero GGBot
body/wheel GLBs**. Those cannot be registered as real renderable bodies yet.
`prepare_vehicle_assets.py` discovers GGBot GLBs when they arrive, generates
initial approximate definitions and Creator entries, and preserves existing TSV
tuning and manually authored variants. Verify each new body's orientation,
normalization and anchors before treating it as production-ready. No fake paths,
placeholder substitutions or invented models were added.

The original hatchback GLB/license remain untouched. Small derived GLBs select
its real body mesh and one real wheel mesh; a normalized full-car preview keeps
all wheels visible in Creator. The body/wheel derivation removes unused mesh
records because raylib loads all meshes, regardless of scene-node selection.
The embedded source buffer/material data stays intact. Regeneration/checking is
wired into CMake and CTest.

`VehicleSystem` extends the existing **WorldAsset** ownership and
**VehicleSpawn** markers. There is no parallel placed-vehicle map or character
system. Runtime motion references the same assets that Creator selects, edits
and saves. Marker-derived cars track their source marker; moving/rotating the
car updates that marker, deleting it disables its marker, and duplication
creates an independent placed car. Marker edits/deletions reconcile without
healing damaged cars or resetting driven positions. New coastal worlds get a
hatchback marker near the capital at X/Z `(5, 8)`; older authored maps remain
intact and can receive a car through DEV tools.

Motion uses lightweight bicycle steering, bounded substeps, drag/braking,
terrain pitch/roll and acceleration response, and the existing WorldCollision
path. Exit tries both sides and the rear with multiple clearances, rejects a
moving normal exit, and parks on successful exit. World collision now uses
rotated model footprints instead of point-sized generic prop collision, so
industrial shacks/fences and cars actually block movement. Imported model
bounds are conservative solids; they do not automatically create enterable
interiors. Existing procedural house doorway/vault logic remains intact.

Vehicle rendering, chase-camera smoothing and audio each have dedicated modules.
Physics sleeps parked/unoccupied vehicles and those outside 180 m; draw distance
is 300 m; runtime vehicles are capped at 128, definitions at 64, and the vehicle
model cache at 128 body/wheel entries. Collision substeps are capped at 12, with
nine footprint probes per step (at most 108 existing collision queries for a
moving car). Cheap per-object broad phases avoid distant trigonometry. Static
world queries still scan existing world collections; a shared spatial index is
a useful next performance pass for heavily authored maps. No terrain-streaming
algorithm or NPC/loot ownership was replaced.

## Combat and sound

CombatWorld delegates logical car hit regions to VehicleSystem. Body/roof panels
stop shots; front engine hits also damage the engine; tire hits damage individual
tires and reduce handling/performance. Window shells retain 70% shot damage and
permit continued projectile tracing, including a driver-seat occupant hit.
WeaponSystem bounds penetration to eight contacts per swept segment. Destruction
disables driving and shuts down the engine. Visual window materials may remain
opaque/fused: gameplay hit regions are independent of their mesh/materials.

Damage uses normalized persisted condition (0–100) against configurable maximum
body/engine/tire health. Engine condition affects acceleration/top speed, tire
condition affects performance/body response, and the fuel callback can consume
normalized fuel without introducing a full fuel economy. Empty fuel prevents
acceleration. There are no explosions, passenger seats, vehicle repairs,
networking, full rigid-body suspension or NPC driving. Existing NPC melee and
weapons behavior remains; this pass does not add NPC firearm AI. The glass and
occupant tests fire the actual existing player projectile system headlessly.

Audio uses the committed GGBot start, loop, acceleration, shutdown, horn,
door-open/close and parking-brake OGGs. One driver engine music stream and seven
one-shot handles are loaded lazily. Effects do not overlap copies of themselves;
acceleration effects have a cooldown; exit/pause/shutdown stop streams; handles
unload before the existing audio device closes. The existing M mute and volume
controls apply to vehicles too. Nearby non-driver ambient engine loops are not
implemented.

## Controls and Creator

| Action | Keyboard/X11 | Phone |
|---|---|---|
| Enter / exit | E | USE |
| Accelerate / reverse | W / S | Left stick up / down |
| Steer | A / D | Left stick left / right |
| Brake (hold) | Space | JUMP held |
| Parking brake (hold) | C | CROUCH held |
| Horn | Shift | SPRINT |
| Driving camera orbit | Existing look controls | Right stick |
| DEV tools panel | F1 | DEV TOOLS, top center |
| Build / inspect toggle | F2 | Panel BUILD MODE / RETURN TO PLAY |
| Spawn hatchback nearby | F3 | Panel SPAWN HATCHBACK |
| Previous travel position | F4 | Panel PREVIOUS LOCATION |
| Capital / Espera / Porto Luma / Suda Haveno / Roka | 1 / 2 / 8 / 9 / 0 | Named panel buttons |

DEV initially opens inspection/gameplay controls. Enter BUILD MODE for Creator;
it safely exits a driven car first. Normal on-foot movement/combat inputs retain
their existing behavior. Held brake is separate from the on-foot Jump edge.
Native touch ownership, cancellation before fresh capture, the final synthesized
mouse release event, mode changes and modal DEV panels are quarantined. Mouse
HUD testing and the existing keyboard controls remain available.

Creator retains preview/place/select/move/rotate/duplicate/delete, history,
autosave and explicit SAVE/LOAD. Pack filters are ALL, URBAN, CHARACTERS,
SURVIVAL, INDUSTRIAL and VEHICLES; search/pagination use the existing inventory
UI. All 13 industrial shacks-pack entries and every registered vehicle definition
use the existing catalog/controller/save path. Imported models select as whole
rotated objects, procedural buildings include their roofs in selection, and
procedural tree selection includes its canopy. Bootstrap assets can be selected
and edited. A red placement ghost warns of collision; placement remains possible
for deliberate modular assembly. Search works with the existing text input;
this pass does not add automatic Android IME opening.

## Persistence and compatibility

Creator now writes **V6** full-world snapshots. Existing ASSET records remain,
with an associated VEHICLE_STATE record for definition ID, source marker,
authored home/heading, condition, fuel, enabled/destroyed flags and four tires.
Position, rotation and physical size are preserved by existing asset records.
V3 additions, V4 snapshots and V5 geography snapshots still load. IDs, roads,
settlement identity, NPC/loot markers, inventory profiles and coastal geography
are preserved. Older binaries cannot read new V6 saves; back up before downgrading.

Runtime driving/condition also saves to `verda_creator.map.vehicles` beside the
existing map path, normally `$HOME/.local/share/outland/maps/`. It is shared by
the world, not copied into inventory profiles. Saves use temp/rename and loads
validate the complete file before mutation. The companion state applies only
when ID, definition, authored home/heading and dimensions still match: later
Creator edits take precedence. Invalid companion saves are protected from
runtime overwrite and logged. Driving saves at most every two seconds while
dirty plus shutdown; Creator keeps its established edit/autosave/explicit-save
policy. ENV `OUTLAND_SAVE_DIR` continues to override the root save directory.

## Raylib 6 migration and rig limits

CMake requires `raylib>=6.0`; a shared compile-time contract asserts raylib major
6, ModelSkeleton and float UpdateModelAnimation. The tested library was exact
upstream **6.0.0**, not a 5.5 compatibility shim. ModelCache validates and owns
new skeleton/currentPose/boneMatrices resources through raylib's loader/unloader.
AnimationController and SkeletalGait use Model.skeleton and
ModelAnimation.keyframeCount/keyframePoses. Procedural skeletal walking/running,
own-model clips and bind-pose reset remain operational. Tests/mocks follow one
float-frame API contract; an additional test calls real raylib fractional pose
updates and checks currentPose/bone matrices without a graphics context.

Raylib 6 clips no longer contain bone names/parents. Compatibility therefore
requires source-model provenance, bone counts, valid poses and valid acyclic
model skeletons. CharacterRenderer only uses each model's own clips. The audit
now generates original topology BoneInfo for headless gait checks; it does not
invent animation clips or authorize retargeting by joint count. Existing audit:
**84 models, 108 embedded clips**. Common PSX humanoids have 33 joints, some
female rigs 41; creature and first-person arm rigs differ. Many body clips remain
unnamed/unmapped, so the existing named-joint procedural walking fallback is
retained. Unsupported creature rigs do not receive invented walking/attack/death
clips. See character-rig-compatibility.md/json for every rig and clip action.

## Verification

- DEV Debug build, OUTLAND_DEV_TOOLS=ON, raylib 6.0.0: **34/34 CTest tests pass**.
- Release build, OUTLAND_DEV_TOOLS=OFF, raylib 6.0.0: **29/29 pass**.
- DEV ASan+UBSan with leak detection/halt-on-UB: **34/34 pass**.
- Registry/assets, normalization/anchors, enter/exit, steer/accelerate/brake/reverse,
  activation/sleep and limits, fuel hook, body/glass/driver/tire/engine/destruction,
  marker reconciliation, transactional runtime saves, Creator vehicle/industrial
  operations and V6 round trips, DEV destinations/touch ownership, audio lifecycle,
  actual raylib 6 poses and all pre-existing regression suites are covered.
- Project runtime/tests were sanitizer-instrumented; the shared raylib library
  was built normally. Python validators are not sanitizer-instrumented.
- No game launch, GPU/display/audio-device playtest, Android APK/NDK build or
  physical phone driving test was performed. C++/raylib Termux builds are the
  intended device workflow. Steering feel, camera clearance, wheel direction,
  terrain seams and logical window placement still need the checklist below.

## Exact Termux apply/build/push steps

Download the supplied `outland-vehicles-78338d6.patch.gz` into `/sdcard/Download`.
Use the direct curl link supplied with the handoff, not an older patch filename.
Do not stage unrelated source archives. This chain stops at the first failure:

```sh
(
set -e
cd "$HOME/Outland"
git pull --ff-only
gzip -t /sdcard/Download/outland-vehicles-78338d6.patch.gz
gzip -dc /sdcard/Download/outland-vehicles-78338d6.patch.gz > "$TMPDIR/outland-vehicles.patch"
git apply --check "$TMPDIR/outland-vehicles.patch"
git apply --index "$TMPDIR/outland-vehicles.patch"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$PREFIX/share/pkgconfig"
pkg-config --modversion raylib
cmake -S . -B build/dev-raylib6 -DOUTLAND_DEV_TOOLS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/dev-raylib6 -j2
ctest --test-dir build/dev-raylib6 --output-on-failure
cmake -S . -B build/release-raylib6 -DOUTLAND_DEV_TOOLS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/release-raylib6 -j2
ctest --test-dir build/release-raylib6 --output-on-failure
git commit -m "Add vehicle gameplay, DEV building tools and raylib 6 support"
git push origin main
)
```

The pkg-config version must be 6.x; use fresh directories so prior cached 5.5
paths do not survive. Your GitHub credentials/remotes are not changed. If a
command fails, inspect `git status --short` and the first compiler error before
repeating the chain; do not apply an already-applied patch twice.

## Device inspection (manual, after builds)

1. Back up existing `verda_creator.map` and `.vehicles`, if present. In your
   established Termux-X11 session, launch `./build/dev-raylib6/outland` manually.
   The worker did not execute this command.
2. Choose DEV LAB. Open DEV TOOLS, CAPITAL, then SPAWN HATCHBACK. A fresh world
   also has its default hatchback at `(5, 8)`; old saved maps are not forcibly reset.
3. Walk to the car, tap USE, drive with the left stick, look with the right, hold
   JUMP to brake, test reverse, parking brake and horn. Stop and USE to exit.
4. Fire at body panels, engine/front lower section, tires, and window-height
   regions. Inspect reduced performance/destruction. Headless tests verify
   occupied-window penetration; a real driver taking ranged NPC fire needs an
   existing ranged shooter/test setup, which this pass does not add.
5. Drive north from the capital toward Espera `(0,-1800)` and inspect roads,
   terrain/camera seams and collisions. Use named DEV teleports for Porto Luma
   `(1750,-400)`, Suda Haveno `(300,1800)`, Roka `(-1750,450)`, and PREVIOUS LOCATION.
6. Stop, enter BUILD MODE. Open assets, choose INDUSTRIAL and VEHICLES, preview,
   place, select the whole object, MOVE/ROTATE/DUPLICATE/DELETE; test UNDO/REDO.
   Existing flight/noclip controls remain available for construction inspection.
7. SAVE; leave/restart manually and verify cars, definition/rotation/condition,
   industrial props, buildings, marker edits, roads and settlement identity remain.
   Inspect any SAVE FAILED/vehicle-state-protected log before proceeding.

Next coworker should inspect mobile steering/camera/door clearance, import the
missing GGBot bodies and tune their real bounds/anchors, profile dense authored
maps for a shared world collision spatial index, and obtain appropriately named
compatible character clips rather than guessing unmapped animations.

## Every changed file

### Registry, runtime, rendering, sound and saves

- `assets/verda/vehicles/runtime/hatchback_body.glb`
- `assets/verda/vehicles/runtime/hatchback_preview.glb`
- `assets/verda/vehicles/runtime/hatchback_wheel.glb`
- `assets/verda/vehicles/vehicle_manifest.tsv`
- `include/outland/game/vehicles/VehicleAudio.hpp`
- `include/outland/game/vehicles/VehicleRegistry.hpp`
- `include/outland/game/vehicles/VehicleRenderer.hpp`
- `include/outland/game/vehicles/VehicleSystem.hpp`
- `src/game/vehicles/VehicleAudio.cpp`
- `src/game/vehicles/VehicleRegistry.cpp`
- `src/game/vehicles/VehicleRenderer.cpp`
- `src/game/vehicles/VehicleSave.cpp`
- `src/game/vehicles/VehicleSystem.cpp`
- `tools/prepare_vehicle_assets.py`

### Creator and DEV

- `include/outland/creator/CreatorAssetRegistry.hpp`
- `include/outland/creator/CreatorController.hpp`
- `include/outland/creator/CreatorSession.hpp`
- `include/outland/creator/CreatorTouchUI.hpp`
- `include/outland/creator/VehicleAssetCatalog.hpp`
- `include/outland/dev/DevLab.hpp`
- `src/creator/CreatorController.cpp`
- `src/creator/CreatorMapIO.cpp`
- `src/creator/CreatorTouchUI.cpp`
- `src/dev/DevLab.cpp`

### Integration and world/combat/input

- `include/outland/game/combat/CombatWorld.hpp`
- `include/outland/input/InputSystem.hpp`
- `include/outland/input/PlayerInput.hpp`
- `include/outland/world/VerdaRegion.hpp`
- `include/outland/world/WorldAsset.hpp`
- `include/outland/world/physics/WorldCollision.hpp`
- `src/engine/render/Renderer.cpp`
- `src/game/combat/CombatWorld.cpp`
- `src/game/combat/WeaponSystem.cpp`
- `src/input/InputSystem.cpp`
- `src/world/VerdaRegion.cpp`
- `src/world/physics/WorldCollision.cpp`

### Raylib and animation

- `include/outland/assets/ModelCache.hpp`
- `include/outland/assets/RaylibContract.hpp`
- `include/outland/characters/AnimationAssetCatalog.hpp`
- `src/assets/ModelCache.cpp`
- `src/characters/AnimationController.cpp`
- `src/characters/CharacterRenderer.cpp`
- `src/characters/SkeletalGait.cpp`
- `tools/audit_character_rigs.py`

### Tests

- `tests/integration/assets/raylib_animation_frame.hpp`
- `tests/integration/assets/test_animation_assets.cpp`
- `tests/integration/assets/test_animation_controller.cpp`
- `tests/integration/assets/test_raylib6_pose.cpp`
- `tests/integration/assets/test_skeletal_gait.cpp`
- `tests/integration/creator/test_creator_touch_ui.cpp`
- `tests/integration/vehicles/test_creator_vehicles.cpp`
- `tests/integration/vehicles/test_dev_travel.cpp`
- `tests/integration/vehicles/test_vehicle_audio.cpp`
- `tests/integration/vehicles/test_vehicles.cpp`
- `tests/integration/world/test_combat_input.cpp`

### Build/documentation

- `CMakeLists.txt`
- `README.md`
- `docs/character-rig-compatibility.md`
- `docs/vehicles-creator-raylib6-handoff.md`
