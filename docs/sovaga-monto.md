# Sovaĝa Monto and the mountain road

North of Sovaĝejo a range of peaks now stands along the west coast, outside the ring road. Its
summit, **Sovaĝa Monto**, is about **289 m** high, the highest ground on Verda by far; the rest of
the island stays under 30 m. Sovaĝejo's dirt road no longer stops at the trailer park. It goes out
through the park's north gate, winds about **2.9 km** over the range, and comes down to the ring
road.

Code: `world/MountainRoad.hpp` lays out the range, the road and its profile, and shapes the land.
`outland_build_world` (`sovagejo_mountain()` in `src/dev/VerdaWorldKit.cpp`) builds what stands
along it.

## The range

`mountain::massif()` adds the range to the land, before the building pads and the shore
(`TerrainHeight`). It has four smooth peaks along the coast, each with a little crag and ridge
noise:

| Peak | Bearing | Height |
| --- | --- | --- |
| South shoulder, above Sovaĝejo | 192° | ~70 m |
| Ridge behind La Ringo Okcidento | 206° | ~120 m |
| **Sovaĝa Monto**, the summit | 222° | **~289 m** |
| North ridge | 238° | ~115 m |

The range falls away to nothing:
- at the ring road's verge, so the ring and its levee never move;
- around Sovaĝejo's pad;
- around La Ringo Okcidento's pad.

Toward the sea it runs down into the shore band, so the seaward faces drop to the water.

## The road

**Route.** It leaves the trailer park's north gate at (-1790, 40), where the four fence panels
across the lane were taken out. It climbs the range's landward flank through the saddles between
the peaks, passes under the summit at about **99 m**, then descends two hairpins to the ring road's
outer verge at bearing 246.5°, a little before Ranĉo Montaro. A last stretch of dirt meets the
ring's centre line there, so traffic joins.

**How it was laid out.** The bends were placed where the land is already at the road's height,
following the contours. They are then smoothed through a centripetal Catmull-Rom spline and sampled
every 4 m.

**Profile (the road's height along it).**
- It follows the land, evened out over 40 m either way.
- The level stretches are made flat: each pullout, the fork, and both hairpins, so the inside of a
  hairpin is one level. Each flat stretch is kept within reach of its neighbours.
- It is then eased so it never climbs or drops more than **9.5%**.

**The land is cut and filled to the road.** `mountain::graded()` makes the roadbed level across,
5.5 m each side of the centre line. Beside it, the land banks back into the mountain over 14 to
34 m: wider where the road is cut deep or built up.
- Where the road's other leg is near (a hairpin's other side), both shape the land and blend
  smoothly.
- Every pullout widens the roadbed into a 16 m bench on one side, at the road's height.
- Supra Sovaĝejo's lane and pad are level with the fork.

So the ground you drive on is the road: within a few millimetres of its profile, level across to
3 cm at the worst hairpin, and never steeper than its grade. The terrain is drawn at the highways'
2 m grid along the road (`TerrainWorld`), so the cuttings and banks show as they are.

## Pullouts

There are six, every few hundred metres. Each sits on the side where the land lies nearer the road,
so there is less to cut or fill. Each has:
- a gravel **lay-by** off the road and back on;
- four **parking bays** behind square bollards, most with a car;
- a **trailer home**, a HOME for two people, with yard junk and an Apelacio flag;
- a **street light** over the lay-by.

The third pullout is the **lookout** under the summit. It has a second light, an Apelacio flag on a
tall pole, and a board reading "SOVAĜA MONTO / 289 m".

## Supra Sovaĝejo

At the road's seaward bend, about 680 m from the park, a gravel lane forks off to a level bench
above the sea, about 31 m high.
- **Homes:** six trailers, three on each side of the lane, their fronts to it. Each is a HOME for
  three people, with a car beside most of them and an Apelacio flag.
- **Common ground:** a fire pit with benches at the lane's end, street lights along the lane, and
  shade trees.

It is still Sovaĝejo: its people, and those of the pullout trailers, are counted in Sovaĝejo's
population. Island life files everything up the mountain road under Sovaĝejo, however far from the
town's middle (`nearest_town` in `LifePopulation.cpp`).

## Flags and boards

The Apelacio, "APELACIO AL LA ĈIELO", the pine tree that is Sovaĝejo's own flag
([esperanto-signage.md](esperanto-signage.md)), flies:
- beside every trailer up the road (`trailer_flags`);
- on tall poles at both ends of the road, at the fork, at the lookout, and on Sovaĝa Monto's summit
  (`mountain_signs`).

Hand-painted boards mark:
- the park gate: "SOVAĜA MONTO / LA RINGO 2,9 km";
- the fork: "SUPRA SOVAĜEJO".

At the ring road end the board ("SOVAĜA MONTO / SOVAĜEJO 2,9 km") is a county board, since that end
lies in Altverdo. Along the Ferarbaro stretches the backcountry boards appear as on any dirt road.

## Other changes

- **Battle Royale:** the helicopters now fly at 700-766 m (was 650), so they stay 400 m above
  every hill, Sovaĝa Monto included.
- **Traffic:** a car pulling out beside a divided road (a boulevard with a planted median) now
  takes the carriageway on its own side, and arrives on the destination's side. It no longer tries
  to cross the median.

## Dev builds

`OUTLAND_DEV_SPAWN` starts you at a spot (x, z, yaw in degrees):
- `"-1618.6,-709.6,156"`: on the climb, looking up the road;
- `"-1233,-1172,80"`: at the lookout, looking out over its trailer;
- `"-1785,-565,-106"`: at the fork, looking down the lane to Supra Sovaĝejo;
- `"-650,-1405,-97"`: at the ring road end, looking back up.

## Validation

`outland_mountain_road_tests`:
- **The road:** more than 2.8 km from the gate to the ring road's verge.
- **The ground:** follows the road's profile within 5 cm, is level across the road within 12 cm,
  and is never steeper than 10.5%.
- **The summit:** the island's highest point; nothing else within 60 m of its height.
- **The range:** none of it under the ring road or La Ringo.
- **Pullouts and Supra Sovaĝejo:** every pullout bench is level, and so are Supra Sovaĝejo's lane
  and pad.
- **The world map:** the road's pieces join the gate to the ring, six trailers stand in Supra
  Sovaĝejo and one at each pullout, and the north gate is open.

Other suites:
- `outland_world_kit_tests`: 50 enterable trailers, every one walkable through its door. Sovaĝejo's
  roads are dirt, plus gravel for the lay-bys and the lane.
- `outland_signage_tests`: the Apelacio poles up the mountain, one of them on the summit, and the
  four boards, with the summit's height at the lookout.
- `outland_helicopter_drop_tests`: the helicopters stay 400 m above every hill.
- `outland_traffic_tests`: a commuter pulls out onto a boulevard without crossing its median.

`maps/verda_world.map` was regenerated. The only changes are the four fence panels, and the road,
lay-bys, lane, trailers, lights, bollards and markers added after every older piece, so older ids
are unchanged.
