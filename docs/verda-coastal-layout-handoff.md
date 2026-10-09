# Verda central capital and coastal settlement layout

Base: main `6e3be35` (production loot/inventory). No game was launched.

## Positions (metres, X/Z)

Main previously defined only Espera and the training origin; it had no capital, other cities, coastline or finite world boundary. This pass names the existing central origin as Verda's capital and establishes three additional coastal settlement foundations. The starter names can be edited in Creator snapshots.

| Settlement | Previous X/Z | New X/Z | Identity |
|---|---|---|---|
| Verda capital | Existing central origin `(0, 0)` | `(0, 0)` | Central residential/civic building pad |
| Espera | `(0, -70)` | `(0, -1800)` | Original northern rural village |
| Porto Luma | Not previously defined | `(1750, -400)` | Eastern trading port, shops/warehouses |
| Suda Haveno | Not previously defined | `(300, 1800)` | Southern residential haven |
| Roka | Not previously defined | `(-1750, 450)` | Western industrial town, muted warehouses/shops |

The four coastal hubs are 1.79–1.83 km from the capital and over 2 km from adjacent hubs. The irregular shoreline is roughly 2.0–2.4 km from the origin. Cities stay inland with space for streets/buildings, rather than sitting on the water. The capital is flat to a radius of 180 m, transitions smoothly to natural terrain over the next 80 m, and stays at the existing player/training origin. Smaller 85 m flat settlement pads transition over 65 m. The countryside between them retains procedural hills and foliage.

There was no larger authored world to shrink. The existing 640 m fixed visible ground patch is replaced with player-centered streaming; the world has no new coordinate clamp. Land extends across an approximately 4.4 km island, with ocean beyond it.

## Roads and existing contents

`VerdaLayout.hpp` is the shared source for stable site IDs, default centers, geographic constants and coast shape. No city coordinates are placed in Renderer.cpp.

Espera remains settlement index zero to preserve existing Creator, map-overlay and loot-source contracts. Its six buildings, ten trees and two roads keep IDs, styles, sizes, rotations and metadata and translate together by Z=-1730. The same translation handles any generated assets/markers. The capital and three additional towns use the existing enterable procedural building/road system, with eight buildings per new site. These are editable settlement foundations, not finished hand-authored cities or a new asset/character pipeline.

Each coastal hub has an asphalt radial route to the capital. An inland ring at radius 1500 m links neighbouring hubs around the island. Routes connect at town centers, remain on land, and leave large wilderness sectors between them. Local Espera streets remain dirt/gravel. Distant road segments are culled; nearby road ribbons retain 2 m terrain sampling instead of stretching tessellation across kilometre-scale links.

The two default Espera loot markers move with the village. The three training-area loot markers stay at the central origin. IDs and existing loot tables remain unchanged. Existing loot reconciliation already invalidates generated loot when a marker position changes; bags, loose ammunition, equipped weapons and independent dropped pickups are not reset or rewritten. The new towns do not receive invented NPCs or injected loot markers: populate them with the current Creator catalogs and save.

## Rendering, movement and DEV destinations

`TerrainWorld` maintains 49 terrain chunks (7x7, 128 m each). It reuses overlapping chunks when crossing a boundary, unloads outgoing chunks, and supports negative coordinates, teleports, and rejecting non-finite/extreme positions. At a normal one-chunk move it replaces seven meshes, rather than rebuilding the whole island. Geography changes invalidate the mesh cache. CPU and GPU ownership are tested with wrapped uploads/model functions without a graphics context.

The ocean is an opaque streamed plane at Y=-32 m over lowered seabed, below the original terrain's theoretical minimum so inland valleys do not become unintended lakes. Walkers/NPC movement using the existing WorldCollision path stops at submerged shoreline ground; Creator flight remains independent. Swimming, boats, waves, water lighting and water projectile effects are not implemented. Shore grass is suppressed below the waterline.

DEV key 1 remains the central training/capital spawn. Key 2 and the building-test destination resolve Espera by saved settlement ID, so they follow authored centers too. Additional keys: 8 Porto Luma, 9 Suda Haveno, 0 Roka. Missing settlements fall back to the training origin. Existing gameplay spawn, training/combat coordinates, controls and other DEV destinations remain unchanged.

## Persistence compatibility

The existing Creator serializer now writes **V5 full-world snapshots**, adding a required `GEOGRAPHY 0/1` record. V3 addition maps and V4 full-world saves still load. V4 saves keep all authored settlement centers, buildings, assets, roads, markers, population and state; they select the original terrain. The Renderer constructs the legacy generated baseline when an authored map is present, so V3 additions retain the original Espera as well. V5 restores the saved geography explicitly. Parsing commits the geography only after the full snapshot succeeds.

Existing authored maps are intentionally not forcibly migrated to the new layout. Relocating an unknown edited map automatically would violate preservation. Fresh worlds on the user's phone receive the coastal layout; the save search before this pass found no authored map. New V5 snapshots preserve edits/deletions/roads through the same full-world persistence and Creator autosave/undo infrastructure. Older binaries cannot read V5: back up a save before downgrading.

`TerrainHeight` already serves a single active runtime world through static sampling. The new geography switch follows that architecture and is set before mesh construction/after a successful map load. Simultaneously simulating multiple independent terrain profiles would require a separate terrain context; this pass does not introduce that broad refactor.

## Changed files

| File | Change |
|---|---|
| `include/outland/world/VerdaLayout.hpp` | Shared sites, centers, capital/coast constants. |
| `include/outland/world/VerdaRegion.hpp` | Coastal/legacy generation selection and saved geography property. |
| `src/world/VerdaRegion.cpp` | Move Espera intact, generate new foundations, radial/ring roads, local road culling. |
| `include/outland/world/terrain/TerrainHeight.hpp` | Active-world geography selection. |
| `src/world/terrain/TerrainHeight.cpp` | Flat capital/town pads, smooth irregular shoreline and seabed; original terrain retained for legacy maps. |
| `include/outland/world/terrain/TerrainWorld.hpp` | Streaming update/cache state. |
| `src/world/terrain/TerrainWorld.cpp` | Bounded reusable terrain chunks, cache invalidation and streamed ocean tiles. |
| `src/world/physics/WorldCollision.cpp` | Block submerged coast ground for walkers in coastal worlds only. |
| `src/world/foliage/FoliageSystem.cpp` | Suppress submerged coastal grass. |
| `src/creator/CreatorMapIO.cpp` | V5 geography persistence and V3/V4 compatibility. |
| `src/engine/render/Renderer.cpp` | Select saved-world baseline, follow camera with terrain, pass saved region to DEV destinations. |
| `include/outland/dev/DevLab.hpp` | Region-aware DEV destinations and new hub shortcuts. |
| `src/dev/DevLab.cpp` | Resolve saved centers; expose hub shortcuts. |
| `maps/verda_loot_defaults.map` | Translate Espera markers; retain training markers/IDs. |
| `tests/integration/world/test_verda_layout.cpp` | New geometry, connectivity, preservation, terrain-stream ownership, shoreline and persistence checks. |
| `tests/integration/world/test_verda_environment.cpp` | Explicit legacy fixture retains original environment geometry regression checks. |
| `tests/integration/inventory/test_loot.cpp` | Three origin sources activate first; two distant Espera sources activate on arrival. |
| `CMakeLists.txt` | Register the new headless layout/streaming test. |
| `docs/verda-coastal-layout-handoff.md` | Layout, file list, compatibility, verification and device handoff. |

## Validation and next inspection

DEV and release builds use raylib 5.5, matching the version installed separately on the phone. The project animation code still requires 5.5; this pass does not migrate it to the breaking 6.0 animation API.

New tests verify fixed/flat capital, non-overlapping coastal sites, exact Espera content translation, land-only connected roads, legacy geography, V5 authored edits/markers/roads, rejection of invalid geography without mutation, correct DEV destinations, mesh reuse/cleanup across positive/negative coordinates and teleports, geography cache refresh, bounded chunk counts and shoreline collision. Existing combat, inventory, NPC, Creator, animation and asset tests are run in both builds. ASan/UBSan with leak detection checks the changed terrain lifecycle and relevant world/save/gameplay paths. Final verification: DEV 28/28 tests; release 25/25; seven relevant ASan/UBSan tests with leak detection; `git diff --check` passed. No game or graphics window was launched.

The next coworker should profile chunk upload spikes at boundary crossings and DEV teleports on the actual phone. Mesh creation is synchronous; a teleport rebuilds up to 49 meshes. A worker/loading queue could smooth that later without changing the layout. Device visuals, FPS, shoreline tessellation and long walking routes have not been inspected because the game was not launched. Use Creator to grow each foundation and add loot/NPC markers; save writes the permanent V5 snapshot.
