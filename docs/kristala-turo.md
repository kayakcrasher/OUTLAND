# Kristala Turo and its penthouse

Verda's tallest building: a blue glass skyscraper on the block east of the highway at the foot of
Norda Kvartalo (around x 52, z −214), its front and its plaza to the south, toward downtown. Model:
`assets/verda/city/towers/kristala_turo.glb`, written by `tools/generate_city_kit.py`
(`kristala_turo()`); placed by the world kit (`sky_tower()`, right after `north_quarter()`) into
`maps/verda_world.map`. Layout constants: `world::tower` (`include/outland/world/SkyTower.hpp`).

## The tower
- **26 x 26 m, 118.7 m to the tip of its spire:** a 6 m lobby, 23 office floors of 3.6 m (the top
  three set back to 23 x 23 m), the penthouse floor at 89 m, a lantern and a spire over it.
- **The plaza:** granite planters and benches either side of the way in, a canopy over the doors and
  the tower's name on its offices' board over them (KRISTALA TURO · OFICEJOJ · PENTDOMO).
- **The lobby is open to everyone:** glass on three sides, a marble floor, the concierge's desk, benches
  and plants, a service door at the back. The tower is a place of work on the island (offices).
  It is on the enterable list, so the world-kit test walks in through its front door.
- **The elevator core** faces the way in, with three brass doors. Stand at them and press **E / USE**:
  "Elevator up to the penthouse (25)". The ride takes a few seconds behind a fade, with the floor counting
  up; you step out at the penthouse's elevator door. Up top, the same key takes you back down to the lobby.

## The penthouse
- **A glass suite on the 25th floor:** a living room (sofas, a coffee table), a kitchen island, a
  dining table, and the bedroom behind a partition by the core. Its glass front opens onto the terrace.
- **The terrace runs all round** behind a glass balustrade with a steel rail (you can't fall off). There
  is a pool and two loungers on the front terrace, looking south over downtown.
- **For sale for 100000 kronoj** (`game::economy::SkyPenthouse`). Up there the HUD says
  "PENTHOUSE FOR SALE - 100000 kronoj"; press E / USE to buy it. The map shows it as a for-sale mark
  until you own it.
- **Once it is yours** it is home: the compass shows HOME, and E / USE by the bed sleeps till morning.
  Ownership is kept with the wallet (`EconomyState::properties`, id `kristala_turo_penthouse`), so it
  survives saves.

## Tests
`outland_sky_tower_tests` (`tests/integration/world/test_sky_tower.cpp`) covers:
- the price, and that it is bought once;
- that the tower is on its site and taller than any other model on the island;
- the walk off the plaza through the doors to the elevator, and the lobby's walls;
- the elevator's call points;
- the level penthouse floor;
- the walk out past the sofa, between the pool and the loungers;
- the balustrade stopping you at the edge.
