# Production loot and inventory foundation

Based on main `14fddad`. This extends existing Creator LootSpawn markers, WeaponSystem, Health, ModelCache and mobile input. The previous equipment-only panel and marker-to-reserve-ammo shortcut are removed. Character models/rigs, NPC AI, movement, map format and existing world saves are unchanged. No game was launched.

## Gameplay

Approach a visible pickup within 2.8 m and press **E / USE**. The prompt names the item and quantity. Static-world geometry must not obstruct the interaction ray. Accepted quantities enter the bag; any quantity that does not fit remains on the ground. One firearm per existing weapon type may be carried, so duplicate guns stay in the world instead of overwriting loaded ammunition.

Open **BAG / I**. Select a row, then **USE / EQUIP**, **DROP 1**, or **DROP STACK**. **PREV/NEXT** reaches all slots; **CLOSE/I** returns to gameplay. Desktop arrows select rows, Enter uses, Delete drops one, Shift+Delete drops the stack. The panel captures all touch contacts and clears gameplay movement/fire while open; only keyboard weapon cycling is retained. Android contact IDs are edge-triggered, dragging/reordering contacts does not repeat item actions, native touch suppresses synthesized mouse clicks, and focus changes do not replay held contacts. Controls scale together from a 720-high canvas.

The starting bag preserves the existing pistol/rifle and 12/30 loaded magazines, 60 pistol rounds and 120 rifle rounds. It adds two canned foods, two waters and one bandage. These values come from TSV data. Capacity is 12 stack slots; equipping the backpack adds 12, and the portable trunk adds 8. Only one capacity modifier is active. Dropping the last equipped container is refused if remaining contents would exceed base capacity.

Food, water and soda consume one item and emit food/water hooks with configured values. Totals persist as foundation data; hunger/thirst meters or decay are not simulated. Antibiotics, antiseptic and bandages heal existing player Health and are retained when health is full or the player is dead. The medical items currently provide generic healing, not infection diagnosis.

Equipping a firearm selects the existing WeaponSystem backend. Ammo rows only reload compatible weapons. Loose rounds remain in inventory until reload completes; magazine rounds remain in WeaponSystem. A firearm drop transfers its actual loaded rounds into the pickup and clears its magazine; recollection restores those rounds. Cycling with only one owned firearm does not cancel its reload. Legacy `collect_ammo()` cannot bypass a bound inventory.

Lantern/torch use toggles a portable light effect; switching on consumes one supplied battery. The current visual effect is a warm tint on nearby pickups and a ground ring, not dynamic scene lighting. Axe, knife, tent, sleeping bag, campfire kit, matches and lighter are real transportable pickups that can be carried and dropped. Melee attacks, sleeping, deployable camp mechanics and crafting are not included. Backpack/trunk are capacity items; nested inventories and separately accessible world storage are not implemented.

The hunting-rifle pickup uses the real Survival GLB and the existing VR30 rifle combat tuning. Equipping preserves the current firearm rendering/ballistics; bolt-action mechanics and PSX first-person weapon rigs are not introduced. The V9 pistol pickup reuses existing procedural gun geometry, with its hands hidden. No supplied pistol GLB was invented.

## Author loot in Creator

Select **Gameplay**, search **Loot**, and place General, Medical, Camp or Weapon Loot Spawn markers. Move/duplicate/delete and save using the existing builder. Marker IDs select the corresponding table through `loot_spawn_<table>_...`; the unsuffixed existing Loot Spawn remains the general table. This adds no fields or changes to the Creator map schema.

Fresh training worlds load five sample markers from `maps/verda_loot_defaults.map`: three near the starting area and two in Espera. Existing authored maps take precedence and receive no injected markers; add LootSpawn markers in the builder to populate those maps. The sample map uses the existing V3 loader and is included in future full V4 builder snapshots when saved.

Loot generation is deterministic per settlement/marker/mode. It activates within 90 m, reconciles every half-second, and renders models within 60 m through the existing bounded ModelCache. At most 1,024 live pickups and 10,000 visited marker records are retained. Outside activation distance a marker does not roll its table. Fully scavenged markers remain empty across restarts. Moving, disabling, deleting or changing a marker's table intentionally invalidates its generated loot; identical IDs in different settlements remain distinct. Dropped pickups have independent IDs.

## Data and architecture

`assets/verda/survival/gameplay/items.tsv` defines 22 items across food, water, medical, ammo, weapon, utility and container categories: model, stack size, displayed model height, action/value, compatible weapon, capacity modifier, use requirement, starting quantity and loaded rounds. Stable IDs are persistence identities. The registry validates definitions and starter capacity transactionally.

`loot_tables.tsv` defines weighted General/Medical/Camp/Weapons tables, quantity ranges and roll counts for normal and Zombie modes. Zombie ammo has smaller ranges and lower weights; starter weapon/ammo quantities remain unchanged to preserve existing combat. Tune data, rebuild, and restart. CMake refreshes these TSVs and the sample map beside the executable even when no C++ source changed. Existing Survival GLBs/textures are reused unchanged and audited against the existing runtime manifest.

- `ItemRegistry`: typed item and table metadata; marker table resolution.
- `Inventory`: stack operations, capacity, unique firearm ownership, equipped container and consumption foundation state.
- `LootWorld`: bounded marker-generated/dropped pickups, deterministic activation and reconciliation.
- `LootSession`: pickup/use/equip/drop, existing Health/WeaponSystem bridges, dirty status and debounced saves.
- `LootSave`: validated transactional profile I/O.
- `LootUI`: modal input, inventory presentation and pickup rendering using ModelCache.
- Renderer delegates to these modules; it contains no item-specific behavior or item model filenames.

## Persistence boundaries

Profiles live beside the existing user maps, normally:

```text
~/.local/share/outland/profiles/explore.loot
~/.local/share/outland/profiles/battle_royale.loot
~/.local/share/outland/profiles/zombie.loot
```

They follow the existing HOME/OUTLAND_SAVE_DIR/application-data path policy. The new version-1 profile stores stacks, equipped capacity item, light state, consumption totals, selected firearm, loaded magazines, remaining pickups, spent marker records and drop-ID sequence. Loose ammo is stored only in bag stacks. There is no second authoritative ammo store.

Autosave runs after one quiet second; failures retry after five seconds. Returning Home and normal shutdown also save. A writable-profile save failure keeps the live session and reports failure. A corrupt/incompatible existing profile is never overwritten with starter items; the user can return Home, but changes in that fallback session do not replace the protected file. Stable item/table IDs must be preserved when tuning; migrations for future incompatible catalogs are not yet implemented.

Profiles do not write `verda_creator.map`, serialize player position/health, or persist NPC AI. Existing health/NPC session reset behavior is retained. Reload timers and active bullets are not persisted: a mid-reload save contains the original magazine and unspent loose ammo, then resumes with the reload canceled. Editor saves contain authored map content, not runtime pickups or player inventory. DEV Creator does not bind the gameplay inventory or scavenge markers.

## Validation

- DEV Debug: **27/27 tests passed**.
- Release, Creator OFF: **24/24 tests passed**.
- ASan/UBSan with leak detection: **11/11 focused suites passed** (loot, loot UI, combat, combat input, NPC AI, Creator map/touch/session, runtime assets, animation and gait). Project code is instrumented; the installed raylib library is not.
- New tests cover registry/data references, Zombie ammo tuning, deterministic/activation-limited spawning, disabled/moved markers, spent-marker persistence, capacity and partial pickup, stacking, consumption hooks, medical use, ammo compatibility and reload conservation, duplicate firearms, loaded-magazine drops, blocked/full-world drop rollback, modal/scaled multitouch, autosave, mode isolation, mid-reload persistence, malformed/truncated/duplicate records and safe save failure.
- Existing combat, character/NPC, animation, mobile controls, Creator editing and V3/V4 map regression suites remain green. Metadata copy behavior and `git diff --check` pass. No game/window was launched.

## Changed files

| Files | Change |
|---|---|
| `CMakeLists.txt` | Inventory sources, test targets/wrappers, asset audit and always-current loot metadata copies. |
| `assets/verda/survival/gameplay/items.tsv` | Data-driven item catalog and starting kit. |
| `assets/verda/survival/gameplay/loot_tables.tsv` | Four weighted tables with normal/Zombie variants. |
| `maps/verda_loot_defaults.map` | Fresh-world sample LootSpawn markers, existing map format. |
| `include/outland/game/inventory/ItemRegistry.hpp`, `src/game/inventory/ItemRegistry.cpp` | Registry and table parser. |
| `include/outland/game/inventory/Inventory.hpp`, `src/game/inventory/Inventory.cpp` | Stack/capacity/container state. |
| `include/outland/game/inventory/LootWorld.hpp`, `src/game/inventory/LootWorld.cpp` | World pickups and marker activation/reconciliation. |
| `include/outland/game/inventory/LootSave.hpp`, `src/game/inventory/LootSave.cpp` | Separate atomic mode profiles and validation. |
| `include/outland/game/inventory/LootSession.hpp`, `src/game/inventory/LootSession.cpp` | Actions, ammo/health hooks and session persistence. |
| `include/outland/game/inventory/LootUI.hpp`, `src/game/inventory/LootUI.cpp` | Inventory UI, touch ownership and world model presentation. |
| `include/outland/game/combat/WeaponSystem.hpp`, `src/game/combat/WeaponSystem.cpp` | Optional inventory bridge, ownership-aware selection, magazine transfers and canonical reload ammo. |
| `include/outland/game/combat/CombatRenderer.hpp`, `src/game/combat/CombatRenderer.cpp` | Optional hands visibility for reusing the pistol as a pickup. |
| `include/outland/creator/CreatorAssetRegistry.hpp`, `src/creator/CreatorController.cpp` | Medical/Camp/Weapon LootSpawn options using existing marker persistence. |
| `src/engine/render/Renderer.cpp` | Replace equipment/ammo stubs with module wiring, world prompts, modal actions and profile saves. |
| `tests/integration/inventory/test_loot.cpp` | Gameplay and persistence boundaries. |
| `tests/integration/inventory/test_loot_ui.cpp` | Headless UI ownership/paging/scaling regression checks. |
| `tools/audit_loot_assets.py` | Catalog/model/texture references and loot-table validation. |
| `docs/loot-inventory-handoff.md` | Gameplay, architecture, changed files, tests and limitations. |

Next coworker should inspect on-phone item scale/orientation, inventory ergonomics, obstacle interaction/drop checks and restart persistence. Follow-ups include dedicated hunting-rifle tuning/visuals, melee tools, actual deployable camps, dynamic lantern lighting and nested/world containers. Full survival stats remain deliberately outside this task.
