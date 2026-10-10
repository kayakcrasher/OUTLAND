# OUTLAND: one island, three ways to live on it

The three player-facing modes are **interpretations of the same Verda**, not three games. They share the same roads, towns, buildings, cars and weapons. DEV/Creator stays backstage as the workshop used to build Verda. `game/ModeRules.hpp` is the code-side statement of this. Each mode switches layers on; none forks the world.

| Layer | Explore | Zombie | Battle Royale |
|---|---|---|---|
| Civilian life (homes, jobs, Sundays) | ✅ `game::life` | survivors only (~20) | suspended |
| Factions | Police, Krimulo | Green Star Federation, Esperantiza families | — |
| Zombie ecology | — | spawn sources, hordes, noise | — |
| Ranged combat bots | police / Krimulo later | survivor patrols | dozens of opponents |
| Ammunition | normal | scarcer (`0.65`) but still fun | normal |

## Explore: the soul of OUTLAND (first layer done)

Explore is living on Verda. It is not "GTA mode" or "The Sims mode". You can drive, own property, earn and spend, take jobs, run into crime, help people, deal with factions, explore Cold War installations, wander into church on Sunday, visit Roka at night, or just watch island life. **NPCs make or break it.**

Done: the civilian life layer. Every resident has a home, job, hours, lunch, a pub, relatives and a Sunday routine. Distant residents cost almost nothing; nearby ones become bodies. Fear interrupts the schedule and life resumes afterwards. See [verda-life-handoff.md](verda-life-handoff.md).

Next, roughly in order:
1. Authored building purposes in the map format, so Creator decides where the church and pub are.
2. Residents drive cars on the road graph.
3. Day/night lighting from `WorldClock`.
4. Witnesses → police response; Krimulo night activity that the player can stumble into.
5. Money, jobs and property for the player, plus a save file for island state (clock, deaths, ownership).
6. Conversations and simple services: buy in Porto Luma, hire, help.

## Battle Royale: Verda with civilisation suspended

Same island; the player drops in with dozens of AI opponents; loot, move, use vehicles, take cover, hunt; one remains. The world matters tactically:

- **Roka:** industrial cover and warehouses.
- **Porto Luma:** docks and long sightlines.
- **Espera:** rural houses and fields.
- **Suda Haveno:** residential.
- **The capital:** dense and dangerous.

**Next AI focus: ranged combat intelligence.** Bots must understand weapons, cover, windows, vehicles, sound, distance, ammo, healing, retreating, repositioning and when to push, instead of sprinting at the player. Proposed shape: a `game/ai/combat` brain above `NpcBehavior`, mirroring how `game::life` sits above it.

- **Perception without cheating:**
  - sight uses the existing `CombatWorld::trace_segment` (cover and windows already block or pass bullets);
  - hearing uses a gunshot/footstep/vehicle event bus with distance falloff;
  - memory is a last-known position that decays.
  - Bots never read the player's true position unless they perceive it.
- **Weapon model:** bots use the same `WeaponDefinition`s with magazines, reloads and spread. Range preference comes from weapon (pistol close, rifle mid, Barret long).
- **Tactical options**, scored each 0.25–0.5 s:
  - engage, take cover (cover points sampled from building walls, window openings, vehicles, terrain);
  - reload or heal behind cover, reposition or flank, retreat when outgunned or low;
  - push when the target is reloading, wounded or lost; investigate sounds; loot; drive.
- **Difficulty** changes reaction time, aim error and aim settle time, decision interval, the chance of picking the best-scored option, and aggression. Health, damage and knowledge **stay identical** at every difficulty.
- **Map awareness:** precompute per-town cover/sightline data (warehouses, dock lines, fields).
- **Performance:** the same two-tier trick as life. Distant bots fight abstractly, by sector and probability, until the player is near.

## Zombie: Verda after the catastrophe

This is not an arcade wave shooter. Esperantiza families, hardy islanders, try to preserve their communities. Roughly twenty survivor/faction NPCs form the living core while zombies spread through the same towns and countryside.

- **Objective:** clear the island's zombie spawn sources, not survive forever.
- **Supplies:** food matters. Ammunition is harder to find but still plentiful enough that shooting stays fun.
- **Factions:** the Green Star Federation is the organised faction. Ordinary families, isolated survivors, abandoned houses, Cold War installations and bunkers make the island feel inhabited after collapse.

Next AI: zombie ecology and survivor behaviour. This includes infected spawn regions (the `ZombieSpawn` markers and `SettlementState::Infected/Overrun` already exist in data), noise attraction from the same sound bus BR needs, hordes, survivor shelters, scavenging, defensive positions, faction patrols, rescues, and settlements that become safer as nearby threats are cleared.

`game::life` is reused here: survivors are residents with a shrunken schedule (shelter, scavenge, guard, sleep) and their own homes and relatives. That keeps them people rather than spawn points.

## Shared groundwork worth doing once

- **A sound event bus** (gunshots, engines, footsteps, doors). It is needed by Explore fear, BR hearing and Zombie attraction. `report_gunfire` is its first consumer.
- **Navigation:** a door/road waypoint graph per town. Collision-only steering is the shared weak point of all three modes.
- **Real body animation clips** (see `npc-ai-handoff.md`).
