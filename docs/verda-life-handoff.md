# Verdan civilian life: first simulation layer (Explore)

Base: `75f9a4a` (DEV builder repair + world export). No save formats, assets, Creator data or mobile controls changed.

## What it is

Explore now has residents. Each one has a name, home, household, occupation, workplace, regular pub, church, shop, relatives and a seven-day schedule. A Roka factory worker leaves home in the morning, eats lunch at the pub nearest work and goes home or to the pub in the evening. Porto Luma dockworkers start at 05:30. Shopkeepers work Saturdays. The publican is off on Mondays. The pastor runs Sunday service. Police work three shifts and patrol an assigned town. Some Roka/Porto Luma workers are Krimulo, with normal day jobs and Friday/Saturday nights at a Roka warehouse.

The life layer sits **above** the existing NPC state machine. `NpcBehavior` still owns Idle/Wander/Alert/Chase/Attack/Flee/Dead. Life only moves the anchor an NPC wanders toward.

```
game::life::LifeSimulation           (every resident, cheap, no pathfinding)
   │  abstract tier: schedule block → place → straight-line trip in island minutes
   │  physical tier: nearest residents become NpcSystem actors (max 24)
   ▼
characters::NpcSystem / NpcBehavior   (bodies: Idle/Wander/.../Flee/Dead — unchanged states)
```

### Abstract tier: everyone, always

A resident is just `schedule + current place + {from, depart, arrive}`. Their position is computed directly from the clock: a lerp along the trip, then the place's interior or doorway. Re-planning is round-robin, so the whole island is revisited every 0.5 s of real time. In the Debug build, **800 residents cost ~0.006 ms/frame** on the desktop test machine. The phone has not been profiled. Trips use island time: walking for ≤500 m, otherwise "drove/caught a lift" (4 min + 11 m/s).

### Physical tier: only near the player

- Outdoor residents get bodies within 110 m and lose them beyond 150 m. NPC drawing still stops at 100 m, so the swap is never on screen.
- Residents indoors only get bodies when the player is within 18 m of the building. Nobody is simulated sleeping in a house you aren't standing next to.
- The cap is 24 bodies, nearest first.
- A body walks to its anchor with the existing collision. It leaves a building through its doorway, walks to the target's doorway, then goes inside. There is still no navmesh: a straight line from door to door can stall on a wall.
- On despawn, the body's exact position becomes the start of the abstract trip, so nobody teleports home.
- People step outside in 20-minute spells: smoking outside the pub, yard work at the garage, Krimulo loitering outside the warehouse at night. This keeps streets alive without simulating interiors.

### Interruption and resumption

- If a body flees (it saw the player shooting, or it was hurt), its resident switches to **Shelter**: it goes home for 150 island minutes and hurries (runs).
- `report_gunfire` reaches **every** resident within 260 m, with or without a body, so a gunfight in Roka empties the street. Espera, 2 km away, never hears it.
- While the body is still fleeing, the shelter is extended. Once the threat memory expires, the body walks home. When the shelter time ends, the normal schedule resumes.
- A death lasts for the rest of the session. The dead never respawn.

### Where places come from

Places are derived from the real `VerdaRegion` buildings, so Creator edits change who lives where. The game rebuilds residents every time Explore starts.

- **By building style:**
  - Garage becomes a garage.
  - Warehouse becomes the docks in Porto Luma and works elsewhere.
  - Shop becomes a shop with a flat above it.
  - Houses become homes (two-storey buildings are 8-person apartments).
- **Civic buildings:** each town gets a church and a pub from the houses or shops nearest its centre. The capital also gets a police station, clinic, town hall and shop.
- **Working towns:** in Porto Luma and Roka, the surplus shopfronts become worker tenements, because the generated foundations have no houses there.
- **Outdoor places:** each town gets a square, and Espera gets fields.
- **Overrides:** a building whose id contains `church`, `chapel`, `pub`, `police`, `clinic`, `hall`, `dock`, `garage`, `shop`, `factory` or `home` keeps that purpose. Real authored purposes belong in the map format next (see below).

**Population:** each town fills its homes up to `Settlement::survivors`, or up to capacity when that is 0. With the default coastal layout, that gives 100 residents. `PopulationOptions::density` scales it, and the test uses `density=8` for 800 residents. Uniforms follow occupation: police use `emergency_police` models and doctors use `emergency_doctor`.

### Explore HUD

The island clock appears top-centre (`MON 07:30`; the default is 30× real time, so a day lasts 48 real minutes). Standing within 3.5 m of a resident shows their name, job and what they're doing, for example *"Petro Ostrova, mechanic - working at the garage in Espera"*. Krimulo membership is deliberately not shown.

## Mode rules

`game/ModeRules.hpp` records that the modes are interpretations of one island. It turns on `civilian_life` (Explore), `survivor_factions` + `zombie_ecology` + `ammo_scarcity 0.65` (Zombie), and `combat_bots` (Battle Royale). Only `civilian_life` is consumed today. The rest are named switches for the next passes, not implemented behaviour.

## Known limits / next steps for Explore

1. **Authored building purposes.** Add a `purpose` field to buildings in the Creator map format (V6), with a Creator picker. The keyword and centre-distance designation is a placeholder.
2. **Residents use cars.** Long physical trips are still on foot until the body leaves the radius. `VehicleSystem` already exists; a commuter should walk to a parked car and drive the road graph.
3. **Night.** The clock exists, but lighting doesn't follow it yet. Roka at night needs a sky and lighting pass driven by `WorldClock::hour()`.
4. **Witnesses and police response.** Gunfire already reaches every resident. Next: witnesses report to the police faction and on-shift officers converge (Police should get ranged combat once the Battle Royale combat brain exists).
5. **Persistence.** The clock, deaths and resident state are session-only, like NPC health today. Ownership, money and jobs need a save file.
6. **Animation.** Bodies still slide in bind pose (see `npc-ai-handoff.md`). Hurrying residents use the Walk clip at run speed.

## Validation

- `tests/integration/life/test_life_simulation.cpp` (CTest `outland_life_tests`) covers:
  - mode rules, clock and weekday wraparound;
  - door geometry against the collision frame;
  - deterministic population; every occupation, Krimulo and relatives present;
  - uniforms and dockworkers at the docks;
  - weekday, Saturday, Sunday, early-shift and Krimulo schedules;
  - all workers at work by 10:00 with nobody near;
  - an inter-town commuter in transit, then home;
  - body cap and nearest-first spawning; marker reconciliation keeping residents; despawn;
  - a resident physically walking out of their house and in through the garage doorway with real `WorldCollision`;
  - flee → shelter → resume; gunfire radius; hurrying; permanent death; session reset;
  - directed travel past the old 6 s wander timeout;
  - 800-resident abstract update under 1 ms/frame.
- DEV Debug: **35/35** CTest. Release (`OUTLAND_DEV_TOOLS=OFF`): **30/30**. ASan+UBSan: life, NPC AI and combat suites clean.
- Built against raylib 6.0 compiled headless (`PLATFORM=Memory`) in the cloud workspace. **The game was not launched.** No phone FPS or visual check has been done yet.

## Changed files

| File | Change |
|---|---|
| `include/outland/game/ModeRules.hpp` | Per-mode layer switches for the shared island. |
| `include/outland/game/life/LifeTypes.hpp`, `src/game/life/LifeTypes.cpp` | Clock, places, occupations, factions, activities, resident record. |
| `include/outland/game/life/LifePopulation.hpp`, `src/game/life/LifePopulation.cpp` | Places from buildings, households, jobs, factions, relatives, weekly schedules. |
| `include/outland/game/life/LifeSimulation.hpp`, `src/game/life/LifeSimulation.cpp` | Abstract/physical tiers, door routing, shelter, gunfire, descriptions. |
| `include/outland/characters/NpcSystem.hpp`, `src/characters/NpcSystem.cpp` | Resident-owned actors (spawn/find/direct/despawn), kept across marker reconciliation, removed on session reset. |
| `src/characters/NpcBehavior.cpp` | Directed actors travel to a far anchor without the wander timeout, linger 1–6 s, and run when hurrying. Marker NPC behaviour is unchanged. |
| `src/engine/render/Renderer.cpp` | Build/reset life on entering Explore, feed gunfire, update after NPCs, clock and resident HUD. |
| `CMakeLists.txt` | Life sources and `outland_life_tests`. |
