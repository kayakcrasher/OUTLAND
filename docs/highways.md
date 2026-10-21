# Highways

Verda's towns are joined by proper divided highways (`world/Highways.hpp`). The ring road circles
the island at 1,500 m. The main highway runs north–south right through Verda City, and two spokes
leave the city east and west:

| Highway | From | To |
|---|---|---|
| north–south | Espera's south edge street (0, −1680), straight down x = 0 through downtown | Suda Haveno's north edge street (300, 1680) |
| east | the Orienta boulevard (476, 0) | Porto Luma's grid (1624, −440) |
| west | the Okcidenta boulevard (−476, 0) | Roka's grid (−1624, 410) |

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

The spokes cross the ring road at grade, at four junctions. The old two-lane links have been
replaced, along with the spurs that ran from the ring road into each hub and doubled the spokes.

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
- **Highway crossings** (ring × spoke). A plain asphalt apron: no lane lines and no barrier for
  `paved_half / sin(angle) + 8` m either way along both roads.
- **Ends** (a spoke running into a town's streets). The last 90 m tapers smoothly from the full
  18.8 m down to 15 m, a little wider than the town's 12 m street. The barrier stops where the
  taper begins, and the last 45 m is plain asphalt with no lane lines.
- **Crossovers.** Out on the open road the median opens for 22 m every 420 m, so cars can turn
  across.
- **Side roads** (farm tracks, hamlet spurs, streets) that join or cross a highway. The median
  barrier opens for the side road's width plus 7 m either side, so traffic can turn across.

## Graded roadbed
The terrain is cut and filled to the highways (`highways::graded`, called from
`TerrainHeight::sample`).

**Profile.** Each line's road profile is built once from the natural land along it:
1. A ±110 m moving average of the land, with ground on building pads weighted 61×.
2. **Holds.** Where a building pad has levelled the land (towns, hamlets, ranches, the stockyard),
   the road sits at the pad's height. Each spoke's ends meet the town's land, level for 24 m.
3. **Grade limit.** No piece is steeper than 7%: neighbours that differ by more meet in the
   middle (a cutting or an embankment), and holds stay put.
4. **Crossings.** Where two highways cross, they share a 24 m level plateau. Its height is as
   close to halfway between the two profiles as both can reach within the grade. Then the grade
   limit runs again.

**Terrain.**
- Within 12.4 m of a centre line (the whole width plus the verge), the land is the road's height:
  level across.
- Over the next 28 m it eases back to the natural land.
- Beyond the roadbed, deep inside a building pad, the pad keeps its own ground.

`outland_highway_tests` checks that every highway stays under 12% grade (the network is built to
7%) and level across (under 4% tilt).

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
- The stadium ground moved 6 m east and north (606, 46), clear of the Porto Luma highway.
- Police stations take a lot only where their patrol cars have a clear run out. Verda City's
  station stands by the north highway again.
- Paul Acosta parks on the hard shoulder of the town-bound carriageway.
- Roadside zombie nests also form along highway pieces in the countryside, at least 500 m apart.
- Welcome signs also go where a highway passes through the edge of a town without ending in it
  (Vojrando).
- The map screen draws the highways in amber.
