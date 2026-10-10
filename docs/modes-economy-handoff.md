# OUTLAND match lifecycle, Explore and economy handoff

Base: `c4f77d9` on `main` (includes the newly committed 43-model interior pack).
The patch extends existing bots, NPC life, justice, weapons, inventory, vehicle runtime and Creator catalogs. No game was launched during this work.

Battle Royale starts with the existing data-driven rifle/pistol starter kit, loaded magazines, reserve ammunition, food, water and medical supplies. Each match starts fresh; old BR inventory files are neither restored nor overwritten. The player drops at a collision-checked town location; 23 production-character bots spread around Verda's settlements with 150 m player clearance and 60 m separation. Existing Easy/Medium/Hard bot brains remain authoritative.

The objective is **Be the last survivor.** A capital-centred danger circle opens at 2,450 m, waits 60 seconds, then shrinks over 600 seconds to 25 m. Outside damage is 6 health/second; bots converge toward safety. Zone eliminations earn no player kill credit. `ModeSession` latches win/death, placement, kills, participants and difficulty. Death from vehicles/NPCs also ends the match. Mutual final deaths do not award victory. Result controls offer restart and Home; restart uses the same complete initialization as a new match. AI, driving and projectiles pause on the result screen. Match driving/destruction is rolled back before leaving BR, protecting the persistent Explore/Creator fleet.

Explore starts in the capital near an accessible authored store at 09:30. Settlement lookup uses the stable capital ID, not map order (Espera is first in the shipped map). Explicit `OUTLAND_DEV_SPAWN` still overrides this. Its banner explains exploring, driving, interacting, visiting towns and obeying/breaking the law. It has no victory condition. Death offers respawn/Home and keeps the player's inventory and money. Existing civilians, witnesses/police, wanted levels, vehicles, combat, inventory, loot and day/night remain in use.

Economy uses whole **Verdan kronoj** and grants a new/legacy Explore profile 250 once. Buy supplies/equipment/furniture and sell scavenged goods for income. All **67 items** are available at general stores; **128 offers** also cover medical, hardware, furniture and pub specialties. Prices, resale values and starting stock live in `assets/verda/economy/offers.tsv`. Existing Shop, Clinic, Garage and Pub places provide storefronts, using the same authored purposes and door positions as civilian life. A Shop building ID containing `furniture` or `hardware` chooses that specialty; ordinary shops are general stores. No settlement coordinates are duplicated in the economy.

Approach a door within 3.5 m with a clear approach and press **E / USE**. Store interaction takes priority over nearby cars/pickups. Select a row, then **BUY ONE / SELL ONE**. Arrows select, Enter buys, Delete sells, and I/CLOSE leaves. The store owns input while open; held/reordered Android contacts and focus recovery do not repeat trades. Home, difficulty, result and store buttons fit narrow portrait and landscape viewports and retain the existing Termux/X11 short-click adapter.

Trades check exact quantities, funds, stock, ownership and capacity before transferring anything. They keep cash and inventory together and save immediately. A corrupt profile disables trading and preserves the original file. Loaded guns cannot be sold; spending the loaded rounds or dropping the gun conserves ammunition. Buying a gun equips it unloaded (reload using compatible loose ammo). Selling an empty selected gun switches to another owned gun. Backpacks/containers still use the existing BAG/USE equipment flow.

All **43 Dissonant Void interior models** from `assets/verda/interiors/props/` are available for purchase and DEV placement, including furniture, household goods and appliances. Their real GLB bounds generate the DEV catalog from `items.tsv`. The flat carpet uses horizontal extent for pickup normalization, avoiding division by zero. In Explore, use **BAG / DROP** to put purchased props into the world; they remain recoverable pickups and survive profile reload. In DEV, find them under **ALL / PROPS**, place/select/edit them with existing Creator tools, then SAVE/EXPORT to publish them as part of the game world. Temporary substitute furniture was discarded once the committed interior pack landed.

`OUTLAND_LOOT 2` saves cash, restock countdown and per-shop stock in the same atomic file as the existing bag, weapons and world pickups. Version 1 profiles still load, receive the initial grant once, then upgrade on save. An empty wallet stays empty after reload. Existing Creator map formats, world exports, settlement coordinates and vehicle save format are unchanged. Goods restock after ten minutes of active gameplay; modal screens pause the countdown. Selling goods cannot generate a buy/sell arbitrage loop from the configured prices.

Known limits: prices are fixed; jobs, wages, taxes, property ownership and NPC wallets are not simulated. Stores are open continuously rather than following individual shopkeepers' hours. Most furniture/appliances are decorative inventory/world props; seating, appliance power and housing storage are not added. Explore DROP does not export a prop into the published map; use DEV placement/SAVE/EXPORT for that. Circle pacing, storefront approaches and phone rendering/touch feel need device inspection; headless tests do not substitute for that. Existing character rigs/animations were preserved, not changed by this pass.

Validation on upstream raylib 6.0, without opening a game window:

- DEV Debug build and full CTest: **53/53 passed**.
- Release build (DEV tools OFF) and full CTest: **44/44 passed**.
- AddressSanitizer + UndefinedBehaviorSanitizer with leak detection: **6/6 passed** (mode sessions, economy, Explore coexistence, UI, loot and vehicles).
- Catalog checks verify every interior GLB has embedded textures and a purchase/DEV entry; all existing combat/NPC/animation/Creator/world/navigation/law/day-night tests remain green.
- Patch application is checked against the base before download delivery.

Every changed file:

| File | Change |
|---|---|
| `CMakeLists.txt` | New runtime modules, generated interior catalog dependency, economy deployment and tests. |
| `assets/verda/economy/offers.tsv` | 128 data-driven prices, resale values and stock limits. |
| `assets/verda/survival/gameplay/items.tsv` | 67-item catalog including household goods and all 43 interiors. |
| `include/outland/creator/CreatorAssetRegistry.hpp` | Register the generated interior entries. |
| `include/outland/creator/EconomyAssetCatalog.hpp` | Generated DEV definitions from item metadata and real GLB bounds. |
| `include/outland/game/HomeScreen.hpp` | Responsive bounds and stable native touch ownership. |
| `src/game/HomeScreen.cpp` | Responsive mode/difficulty buttons and existing X11 click integration. |
| `include/outland/game/ModeSession.hpp` | Reusable session phase, match result and circle configuration. |
| `src/game/ModeSession.cpp` | Complete result lifecycle, circle and safe mode spawning. |
| `include/outland/game/PlayUI.hpp` | Store/result modal interface and objective banner. |
| `src/game/PlayUI.cpp` | Responsive touch UI, objectives, results, trade selection. |
| `include/outland/game/ai/BattleRoyaleBots.hpp` | Circle integration hook on the existing bot system. |
| `src/game/ai/BattleRoyaleBots.cpp` | Safe-zone convergence and uncredited environmental eliminations. |
| `include/outland/game/economy/Economy.hpp` | Registry, storefront, offer, wallet/stock state and transaction API. |
| `src/game/economy/Economy.cpp` | Catalog validation, purpose-derived stores, exact trade checks and restocking. |
| `include/outland/game/inventory/ItemRegistry.hpp` | Furniture category and safe pickup normalization contract. |
| `src/game/inventory/ItemRegistry.cpp` | Furniture parsing/labels and planar/invalid model handling. |
| `include/outland/game/inventory/LootSave.hpp` | Economy state included in the inventory snapshot. |
| `src/game/inventory/LootSave.cpp` | Atomic V2 persistence, stock validation and V1 migration. |
| `include/outland/game/inventory/LootSession.hpp` | Economy access and reusable combat-aware trade service. |
| `src/game/inventory/LootSession.cpp` | Fresh BR equipment, Explore wallet restore and saved weapon-aware checkout. |
| `src/game/inventory/LootUI.cpp` | Retain short X11 clicks and render planar props safely. |
| `include/outland/game/vehicles/VehicleSystem.hpp` | Fleet checkpoint/reset/restore interface. |
| `src/game/vehicles/VehicleSystem.cpp` | Match-local healthy fleet and persistent fleet restoration. |
| `src/engine/render/Renderer.cpp` | Glue existing systems to sessions, objectives, stores, restart/Home and mode isolation. |
| `tests/integration/modes/test_mode_sessions.cpp` | Difficulty, spawning, starter gear, kills, zone, win/death/restart and result locking. |
| `tests/integration/modes/test_play_ui.cpp` | Portrait/landscape bounds, native touch IDs, focus, difficulty and result/trade controls. |
| `tests/integration/modes/test_explore_flow.cpp` | Civilians, clock/night, police, loot, shopping, weapons and published vehicles together. |
| `tests/integration/inventory/test_economy.cpp` | All items purchasable, transactional safeguards, gun sales/equip, stock, furniture and save migration. |
| `tests/integration/inventory/test_loot.cpp` | Catalog expectations extended while retaining all existing behavior tests. |
| `tests/integration/vehicles/test_vehicles.cpp` | Match damage/placement restoration against the persistent fleet. |
| `tools/generate_interior_catalog.py` | Generate/check all interior DEV entries from the existing item catalog. |
| `tools/audit_loot_assets.py` | Validate the larger catalog, furniture category and planar geometry. |
| `docs/modes-economy-handoff.md` | This handoff and device instructions. |

## Termux apply, build and push

Download the verified `outland-modes-economy-c4f77d9.patch.gz` into `/sdcard/Download`, then run:

```bash
cd ~/Outland &&
git pull --ff-only &&
gzip -t /sdcard/Download/outland-modes-economy-c4f77d9.patch.gz &&
gzip -dc /sdcard/Download/outland-modes-economy-c4f77d9.patch.gz > "$TMPDIR/outland-modes-economy.patch" &&
git apply --check "$TMPDIR/outland-modes-economy.patch" &&
git apply --index "$TMPDIR/outland-modes-economy.patch" &&
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig" &&
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$PREFIX/share/pkgconfig" &&
pkg-config --modversion raylib &&
cmake -S . -B build/dev-raylib6 -DOUTLAND_DEV_TOOLS=ON -DCMAKE_BUILD_TYPE=Debug &&
cmake --build build/dev-raylib6 -j2 &&
ctest --test-dir build/dev-raylib6 --output-on-failure &&
cmake -S . -B build/release-raylib6 -DOUTLAND_DEV_TOOLS=OFF -DCMAKE_BUILD_TYPE=Release &&
cmake --build build/release-raylib6 -j2 &&
ctest --test-dir build/release-raylib6 --output-on-failure &&
git commit -m "Complete BR and Explore sessions with Verda economy" &&
git push origin main
```

`pkg-config` should report raylib 6.x. Do not downgrade raylib or delete existing profiles/maps. If patch checking fails, stop and inspect the reported files; do not force it over your edits. The patch stages only its own files, not downloaded source archives.

## Device inspection

Use your existing Termux:X11 session. With its display running, launch on the phone:

```bash
cd ~/Outland
DISPLAY=:0 LD_LIBRARY_PATH="$PREFIX/lib" LD_PRELOAD= ./build/dev-raylib6/outland
```

1. On Home, cycle BR difficulty and start a match. Confirm the objective, 24 participants, usable loaded weapons/reserve ammo and a town drop. Move outside the indicated zone to check warning/damage. Confirm death placement/kills/difficulty and tap RESTART; equipment and match state should reset. Use HOME to leave. A completed victory uses the same result flow with placement #1.
2. Start Explore. Confirm a daytime populated capital location, free-roam banner, money HUD and a storefront USE prompt. Buy water/bandages, use them through BAG, and sell scavenged goods. Visit garages/clinics/pubs for specialties; general stores sell all catalog items.
3. Buy an interior prop, open BAG and DROP it on clear ground. Pick it up again, or leave it placed; exit through Escape/Home and return to verify cash, bag, depleted store stock and dropped props persist.
4. Buy a gun, reload using its ammo, and check combat still works. Try selling a loaded gun: the sale must be refused. Find a parked hatchback, enter/drive/exit; inventory and store interaction should resume. Witnessed violence should still attract police; dusk/night still changes lighting.
5. Enter DEV Creative Builder. Select interior props under ALL/PROPS, preview/place/select/move/rotate/duplicate/delete them, then SAVE. Use the existing EXPORT action to publish your map. Restart/load to verify the edited world. Personal purchases/drops are separate from the exported world.
6. Start BR, drive/damage a car, then return to Explore/DEV: the persistent fleet should be restored.

Next coworker: device-test narrow-screen HUD/store feel, purpose-marker door approaches in modular shops, circle travel pacing across the coast roads, and the mobile draw budget if players cluster many different dropped furniture models. Extend economic jobs/property only through the existing inventory/life/store APIs and retain atomic wallet+bag persistence.
