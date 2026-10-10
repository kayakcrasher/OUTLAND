# Zombie ecology

Zombie Survival's infected live island-wide (`game::zombies::ZombieEcology`), on two tiers like island life:

- **Abstract:** every zombie is a cheap record of where it is and what it is doing.
- **Physical:** zombies near the player become NpcSystem creature bodies. They see, chase and bite using the existing creature behaviour.

In Verda's lore the island's strangeness is what keeps the infected going. They are part of the forgotten experiment, like everything else.

## Where they come from

- **Nests:**
  - authored Zombie Spawn markers;
  - nests around every town: 8 around the capital and 4 around each other town, or 6 if the town is marked infected. Nests are at least 25 m apart and always on standable ground;
  - up to 10 roadside nests along long roads (250 m or more) between towns.
- **Population:** 180 infected, spread across the nests, each with a creature model.
  - A build is deterministic for a given map and seed. On the shipped map that gives 34 nests, 10 of them roadside.

## Day and night

| | Day | Night |
|---|---|---|
| Behaviour | Many lie dormant; the rest shamble near their nest | Everyone is awake |
| Body speed | 1.8 m/s | 3.0 m/s (2.4 at dusk and dawn) |
| Sight | 30 m | 18 m |
| Hearing | Normal | ×1.4 |

- **Hordes form at night:** up to 2 at once, with a new attempt every 25 s.
  - They walk the road graph between towns at 1.3 m/s, and about half of them head for the player's town.
  - They break up at dawn or when they reach the end of their route.
- A HUD line warns **A HORDE IS MOVING NEARBY** within 400 m.

## Noise

Sounds on the SoundBus send any zombie that hears them to investigate the source, with some scatter: gunshots, sprinting, engines and horns. A gunshot can also turn a horde toward it.

## Bodies

- **Bodies appear and vanish by distance:** they appear within 85 m and are released beyond 120 m. At most 14 exist at once, and the closest zombies get them first.
- **Combat:**
  - Bodies have 120 health and bite for 12 every 1.2 s.
  - They hunt with the creature AI. When they aren't hunting, the ecology steers them toward their wander, investigate or horde targets.
- **The HUD shows** `INFECTED NEAR n | KILLED k`.
- **Death and respawn:**
  - A killed zombie counts once, and its corpse stays while you are near.
  - The corpse is cleared once you leave.
  - Its nest sends a replacement after 90 s, and only while the player is more than 90 m from that nest.

## Safe start

At the start of a match, infected within 70 m of the player's spawn move to other nests.

## Tuning

- Population, ranges, horde settings and speeds are in `ZombieConfig`.
- Body speed, sight, health and bite per time of day are set in `ZombieEcology::tune()`.
- After a gunshot, a player who stands still in a town will be overrun within about 20 s. That is intended pressure; soften the bite or the night speed there if needed.

## Tests

`outland_zombie_ecology_tests` checks:
- nests in every town and roadside nests;
- builds are deterministic;
- dormancy and slow bodies by day;
- hordes at night, staying on the roads, with faster bodies;
- hordes break up at dawn;
- gunfire draws in only the zombies within earshot;
- the safe start;
- bodies stay within the budget and bite;
- a kill counts once and the corpse stays, then is cleared;
- respawn;
- `clear()` removes every body.
