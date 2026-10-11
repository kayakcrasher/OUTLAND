# Verda's suburbs and Porto Luma

Built by `outland_build_world` (`src/dev/VerdaWorldKit.cpp`) into `maps/verda_world.map`.

## Okcidenta Verda (west suburbs)
- Downtown's east–west streets carry on west to x = −476; cross streets at x = −330 and −476.
- The Roka highway now leaves from the west end of a 10 m boulevard on z = 0.
- 88 detached homes (single-storey ranch and two-storey), procedural and enterable, owned by the
  capital, on 24 m lots set 14 m back from the street. Yard trees, some picket fences, bins, a car
  on about a quarter of the drives.
- On the boulevard: a park, two shops, a church and a clinic.

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
`house_white/blue/sage/cream.glb` (about 5,500 vertices, 183 KB each); `--check` verifies them.

The houses are enterable. The front door panel beside the garage is left out, so its opening in
the wall is the doorway (the sliding door off the porch is open too). Inside are:
- plaster walls and a wood floor, with a ceiling at first-storey height;
- a living room (sofa, rug, coffee table, armchair, TV) and a dining table with four chairs;
- a kitchen along the back extension;
- a hall sideboard, and a garage behind a partition with a doorway, holding a workbench and shelves.

Collision uses the same triangles. Each house is reported as an enterable building (the world-kit
test walks in through every front door) and carries a Home purpose marker, so island life houses
a family in each; the capital's population grows by three per house.

## Porto Luma
- A 4 × 4 grid of 12 m streets (avenues and streets at ±40 and ±120 from the centre) around
  Placo Luma, the central square. Highways stop at the grid's west edge on the street north of
  the square.
- Main Street rows, a glass tower and office block, corner mid-rises, a car park, signals, street
  trees and lights, and parked cars.
- Waterfront east of the grid: quay roads, six warehouses, a harbour office, a dock building,
  pallets and barrels, and a line of bollards.

## Terrain
`TerrainHeight` levels both suburbs (|x| 150–485, |z| ≤ 190) with downtown, and Porto Luma's
building pad is 205 m in radius, up from 85 m.
