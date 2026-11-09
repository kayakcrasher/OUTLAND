# Norda Kvartalo and La Levita

Verda City's north side, built like a Chicago neighbourhood on downtown's grid. The world kit's
`north_quarter()` (`src/dev/VerdaWorldKit.cpp`) builds it into `maps/verda_world.map`. Its layout,
and the timetable of the trains, are in `world::north` (`include/outland/world/NorthQuarter.hpp`).
The models are written by `tools/generate_city_kit.py` (the "Norda Kvartalo" section) into
`assets/verda/city/north/`.

## The streets
- Downtown's avenues (x = ±87, ±171) carry on north past the grid's edge to two new 16 m streets at
  z = −252 and z = −336. Both cross the highway at grade.
- That makes two more rows of 68 m blocks, centred on z = −210 and z = −294.
  - The first row's two blocks beside the highway are left to the police station, the hospital and
    a radio station.
  - The other six blocks are the neighbourhood.
- Every block has an **alley** down its middle (6 m, east–west, at street level between the two
  sidewalk halves: `ground/alley_block_68.glb`). Behind the houses: wooden back porches, yards,
  trees and bins.
- The terrain is levelled under all of it (`layout::downtown_north_edge`).

## The buildings
Each block's two street fronts are built up in a row, the fronts on the lot line 4 m back from the
kerb. A row is one of five patterns:
- **Greystone three-flats** (7.8 m wide):
  - a flat on each of three floors over a raised garden level;
  - a limestone front (grey or buff) with a three-sided bay, and a stoop up to the door;
  - a deep pressed-metal cornice;
  - common-brick sides and back, with Chicago's stacked wooden back porches and a closed stair
    tower.
- **Brick walk-ups** (16 m, four storeys):
  - six windows a floor with stone sills and lintels, a band at each floor, a bracketed cornice;
  - a black steel fire escape across the front, with its drop ladder;
  - a timber water tank on a steel stand on the roof.
- **Walk-up variants:**
  - Some have a **corner store** on the ground floor, under a green sign: NUTRAĴOJ, BAKEJO,
    GAZETOJ · KAFO, or FRUKTOJ KAJ LEGOMOJ.
  - Some have a **mural** over the end wall facing the avenue. The *verda* mural shows green hills
    under a big sun, the sea and the green star. The *geometrio* mural is bold bands and circles.
- **Corner taverns** (two storeys):
  - neon in the windows and a blade sign out over the pavement;
  - a name board over the door: ĈE LA ANGULO, LA VENTO, TAVERNO STELO, LA RUĜA LAMPO, or LA LASTA
    TRAJNO.

Cars and the odd Verda Fulmo are parked at the kerbs. The buildings are shells (cover), not
enterable.

## La Levita, the elevated railway
- **The line** runs over the second row's alley (z = −294), from x = −150 to x = 150:
  - a steel deck with two tracks, 5.4 m up at its underside;
  - 20 m deck pieces (`north/l_deck.glb`);
  - steel bents at each block's quarter points (`north/l_bent.glb`).
- **Spans:** the line crosses the avenues at ±87 and the highway on 24 m spans. Cars and people pass
  under it freely: collision meets the deck's real shape (`WorldCollision::elevated`).
- **Stations:** three of them, Okcidento (x −130), Centro (45) and Oriento (130)
  (`north/l_station.glb`).
  - Each has a side platform beyond each track, under a canopy, with benches.
  - A board names the station over each platform.
  - Stairs climb to each platform from the yards beside the alley.
- **Trains:** two two-car trains in brushed steel with a red stripe (`north/l_train.glb`).
  - Each shuttles on its own track: west terminal, Centro, east terminal, Centro, and back.
  - They pull away and brake into each stop (14 m/s top speed, 1.1 m/s²) and stand 14 s at each.
  - The second runs half a round (about 71 s) behind the first.
  - The renderer draws them from `world::north::train()`. They are scenery: you can't ride them.

## Tests
`outland_north_quarter_tests` checks:
- the timetable: each train keeps to its track, stays on the deck, never outruns the line or jumps,
  and stands at every station;
- the map:
  - the new streets;
  - the three-flats, walk-ups, taverns and murals;
  - the deck the length of the line, and three stations;
  - the avenues, highway and alleys open to cars and walkers under the deck;
  - the stairs up to Centro's platform.

The road audits (`outland_road_audit_tests`, `outland_road_conflict_tests`) know the deck spans
the streets.
