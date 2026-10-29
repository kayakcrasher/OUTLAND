# Highways

Verda's towns are joined by proper divided highways (`world/Highways.hpp`). The ring road circles
the island at 1,500 m. Two highways cross the island and run right through Verda City as its main
avenues:

| Highway | From | To |
|---|---|---|
| north–south | Espera's south edge street (0, −1680), straight down x = 0 through downtown | Suda Haveno's north edge street (300, 1680) |
| east–west | Porto Luma's grid (1624, −440) | Roka's grid (−1624, 410), straight through Verda City on z = 0 |

## Through Verda City: the east–west boulevard

The east–west highway is Verda City's main avenue from one side of the city to the other: from the
east suburbs' edge, across downtown (crossing the north–south boulevard at the city's centre), to the
west suburbs' edge. Just outside each edge it curves away to Porto Luma and Roka.

- **Width.** In the city it is 16 m, the avenue it replaced (`boulevard_width`): two lanes each way
  with a planted median, and pavements instead of gravel verges. It widens to the full 18.8 m over
  the 60 m after it leaves the city.
- **The median.** Planted, like the north–south boulevard: a kerb, a lawn and hedges, with plane trees
  and double street lamps between the crossings downtown. Every street crosses it with an opening in
  the median.
- **The stadium.** Verda City's ballpark ground moved south to (606, 76), clear of the curve east
  out of the city.

## Through downtown: the boulevard

Downtown, the north–south highway is Verda City's main avenue: a planted boulevard. Every
downtown street crosses it at grade with signals; the highway opens its median and lays a plain
apron at each crossing.

- **The median:** instead of Jersey barriers, a stone kerb and a lawn with clipped hedges and red
  and yellow flower beds. Plane trees and double street lamps stand down it between the crossings,
  with more trees on the verges. Collision is the same as the barrier, so cars can't cross it.
- **The gateways:** a steel sign gantry spans the highway at each end of the grid
  (`civic/highway_gantry.glb`).
  - A car arriving from either direction reads a green board: BONVENON AL VERDA / CENTRO.
  - A car leaving reads the next town: ESPERA 1,5 km to the north, SUDA HAVENO 1,6 km to the south.
- **Parliament** faces the boulevard from the open block south of the grid, its lawn and steps
  toward the highway.

## The ring's interchanges: the autobahn never stops

The ring road is Verda's autobahn: it never stops, like a giant roundabout itself. Where the two
highways cross it (four times) there is an interchange (`highways::interchanges()`), numbered
clockwise from the north: **1 Espera, 2 Porto Luma, 3 Suda Haveno, 4 Roka**.

- **The bridge.** The ring rides over the crossing on a 100 m bridge (`deck_half` 50), 7.5 m up
  (`overpass_rise`): a concrete deck under the roadway, parapets along both edges, an abutment
  at each end holding back the embankment, and two piers on the island below. The ring climbs to
  it and down again at 7%.
- **The roundabout.** Under the bridge, at the crossing highway's level, a roundabout 40 m across
  its lane (`roundabout_radius`, an 8 m lane), anticlockwise on the map: traffic keeps right and
  turns right into it and out of it. Its island has a kerb and a lawn. The crossing highway runs
  into it from both sides, narrowing over its last 50 m to the entry and exit lanes.
- **Slip roads.** Four per interchange, one way (`RoadType::Ramp`): each carriageway's exit leaves
  the ring 172 m before the crossing, tapers off the outer lane, and runs beside the ring 22 m
  out, below its embankment (a retaining wall between them), into the roundabout; the entry runs
  back up beside it onto the ring 172 m beyond. Forward traffic (the way the ring's points run)
  is inside the ring, the other way outside.
- **Level ground.** The roundabout sits on a level pad at the crossing highway's height (raised
  where a town's levelled ground is close beyond, so the highway can reach it within the grade);
  the slip roads have roadbeds of their own, at the ring's height while still on it, then down
  evenly to the pad.
- **On the deck.** Cars on the ring (and people on foot) ride the deck (`highways::deck`); cars
  on the roundabout beneath drive under it. Barriers and parapets count only at their own level,
  and nothing standing on the ground under a deck is in the way of a car up on it.
- **Moved for them.** Suda Haveno's ballpark moved to (50, 1655), clear of the slip roads; each
  ballpark's plot is kept free from the start of the world build.

Tests: `tests/integration/world/test_interchanges.cpp` (the layout, the deck and the pad, routes
through the roundabout the right way round and on and off the ring by the slip roads, one-way
roads only their way, and a car driving over each bridge both ways).

## Cross-section
From the centre line outwards on each side:

| | width | |
|---|---|---|
| median | 1.0 m | a run of Jersey barriers in the middle, paved either side |
| two lanes | 2 × 3.5 m | `road4.png`: white edge lines, a dashed line between the lanes |
| hard shoulder | 1.4 m | plain asphalt (`road3.png`) |
| verge | 1.8 m | gravel |

A highway road is 18.8 m wide (paved), `RoadType::Highway`. The textures and the barrier
(`Cones & Barriers/Concrete Barriers/Jersey_barrier.glb`) come from the city asset pack. They are
drawn by `engine/render/HighwayRender` and recorded into static meshes like the other roads.

## Curves and joins
- The ring is a true circle in 65 m pieces (2.5° apart).
- The spokes' corners are filleted into circular arcs of up to 240 m radius, sampled every 3°.
  Straights are split into pieces of at most 60 m.
- Consecutive pieces are mitred at their joins (`world/roads/HighwayNetwork`), so a curve is one
  seamless ribbon.

## Junctions
`HighwayNetwork` finds the junctions in the map's roads:
- **Highway crossings** (the two highways at Verda City's centre). A plain asphalt apron: no lane
  lines and no barrier for `paved_half / sin(angle) + 8` m either way along both roads. (Where
  they cross the ring there are interchanges instead, above.)
- **Into a roundabout** (an interchange's). The last 50 m narrows to 15 m, the barrier stops, and
  the last 12 m is plain asphalt.
- **Ends** (a spoke running into a town's streets). The last 150 m tapers smoothly from the full
  18.8 m down to 13.2 m, just wider than the town's 12 m street, so the highway blends into the
  street instead of stepping down. The barrier stops where the taper begins, and the last 45 m
  is plain asphalt with no lane lines.
- **Crossovers.** Out on the open road the median opens for 22 m every 420 m, so cars can turn
  across.
- **Side roads** (farm tracks, hamlet spurs, streets) that join or cross a highway (not slip
  roads, which join a carriageway). The median barrier opens for the side road's width plus 7 m either side, so traffic can turn across.

## Graded roadbed
The terrain is cut and filled to the highways (`highways::graded`, called from
`TerrainHeight::sample`).

**Profile.** Each line's road profile is built once from the natural land along it:
1. A ±110 m moving average of the land, with ground on building pads weighted 61×.
2. **Holds.** Where a building pad has levelled the land (towns, hamlets, ranches, the stockyard),
   the road sits at the pad's height. Each spoke's ends meet the town's land, level for 24 m: the
   ground 24 m on into the town's streets, not the end point itself (often still part way up the
   hill on the edge of the town's pad, which left a step of up to a metre down onto the streets).
3. **Grade limit.** No piece is steeper than 7%: neighbours that differ by more meet in the
   middle (a cutting or an embankment), and holds stay put.
4. **Crossings.** Where two highways cross, they share a 24 m level plateau. Its height is as
   close to halfway between the two profiles as both can reach within the grade. Then the grade
   limit runs again.

**Terrain.**
- Within 12.4 m of a centre line (the whole width plus the verge), the land is the road's height:
  level across.
- Over the next 28 m it eases back to the natural land.
- Beyond the roadbed, inside a building pad, the pad keeps its own ground past a bank by the
  shoulder three times as wide as the road is above or below it (at most 10 m), so there is never
  a step at the roadbed's edge.

`outland_highway_tests` checks that every highway stays under 12% grade (the network is built to
7%) and level across (under 4% tilt).

## One-way roads

Two road types are one way, start to end: `RoadType::Ramp` (a slip road onto or off a highway) and
`RoadType::Roundabout` (a piece of a roundabout's circulating lane). The road graph
(`world/roads/RoadGraph`) routes over (edge, end) states, so traffic:
- drives a one-way road only its way, in the middle of it;
- never turns back along the road it has just driven;
- joins and leaves a ramp only by turning right or going straight on (never left across a highway's
  median).

## Traffic and collision
- **Traffic.** Traffic drives on the right, in the outer (slow) lane, 6.25 m from the centre line
  (`RoadGraph::lane_of`). A route eases between lane offsets over 16 m either side of a corner, for
  example from a highway onto a street, so cars no longer jump sideways at a junction.
- **The median barrier.** It stops cars (`WorldCollision::vehicle_blocked`). People on foot climb
  over it. Its runs are indexed on a 32 m grid.
- **Testing.** `outland_highway_tests` drives the route between every pair of towns and checks
  that no point comes within a car's half-width of the barrier.

## What moved with it
- The ranch gates moved out to the new verge, 12.5 m from the centre line (`world/Ranches.hpp`).
  The stockyard gate moved too.
- The stadium ground moved to (606, 76), clear of the east-west highway's curve out of the city.
- Police stations take a lot only where their patrol cars have a clear run out. Verda City's
  station stands by the north highway again.
- Paul Acosta parks on the hard shoulder of the town-bound carriageway.
- Roadside zombie nests also form along highway pieces in the countryside, at least 500 m apart.
- Welcome signs also go where a highway passes through the edge of a town without ending in it
  (Vojrando).
- The map screen draws the highways in amber.
