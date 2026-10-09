# NPC AI and animation foundation

Based on main `047249d` (production characters). No character assets, manifest rows, Creator save formats, mobile bindings, or placeholder characters were changed. The game was not launched.

## Architecture and behavior

`NpcSystem` remains the actor owner and reconciles enabled `NpcSpawn` markers by settlement/marker key. It now retains position, health, state, random generator, and animation clock across reconciliation. Editing a marker moves its actor without healing it; disabling/deleting removes it. Existing V3 marker prefixes select civilian, emergency, hostile, or creature pools through `CharacterRegistry`. V1/V2/V3 map behavior remains unchanged.

`NpcBehavior` implements Idle, Wander, Alert, Chase, Attack, Flee, and terminal Dead. Civilians/emergency personnel wander and flee perceived gunfire or damage, never attack. Hostiles/creatures detect with sight checks, wait their reaction delay, chase a last-known position, and apply timed melee damage only within range and sight. Loss of sight expires memory. Dead actors never move, attack, or receive further damage. Deterministic wandering stays around each marker; movement substeps use the existing `WorldCollision` and terrain sampler.

AI decisions run at 10 Hz for active actors. Activation uses an inner radius and outer hysteresis; outside the outer radius, actors stop simulating and drawing. Sleeping actors do not accumulate catch-up work. Outer-distance despawning retains lightweight actor data/health rather than respawning healed NPCs. The model/animation LRU stays bounded. Distance checks still scan all actors; reconciliation remains a marker scan each frame. AI pauses during Creator, inventory, or lost window focus.

`CombatWorld` accepts actor trace/damage callbacks. Existing swept bullets, cover ordering, damage scaling, headshots, and hit markers now handle NPCs through the existing `WeaponSystem`. Sight and camera collision exclude actor/range-target hits. `Renderer.cpp` contains wiring only, uses existing `Health` for player damage, shows health/death, and clears normal gameplay controls on death. Creator controls remain available. ESC/BACK returns to Home; entering gameplay resets player/NPC session health. NPC health/state are session data, not a new world-save schema.

Configure per-category settings with `NpcSystem::configure(CharacterPool, NpcTuning)`. Inputs are sanitized. Defaults:

| Pool | Detection | Attack range | Walk speed | Run multiplier | Reaction | Health | Activation / outer distance | Damage / interval |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Civilian | 18 m | 1.6 m | 1.5 m/s | 1.7 | 0.6 s | 100 | 65 / 100 m | Never attacks |
| Emergency | 18 m | 1.6 m | 1.5 m/s | 1.7 | 0.6 s | 120 | 65 / 100 m | Never attacks |
| Hostile | 25 m | 1.6 m | 2 m/s | 1.7 | 0.6 s | 100 | 65 / 100 m | 15 / 1 s |
| Creature | 30 m | 1.8 m | 2.8 m/s | 1.7 | 0.3 s | 140 | 65 / 100 m | 18 / 1 s |

All pools default to a 6 m wander radius and 4 s threat memory. Configuration is a C++ API; no extra settings file or save fields were introduced.

## Animation support and limitations

`tools/audit_character_rigs.py` reads the manifest and actual GLBs, emits a machine-readable report, human compatibility report, and runtime catalog. CMake regenerates these when assets change; `--check` detects stale outputs. See [the full compatibility report](character-rig-compatibility.md).

All 82 canonical models contain skins. There are 108 embedded clips: 79 on the 80 bodies and 29 on the two arm assets. The 79 PSX body clips are generic Mixamo/Layer0 clips, approximately 0.067 seconds long, with **no changed animation values**. None declares idle, walk, run, attack, or death semantics. The rebel body has a 30-joint rig and no embedded clips.

Shared ordered topology groups contain 63 bodies with 33 joints, 14 bodies with 41 joints, and 2 bodies with 65 joints. The rebel and two arm rigs are separate (30, 52, and 50 joints). No exact signatures match after including inverse-bind matrices and node transforms. Shared names/counts alone are insufficient to safely transplant animations.

The two arm rigs expose 9 named idle and 8 named attack clips in total; another 12 arm clips are unmapped. These are audited and decoded but remain arm-only assets, not transplanted into body rendering or existing weapon presentation.

`AnimationController` provides independent actor clocks, semantic clip selection, looping Idle/Walk/Run, one-shot Attack/Death, and runtime rig validation. It uses raylib 5.5's 17 ms glTF frame sampling. Only compatible own-model clips are eligible. Missing Run uses Walk/Idle; other missing living actions use Idle or bind pose. Missing Death uses bind pose; it does not revive an actor or invent a death animation. The shared cache restores bind pose when a different actor needs fallback, so one actor's sampled pose cannot leak into another. Animation allocations are lazy and freed on LRU eviction/shutdown. Unmapped body clips are deliberately not allocated during drawing.

Current body movement therefore translates/turns bind-pose characters, and dead bodies remain in bind pose. No retargeting, blending, navmesh/pathfinding, ragdolls, or corpse poses were invented. When real body clips are added, give them explicit action tokens (idle/walk/run/attack/death), regenerate the audit/catalog, and verify their own skeleton compatibility. The controller then selects them automatically.

## Validation

Final checks:

- DEV Debug build, `OUTLAND_DEV_TOOLS=ON`: **20/20 CTest tests passed**.
- Release build, `OUTLAND_DEV_TOOLS=OFF`: **18/18 CTest tests passed**.
- AddressSanitizer + UndefinedBehaviorSanitizer, leak detection enabled: **5/5 focused suites passed** (NPC AI, animation controller, real GLB clip decoding, model cache, character renderer). The project C++ code is instrumented; the installed raylib library is not.
- Real raylib CPU animation decoder loaded all 82 GLBs and 108 clips without creating a window or launching the game.
- Coverage includes seven states, passive/aggressive categories, delayed reactions, sight loss, activation/hysteresis, pause/sleep, death, marker reconciliation, damage/headshots, swept weapon hits, cover, independent animation clocks, loops/one-shots, missing/incompatible rigs/clips, shared bind-pose restoration, and cache allocation/eviction cleanup.
- Existing Creator persistence/touch controls, collision/combat/input, character/asset validation tests pass. `git diff --check` passes.

Reproduce with your installed raylib pkg-config environment:

```sh
cmake -S . -B build/dev -DOUTLAND_DEV_TOOLS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/dev -j2
ctest --test-dir build/dev --output-on-failure
cmake -S . -B build/release -DOUTLAND_DEV_TOOLS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j2
ctest --test-dir build/release --output-on-failure
python tools/audit_character_rigs.py --check
```

Next coworker: author real body clips first, then inspect steering around interiors (collision stops actors but does not find routes), Android frame cost with many active markers, and player death/re-entry presentation on the phone. No visual/device gameplay check was performed, per the no-launch instruction.

## Every changed file

| File(s) | Change |
|---|---|
| `CMakeLists.txt` | Behavior/controller sources, audit regeneration/check, three new headless test targets. |
| `include/outland/characters/NpcBehavior.hpp`, `src/characters/NpcBehavior.cpp` | Reusable state machine, environment/sight adapters, category tuning. |
| `include/outland/characters/NpcSystem.hpp`, `src/characters/NpcSystem.cpp` | Persistent actor state, activation, damage/trace/events, marker reconciliation and session reset. |
| `include/outland/characters/AnimationController.hpp`, `src/characters/AnimationController.cpp` | Clock, clip classification/selection, rig validation, shared-model pose application/fallback. |
| `include/outland/characters/CharacterRenderer.hpp`, `src/characters/CharacterRenderer.cpp` | Player/NPC animation state, lazy clip loading, configurable NPC visibility and fallback drawing. |
| `include/outland/assets/ModelCache.hpp`, `src/assets/ModelCache.cpp` | Own cached animation allocations and shared-pose state; free clips on eviction/clear. |
| `include/outland/game/combat/CombatWorld.hpp`, `src/game/combat/CombatWorld.cpp` | Actor callbacks, NPC hit kind, trace filtering, routed damage. |
| `src/game/combat/WeaponSystem.cpp` | Existing projectile damage/hit feedback extended to NPCs. |
| `src/engine/render/Renderer.cpp` | Wire combat/AI/animation, player health/death HUD/session reset, retain Creator isolation. |
| `tools/audit_character_rigs.py` | Deterministic manifest/GLB rig and clip audit. |
| `include/outland/characters/AnimationAssetCatalog.hpp` | Generated runtime clip/rig metadata. |
| `docs/character-rig-compatibility.json`, `docs/character-rig-compatibility.md` | Generated full audit and compatibility groups. |
| `tests/integration/assets/test_npc_ai.cpp` | Behavior, activation, death, markers, combat/cover tests. |
| `tests/integration/assets/test_animation_controller.cpp` | Rig/clip fallback, state clocks, loop/one-shot, shared-pose restoration tests. |
| `tests/integration/assets/test_animation_assets.cpp` | Real CPU-only decoding of every production GLB's embedded clips. |
| `tests/integration/assets/test_model_cache.cpp` | Mock clip allocations, lazy loading/failure, LRU ownership and cleanup tests. |
| `tests/integration/assets/test_character_renderer.cpp` | Headless animation API mocks. |
| `docs/npc-ai-handoff.md` | This architecture, validation, limitation, and changed-file handoff. |
