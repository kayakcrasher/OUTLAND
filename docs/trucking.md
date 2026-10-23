# Trucking: Verda Transporto and the cargo terminal

Verda has a working port and a trucking company. Ships call at **Kargohaveno de Porto Luma**, the
cargo terminal below Porto Luma, and **Verda Transporto** runs the island's freight from its depot
in Verda City's industrial quarter. Its own drivers are on the road all day, and anyone who isn't
a serving police officer can sign on and drive loads for pay (Explore).

Code: `world/VerdaLayout.hpp` (`cargo_*`, `fish_z`, `quay_*`, `freight_*`), `TerrainHeight` (the
ramps, the quay and the berth), `dev::build_verda_towns` (`cargo_terminal()`, `freight_depot()`), `game::freight::Freight`
(the AI rigs), `game::freight::Trucking` (the player's loads), `game::ranch::Ranches::ship_cattle`
(the cattle quay). Models: `tools/generate_city_kit.py` (`civic/cargo_ship.glb`,
`harbour_crane.glb`, `quay_apron.glb`, `container_stack.glb`, `cattle_gangway.glb`,
`truck_depot.glb`). Tests: `outland_trucking_tests`, `outland_ranch_tests`, `outland_world_kit_tests`.

## The truckers' roads
- **Wider routes.** The trucking company's main routes are 14 m wide: the yard road from
  Verda Transporto's depot along z = -84 into downtown, the link from it up to the boulevard
  where the Roka highway leaves, and the terminal road into the cargo terminal. The terminal road
  narrows to 13 m between the ramp's walls. The homes on the z = -84 road stand 3 m further back.
- **Freight yards.** Espera, Porto Luma, Suda Haveno and Roka each have one of Verda
  Transporto's yards just off the highway, before it narrows into town. A 16 m road leads in from
  the highway (with a median opening) to a 14 m loop round a gravel yard: drive in, go round, stop
  to unload, and head back out the way you came. A shed and containers stand beyond the loop.
  Deliveries to those towns are dropped there, not in the town's streets
  (`dev::freight_yards`, marker `freight_yard_<town>`).
- **The camera.** Driving a rig (or a bus) the camera looks from over the cab, a little ahead of
  its middle. With a trailer hitched it rides further back and higher, over the trailer, so the
  road ahead stays in view (`VehicleRenderer::set_trailer`).

## The cargo terminal
Porto Luma stands 38 m above the sea, so the terminal is down at the shore:
- **The terminal road** runs straight east from the town's east street (x 1876, z -470), across
  the quay road and on down the ramp: a rig turns once, off the town street, not a dog-leg along
  the quay road.
- **The ramp** runs straight east from the quay road (x 1950, z -470), 230 m at an even 15% grade,
  down to the quay. The land is cut and filled to it.
- **The quay** is a 60 x 112 m concrete apron 4 m above the water. Its seaward face is a quay wall
  with rubber fenders, bollards and a yellow safety line. A lane runs the length of it.
- **The berth**: the water along the quay face is dredged deep, and **Stella Maris** lies
  alongside. She is a 96 m feeder container ship: red bottom, dark blue hull, four bays of
  containers on deck, the white house aft with the bridge, and a funnel in Verda green.
- **Two ship-to-shore cranes** (red and white) stand on rails at the quay edge, their booms out
  over the ship. Stacks of 40 ft containers wait on the quay either side of the ramp's mouth.
- **The cattle quay** (Brutkajo de Porto Luma) is at the north end: a pen behind a loading chute
  on the lane, and a cattle gangway up the ship's side. Ranchers' cattle are shipped from here;
  see [vakeroj.md](vakeroj.md).

### Two docks, one loop
The cargo end is where the island gets its supplies. The same quay runs on south, level, to the
**fishers' quay** (Fiŝista Kajo), which is the locals' end:
- **The fishers' jetty** stands off the quay face at z = -360. Its deck is level with the quay, so
  you can walk or drive straight on. Two trawlers lie alongside and a third at its end, with a
  rowing boat, fish boxes, nets, lobster pots and a fish stall at the land end
  (`civic/fishers_jetty.glb`). The smugglers land here too, after dark (see [gangs.md](gangs.md)).
- **A paved parking area** behind the jetty, for the locals.
- **The fishers' ramp** climbs from the quay at z = -360 back up to the quay road, on the street
  that runs into the town at Porto Luma's z + 40.
- **The lane** runs the whole length of the quay and joins the two ramps.

So the docks are a loop: down the terminal ramp, along the quay under the cranes, past the fishers'
jetty and up the fishers' ramp, or the other way round. A rig never has to turn round on the quay.
The quay road between the two ramps is 14 m wide, like the other trucking routes.

## Verda Transporto
The depot is in Verda City's **industrial quarter** (Industria Kvartalo), on level ground west of
Okcidenta Verda and south of the Roka highway. The suburb street on z = -84 runs on into its yard.
- **The depot**: a steel-clad truck shed with roller doors and the two-storey office across its
  front, signed **VERDA TRANSPORTO**. Dispatchers work in the office (island life). Sheds,
  pallets, drums and a container stack stand around it.
- **The truck park**, across the yard road, is where the company's rigs stand nose to the road:
  - three box-trailer rigs for hire (`vehicle_spawn_semi_depot_1..3`);
  - one stock-trailer rig for cattle (`vehicle_spawn_semi_depot_cattle_1`);
  - the company's own drivers' three rigs (`vehicle_spawn_semi_haul_1..3`).
- **The boss**, Tomaso Rado, stands outside the office door. His name tag reads "Verda Transporto".

### The company's drivers (more trucks on the road)
On top of the supply rig and the cattle rig (see [vakeroj.md](vakeroj.md)):
- **The container shuttle** runs between the depot and the cranes at the cargo terminal.
- **A second supply rig** runs the bus loop's towns, starting halfway round from the first.
- **The Roka run** goes from the terminal to Roka, the industrial town, and back to the depot.

They drive like the other rigs: kinematically along the roads, slowing for towns and corners. At
the terminal "the crane swings a container onto the rig".

## Driving for Verda Transporto
Talk to the boss (**USE** next to him). The first time, he offers work; **USE again** to sign
on. A serving officer is turned away ("hand in your badge first"). Gang members and civilians are
both welcome. He gives out **one load at a time**:

| Load | Pickup | Drop |
|---|---|---|
| IMPORT | a container off the ship at the cargo terminal | a town (Verda City, Espera, Porto Luma, Suda Haveno, Roka) |
| EXPORT | goods at the depot | the cargo terminal, for the ship |
| DELIVERY | goods at the depot | a town |
| CATTLE HAUL | a ranch's cattle at its chute | the cattle quay, to be shipped |

1. Take a rig: a **box-trailer rig**, or the **stock-trailer rig** for cattle. The ones for hire
   stand in the truck park, but any of the island's big rigs will do, since they all wear the
   company's colours. That includes the company drivers' rigs beside them (`semi_haul`), the
   supply rig (`semi_freight`) and the cattle rig (`semi_cattle`). Taking one off its run ends
   the run. The wrong trailer, a car or on foot loads nothing; a line on screen says why.
2. Drive to the pickup (a green column of light, and a mark on the compass). Pull up within 16 m
   and **stop**. Loading takes a few seconds. For cattle, the family drives a few head into the
   corral when the job is given out, and they go up the chute when you stop. If none are there
   yet, wait.
3. Drive to the drop and stop to unload. Cattle come off into the pen at the cattle quay and go
   aboard the ship, and the ranch is paid for them.
4. Go back to the boss. He pays for the trip and has the next load ready.

**Pay** is by the trip: 180 kr, plus 120 kr a kilometre of the run (the run from the yard to the
pickup counts half), plus 60 kr a head for cattle. **USE** twice on the boss with a load in hand
gives it up. The panel at the top right shows the load and what to do next. Signing on, the trip
count and the total earned are saved beside the career (`<profile>.career.trucking`).
