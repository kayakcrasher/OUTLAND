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

Cars and the odd Verda Fulmo are parked at the kerbs.

**Every building can be entered** (`walk_in()` in the generator):
- a front door and a back door;
- a floor on every storey;
- switchback stairs up the +X wall, 0.2 m risers and 0.28 m treads, so you can climb to the top
  floor;
- a three-flat's door is up its stoop, its back door out onto the first floor's porch.

**Who uses them:**
- **Flats are homes:** Verda City's people live in them (the city's population grows by two for
  each new home).
- **In and out by the door:** each building's place marker stands 2.6 m inside, in line with its doorway. Island life makes a 3 m place round it and puts its door 2 m in front, on the threshold. Residents walk out through the doorway, never pressed against the front wall beside it.
- **Corner stores are shops**, with flats over them.
- **Taverns are pubs**, with the bar, stools and tables inside and a flat upstairs.

They are on the world kit's list of enterable buildings, so each gets a loot spot on its ground
floor.

## La Levita, the elevated railway
The line runs about **1.15 km** in a U round the north of the city (`world::north::pieces`).

**The route:**
1. **Okcidenta Parko**, the west terminal, stands in the park in the middle of the west suburbs'
   blocks (their empty heart).
2. The line curves out of the park onto the suburb street at x = −330 and runs north up it, on
   **wide bents** whose posts stand on the verges either side, so the street stays open under it.
3. It swings round onto Norda Kvartalo's second-row alley and runs east over it, crossing the
   avenues and the highway on long spans: **Okcidento**, **Centro** and **Oriento** stations.
4. It swings round again and runs down the east suburbs' street at x = 330 to **Orienta Verda**,
   the east terminal.

**How it's built:**
- **Deck:** 20 m pieces down the straights (`north/l_deck.glb`), 4.4 m pieces round the curves
  (`north/l_deck_4.glb`).
- **Bents:** one every 16 m or so (`north/l_bent.glb`, wide ones `north/l_bent_wide.glb`). A bent
  goes up only where both its posts stand clear of every street the line runs over or crosses.
- **The ground:** where the line crosses open country between Norda Kvartalo and the suburbs, the
  ground under it is levelled (`north::corridor_outside`).
- **Stations:** five (`north/l_station.glb`).
  - Each has side platforms under canopies with benches, and stairs down from each. A stair climbs to a
    landing beside its platform; the platform's railing opens onto the landing and is closed everywhere
    else, along the stair too. The landing is railed round. The walkway is the platform's outer lane,
    past the benches and the canopy posts.
  - A board over each platform names the station: "LA LEVITA · OKCIDENTA PARKO - ORIENTA VERDA".
- **Under it:** cars and people pass freely, because collision meets its real shape
  (`WorldCollision::elevated`).

**The trains:**
- Two two-car trains (`north/l_car.glb`), each shuttling on its own track from one terminal to the
  other and back.
- They stop at every station: 14 m/s top speed, 1.1 m/s², 14 s at each stop. A round takes about
  six minutes.
- The second runs half a round behind the first.
- Each car is drawn on its own stretch of the line (`north::car`), so the trains bend round the
  curves.
- They are scenery: you can't ride them.

## Tests
`outland_north_quarter_tests` checks:
- the line is one continuous path;
- the timetable: each car keeps to its track, the trains stay on the line, never outrun it or
  jump, and stand at every station;
- the map:
  - the new streets;
  - the three-flats, walk-ups, taverns and murals;
  - the deck the whole length of the line, and five stations;
  - the avenues, highway, alleys, suburb streets and their cross streets open to cars and walkers
    under it;
  - at every station, both platforms: up the stair from the street, across the landing and along the platform, and no stepping off its side onto the stair.

`outland_world_kit_tests` walks into every building through its front door and up its stairs. The
road audits (`outland_road_audit_tests`, `outland_road_conflict_tests`) know the line spans the
streets.
