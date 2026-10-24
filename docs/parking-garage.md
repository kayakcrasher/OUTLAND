# Parkumejo Centra and the sky mansion

Downtown Verda City's multi-storey car park, in the south-west corner block (around x −129,
z −126), its entrance on the inner street to the north. Model: `assets/verda/city/civic/parking_garage.glb`,
written by `tools/generate_city_kit.py` (`parking_garage()`); placed by the world kit (`downtown()`,
the south-west parking block) into `maps/verda_world.map`. Layout constants: `world::garage`
(`include/outland/world/ParkingGarage.hpp`).

## The garage
- **38 x 32 m, four levels you drive up:** the ground floor (0.1 m), deck 1 (3.3 m), deck 2 (6.5 m)
  and the roof (9.7 m), 2.9 m of headroom on the covered levels.
- **Ramps:** two lanes on the east side (6.4 m wide, 10°), zig-zagging up. Lane A takes you from the
  ground floor to deck 1, and from deck 2 to the roof; lane B from deck 1 to deck 2. At the top of
  each ramp you turn round in the aisle at that end to reach the next one. The ramps are solid, with
  a wall between the lanes and yellow-capped rails round the openings; a hazard strip marks where
  each ramp meets its deck. They go down the same way.
- **Art Deco dress:**
  - limestone parapets between the columns, with two gold speed lines along every level, on a dark granite plinth;
  - stepped limestone pylons with gold caps and gold fins at the four corners;
  - a stepped sign tower on the north-west corner carrying the blue **P**, with gold fins down its face.
- Lamps under every deck; painted bays on the west side. Seven cars are parked on the levels (anyone can take one; the island's drivers leave
  them be). A height bar over the entrance.
- Big rigs don't fit under the decks.

## How cars drive on it
Cars normally ride the terrain. Over the garage (`VehicleSystem::bind_surface`, bound by the game)
a car rides its decks and ramps instead: the highest floor of the model within a metre above its
wheels (`WorldCollision::ground_height`). Spawning, parking (`place()`), the slope and body-tilt
checks and getting out all use the same surface, so a car left on a deck - one of yours, saved -
comes back on that deck. Getting out is checked against the real shape of the building, not its
bounding box.

The garage (and Verda Transporto's truck depot, which had been solid to vehicles) are drive-in
buildings: a car meets their real columns, walls and ramps (`WorldCollision`).

## The mansion (Explore)
On the roof's west side stands a **three-storey Art Deco mansion**, the little sister of Stela Turo:
- dressed limestone on a granite plinth, tall tinted windows with gold sills;
- piers between the window bays that rise past the roof with gold caps;
- bronze chevron spandrels between the floors;
- a gold door surround with a sunburst fan over the door, under a bronze marquee on gold posts;
- bay trees in gold urns at the door;
- on top, a stepped crown over the entrance bay, gold fins up each step and a gold spire.
- Inside: a living room on a black-and-white marble floor (a teal velvet sofa, a striped red-and-gold
  rug and a coffee table, a kitchen bar), the dining floor above, the master bedroom on top (a bed with a gold headboard).
  Stairs (0.2 m risers) climb the east wall.
- **For sale: 60,000 kronoj** (`game::economy::SkyMansion`). Walk in and E / USE buys it; the prompt
  shows the price. Kept with the Explore wallet as a property.
- Once it's yours:
  - **Sleep**: E / USE by the bed sleeps until 7:00 and mends you.
  - **Wake up at home**: if it's the home you bought last, you wake there when you die.
  - **Your own parking bay**: the gold-lined bay right in front of the door, with gold bollards.
    Stop in it and the game says it's yours; a car you leave there stays on the roof.
    Anyone else stopping there is told it's PRIVATA.
  - The compass and the map mark it **HOME** (before then the map shows it for sale).

## Tests
`outland_parking_garage_tests` (`tests/integration/vehicles/test_parking_garage.cpp`), on the
published map:
- the garage is placed, and the parked cars stand on their decks;
- a car driven in at the entrance climbs the first ramp to deck 1;
- a car parked in the mansion's bay stands on the roof and stays there;
- the surface is the terrain anywhere else;
- the mansion's floors and its sale;
- a rig drives into the truck depot's service bay.
