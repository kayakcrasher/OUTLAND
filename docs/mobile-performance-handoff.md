# Mobile performance handoff

Base: main 195ac0b. No game window was launched. No map/save format or asset coordinates change.

## Changes

- `FoliageSystem.hpp/.cpp`: deterministic placement cache, nearest-first fill, at most 24 world/road exclusion evaluations per frame, bounded local footprint. Warm stationary views do no exclusion/height queries. Structural changes and coastline switches invalidate automatically; Creator place/move/rotate/duplicate/delete/undo/redo/load explicitly invalidate.
- `TerrainChunk.cpp`: indexed 25x25 vertex grid replaces duplicated triangle vertices. Same 1,152 triangles, winding, sampled heights and resolution; 625 vertices/height samples instead of 3,456 (82% fewer). Indices belong to raylib's mesh and are released with it.
- `TerrainWorld.hpp/.cpp`: nearest-first streaming builds at most two meshes per update, including startup, teleports and geography switches. Existing chunks are reused and the footprint remains 49. World collision continues sampling the unchanged terrain, independent of loaded render chunks.
- `ViewFrustum.hpp`: conservative sphere/side-plane tests for perspective cameras; handles portrait aspect and objects crossing the camera/planes. Unsupported projection fails open. No gameplay activation distances change.
- `VerdaRegion.hpp/.cpp`: cull procedural buildings/trees using actual terrain height; reject far procedural trees before height sampling. Cache road ribbon heights on stable two-metre segments, keyed by geometry/coastline; bound cached roads and long-road samples. Road movement selects fresh geometry.
- `CharacterRenderer.hpp/.cpp`: cull offscreen actors before model loading and skeletal deformation. Simulation and animation clocks continue. Deferred GPU loads do not mark characters as broken.
- `ModelCache.hpp/.cpp`: reuse resolved paths on cache hits, avoiding per-draw filesystem checks. Optional per-frame GPU-load budget keeps hot models drawable; deferred models retry next frame. The default remains unlimited for existing consumers/tests. Cache hits, LRU, texture ownership and animation loading remain intact.
- `Renderer.cpp`: wire conservative view culling, one new world model and one new character model per frame, retry deferred asset thumbnails, invalidate grass after Creator edits. Reconcile NPC marker instances at 2 Hz in gameplay rather than reconstructing their vector/maps every frame; Creator retains immediate reconciliation. Mode entry reconciles immediately. Add opt-in CPU submission timing panel.
- `CombatWorld.cpp`: conservative XZ rejection before terrain sampling, trigonometry or model collision lookup. Narrow-phase wall/window/roof, vehicle, actor and projectile behavior remains intact. Unknown/zero-sized model footprints use the previous path.
- `FrameTimings.hpp`: average/peak wall-clock costs for simulation, terrain, foliage, world submission and character submission. This is not a GPU profiler and excludes EndDrawing/frame limiting; FPS remains separately visible.
- `test_render_performance.cpp`: perspective/portrait/large-bound/plane-crossing tests and timing accounting.
- Existing environment, Verda layout and model-cache tests: budget ceilings, cache reuse/invalidation/teleports, streaming convergence, indexed winding/bounds/ownership, deferred-load retries and zero-stat cache hits.
- `CMakeLists.txt`: register performance test and headless benchmark in both build variants.
- `benchmark_world_render.cpp`: reproducible CPU-only foliage benchmark with 6,000 collision props. No graphics context or audio device.
- `run_termux.sh`: select optimized release/DEV executable and the system raylib 6 runtime; check DISPLAY; report explicitly forced software rendering.

## Validation

- DEV Debug: 54/54 tests passed.
- Release: 45/45 tests passed.
- ASan + UBSan: 6/6 changed-runtime suites passed (render performance, environment, layout/streaming, model cache, character renderer, combat).
- `bash -n tools/run_termux.sh` and `git diff --check` passed.
- Optimized CPU-only 6,000-prop fixture, identical 120-frame harness against old/new foliage code: warm mean 5.365 ms -> 0.053 ms; overall mean 5.173 ms -> 0.109 ms; peak 8.062 ms -> 0.662 ms. Timings vary with host scheduling and are not mobile FPS. Warm-placement query count is exactly zero; new-cell work never exceeds 24/frame.
- Indexed terrain and streaming budget/convergence are checked using mocked GPU uploads against real terrain generation; all indexed triangles retain upward winding and valid indices.
- No game window was launched.

## Device install/build

Apply the supplied patch using `$TMPDIR`, not `/tmp`. Use `git apply --check` then `git apply --index`; do not add untracked source archives.

```bash
cd ~/Outland
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$PREFIX/share/pkgconfig"
export LD_LIBRARY_PATH="$PREFIX/lib"
export LD_PRELOAD=
cmake -S . -B build/dev-mobile-performance -DOUTLAND_DEV_TOOLS=ON -DOUTLAND_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/dev-mobile-performance -j2
ctest --test-dir build/dev-mobile-performance --output-on-failure
cmake -S . -B build/release-mobile-performance -DOUTLAND_DEV_TOOLS=OFF -DOUTLAND_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/release-mobile-performance -j2
ctest --test-dir build/release-mobile-performance --output-on-failure
```

DEV tools are enabled independently of compiler optimization; use the optimized DEV build for map editing.

## Phone inspection (performed by the teammate, not in this cloud session)

Start Termux:X11 as usual and select its display. Do not change an already working GPU/virgl setup.

```bash
cd ~/Outland
export DISPLAY=:0
OUTLAND_PERF=1 bash tools/run_termux.sh release
# For map editing:
OUTLAND_PERF=1 bash tools/run_termux.sh dev
```

1. Compare the same location/view (capital downtown) and same mode before/after. Let streaming settle, then record FPS and timing panel for 60 seconds.
2. Walk and drive across chunk boundaries, enter a coastal town, and teleport with DEV tools. Terrain stays collision-correct while distant render chunks fill progressively.
3. Turn 180 degrees and switch first/third person: actors/buildings should remain visible when in view, with no new collision or combat changes.
4. Browse Creator packs, move/rotate/place/delete buildings and roads, undo/redo, save/load/export. Fresh thumbnails/models may appear over several frames. Grass exclusions rebuild after edits.
5. Verify Explore stores/loot, BR shooting, NPCs, vehicles and saves. The normal launcher uses OUTLAND_PERF=0/default to hide the panel.
6. If FPS is still poor, share phone model, mode, timing-panel screenshot and the existing graphics driver setup. CPU submission timings cannot distinguish every GPU stall. Software OpenGL, display resolution, thermal throttling and GPU driver support can still dominate.

## Limits / next inspection

Model uploads are bounded in count, not duration: a single large GLB/texture can still hitch. Existing vehicle/pickup model loaders are unchanged. Initial models, grass and terrain appear progressively; collision/saves are independent. Foliage exclusions reflect parked vehicles when cells are evaluated; moving-vehicle grass patches refresh on local cache eviction/invalidation. Existing cache capacities are retained; unusually diverse visible assets may still cause LRU churn. Large texture atlases/instancing and GPU/driver-specific tuning need device evidence. A headless benchmark is not a guarantee of 60 FPS on a phone.
