# Verda's suburbs and Porto Luma

Built by `outland_build_world` (`src/dev/VerdaWorldKit.cpp`) into `maps/verda_world.map`.

## Downtown Verda City
- **Streets:** 16 m wide (`downtown()` in the world kit).
  - Four avenues at x = ±87 and ±171, and the north–south highway down x = 0 as a planted boulevard ([highways](highways.md)).
  - Five streets at z = 0, ±84 and ±168.
  - Sixteen 68 m blocks; the grid reaches |x| 179, |z| 176.
- **South row:** one more row of blocks beyond the grid. From west to east: the apartments, the office plaza, Parliament (beside the highway) and the corner-bar block where La Verda Mano keep their hideout. A street runs along their far side.
- **Parliament** (Registaro de Verda) is a working building: twenty staff work in its great hall under the dome (island life), and its name, REGISTARO DE VERDA, runs across the portico's entablature over the columns.
- **The square:** Verda City's square, where residents meet and loiter, is the courthouse park block (north-west of the boulevard's crossing, around x −45, z 42). The city centre itself is the boulevard's crossing, so people no longer crowd the street there.
- **Level ground:** the terrain is levelled under the whole of it (`layout::downtown_outside`).
- **Traffic:** keeps right, 2.4 m off the centre line. Parked cars sit tight to the kerb.

## Okcidenta Verda (west suburbs)
- Downtown's east–west streets carry on west to x = −476; cross streets at x = −330 and −476.
- The Roka highway now leaves from the west end of a 10 m boulevard on z = 0.
- 88 detached homes (single-storey ranch and two-storey), procedural and enterable, owned by the
  capital, on 24 m lots set 14 m back from the street. Yard trees, some picket fences, bins, a car
  on about a quarter of the drives.
- On the boulevard: a park, two shops, a church and a clinic.

## The industrial quarter
West of Okcidenta, south of the Roka highway, on level ground (`layout::freight_*`): the z = −84
street runs on into the yard of **Verda Transporto**, the trucking company. See [trucking](trucking.md).

## Orienta Verda (east suburbs, the fancy side)
- Same street plan mirrored east (to x = 476). The Porto Luma highway leaves from the boulevard's
  east end.
- 72 houses from the residential house model on 30 m lots, set 21 m back, in four paint schemes.
  Wrought-iron front fences on some lots, two lawn trees each, a car in front of a third of the
  garages.
- A tree-lined boulevard with tall lamps, a park with benches, and a brick clubhouse (pub).

## The house model
`assets/verda/residential/houses/source/house.glb` is the artist's whole neighbourhood scene
(255k vertices, textures lost on export), kept out of the APK and web build by the `source`
folder. `tools/extract_house.py` takes one house from it, puts it on the ground centred on its
footprint (porch toward −Z) and paints its materials with flat vertex colours. It writes
`house_white/blue/sage/cream.glb` (about 9,000 vertices, 290 KB each); `--check` verifies them.

The houses are enterable. The front door panel beside the garage is left out, so its opening in
the wall is the doorway (the sliding door off the porch is open too). Inside are:
- plaster walls and a wood floor, with a ceiling at first-storey height;
- a living room (sofa, rug, coffee table, armchair, TV) and a dining table with four chairs;
- a kitchen along the back extension;
- a garage behind a partition with a doorway (just inside the front door), holding a workbench and shelves;
- seventeen steps up from the hall, beside the garage, to a carpeted upper floor at 3.3 m;
- upstairs: a hall with a rail round the stairwell, a master bedroom over the garage (double bed,
  wardrobe, nightstand, rug), a front bedroom (double bed, dresser), a back bedroom (two single beds,
  desk and chair) and, through it, a bathroom in the back extension (bath, toilet, basin).

The model was built to be seen from outside, so `extract_house.py` also cleans up the inside of the
upper storey. A ray test decides which faces look into the house: inside faces are painted plaster
(walls) or ceiling (roof undersides), and upper-storey and roof faces get a painted back face, so
no gable shows sky from within. Upstairs partitions are 2.1 m tall and stop where the roof comes down.

Collision uses the same triangles. Each house is reported as a two-storey enterable building: the
world-kit test walks in through every front door, climbs every staircase, and reaches every bedroom
and bathroom. Each house also carries a Home purpose marker, so island life houses a family in each;
the capital's population grows by three per house.

## Porto Luma
- A 4 × 4 grid of 12 m streets (avenues and streets at ±40 and ±120 from the centre) around
  Placo Luma, the central square. The highway from the capital stops at the grid's west edge on
  the street north of the square (it crosses the ring road on the way; see [highways.md](highways.md)).
- Main Street rows, a glass tower and office block, corner mid-rises, a car park, signals, street
  trees and lights, and parked cars.
- Waterfront east of the grid: quay roads, a harbour office, a dock building,
  pallets and barrels, and a line of bollards.
- The cargo terminal (Kargohaveno) below the town: a ramp from the quay road down to a quay at the
  shore, with Stella Maris at the berth, two container cranes, and the cattle quay. See
  [trucking](trucking.md).

## Espera and Suda Haveno
The two county towns are built to the same city plan as Porto Luma and Roka by
`county_town` in the world kit. It replaces the old villages: their houses, lanes, field walls
and trees go, and the old village loot moves to the square.

**The plan**
- A 4 × 4 grid of 12 m streets: avenues and streets at ±40 and ±120 m from the centre.
- Nine 68 m blocks, with the square in the middle and Main Street rows facing the square and the
  streets behind it.
- Signals at the four inner crossings, street lights and trees round every block, and cars
  parked tight to the kerb on the streets round the square (clear of the bus lane).
- The highway from the capital ends on the grid's edge street: Espera's south edge at
  (0, −1680), Suda Haveno's north edge at (300, 1680).

**The blocks** (bx, bz are a block's offset from the centre)

| | Espera, the farm town | Suda Haveno, the harbour town |
|---|---|---|
| square | Placo de Espera, with the old village windmill | the square |
| yard | the farmers' co-op (−80, 80), by the highway: a red barn, grain bins, a brick feed store (shop), hay bales, a water trough and the farm truck | the fish market (80, 80), on the sea side: a stucco market hall (dock), pallets and barrels |
| flats | (−80, −80), four blocks of flats, each a home | (−80, 80) |
| corner mid-rise | (80, −80) | (80, −80) |
| car park | (80, 80) | (−80, −80), by the highway |
| everything else | Main Street rows | Main Street rows |

Every home in both towns is lived in. The police station and town hall find lots just outside
the grid.

## Terrain
`TerrainHeight` levels both suburbs (|x| 150–485, |z| ≤ 190) with downtown. Porto Luma's,
Espera's and Suda Haveno's building pads are 205 m in radius (up from 85 m).

## Roka, the industrial city
Rebuilt on Porto Luma's plan (`roka_city` in the world kit): a 4 × 4 grid of 12 m streets (±40, ±120
from the centre) round Placo de la Laboristoj, on a level pad 220 m in radius.
- North and south of the square: Main Street rows (shops, a pub, rooms above).
- West blocks: worker flats (eight prebuilt brick and concrete blocks, each a home) and, between
  them, the town hall and the clinic facing the square.
- East blocks: yards on asphalt - hangars, a two-storey factory, trucks, pallets.
- West of the grid, along the works road: the works - a long factory hall and a second hall,
  a hangar, pipe runs, barricades and a chainlink-fenced stock yard.
- The highway from the capital ends at the grid's east edge (street z = −40); it crosses the
  ring road on the way (see [highways.md](highways.md)).
Every home in the city is lived in (no population cap).

## Sovaĝejo
Now north of Roka, at (−1790, 90), on its own pad: the trailer park's lane runs north–south and a
dirt road carries on from its gate down to the end of Roka's west avenue, with the six homesteads
strung along it. All of its roads are dirt, and Linio 1 no longer calls there.

## Highway hamlets
Besides Ruldomaro, three small trailer stops by the highways, each on a level pad with a gravel lane
square to the compass and a gravel spur onto the highway: six trailers (homes), a general store, a
shed, a fire pit, lights, a fence, loot and a couple of cars. No bus stop.
- **Vojrando** (1312, −239) on the Porto Luma road;
- **Pinejo** (−965, −1017) on the north-west ring road;
- **Sablovalo** (−909, 1077) on the south-west ring road.
They have two zombie nests each (towns have four). Runtime vehicles are capped at 144
(`VehicleSystem::max_vehicles`; the map has 130 markers).
