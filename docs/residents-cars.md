# Residents use cars

Verda's residents now drive their own cars in Explore.
1. The driver walks out to the car.
2. They pull straight out of the space, drive the roads on the right-hand side, and park at the kerb near where they're going.
3. They walk the rest of the way.

Off-screen this is simulated on the island clock. Near the player, the real vehicle is driven through the same physics the player uses.

## Who owns a car

Every real car on the map belongs to a household:
- vehicle spawn markers;
- vehicles placed in the Creator.

When island life is built, each car goes to the nearest home within 70 m that doesn't have one. The driver is the member of that household with the longest way to work.

On the published map, **35 of the 36 cars** belong to residents:

| Where | Cars |
|---|---|
| Downtown kerbside parking | 16 |
| Roka trailer park | 9 |
| Driveways (new) | 10: 8 beside the houses in Suda Haveno, 2 in Espera |

The one car in the middle of the capital has no home near it, so it belongs to nobody and is free for the player.

`outland_build_world` now gives most houses in the small towns a car on the drive. They're `vehicle_spawn_hatchback_<town>_drive_n` markers, and `maps/verda_world.map` is regenerated; the only change is those 10 markers.

Downtown residents live a block from work and walk. The trailer park drives to the Roka works and the capital, and Suda Haveno commutes to the capital.

## When people drive

A trip is driven when all of these hold:
- it's longer than **200 m** door to door;
- the resident's car is within **220 m**;
- the car isn't parked by the destination already.

Otherwise they walk, as before. People without cars still take "a lift" on long trips, which is the old abstract timing. Once at the wheel, a driver whose plans change (gunfire, a new activity) drives on to the new place.

## Off-screen and on-screen

- **Off-screen**, a car advances along its road route. Within 450 m of the player it moves at a realistic 12 m/s and the real vehicle follows it, so a car seen in the distance isn't zipping about. Further away it runs on the island clock (11 m/s × time scale).
- **Within 160 m** of the player (at most 6 cars at once), the real vehicle is driven by an autopilot through `VehicleSystem`:
  - **steering:** pure pursuit along the lane-offset route;
  - **speed:** 11 m/s, slowing to 5 m/s for corners and 4 m/s pulling in and out of a space, and braking smoothly to the parking spot;
  - **stopping:** it stops for anyone or anything in its lane: the player, NPC bodies, other vehicles;
  - **getting stuck:** if it's wedged it backs off and tries again. After 5 tries the driver parks where they are and walks.
- **Beyond 230 m** the car goes back to the off-screen tier from wherever it got to.

**Parking:**
- At home, the car goes back to its own space.
- Elsewhere it parks at the kerb on the destination's side of the nearest road, facing the way that side's traffic runs. Downtown that's the parking lane of the 12 m streets; narrower roads park on the verge.
- Spaces 6.5 m apart are tried until one is free.
- With no road near, it uses a clear patch near the door.

Every Explore session starts with the cars parked at home. Leaving Explore puts them back too.

## Taking a car

- **A parked resident's car:** you can take it. It's a **car theft**, a 1-star crime if someone sees it.
- **A resident's car in motion** can't be boarded ("It's moving") until it's crawling.
- **Carjacking:** boarding one with the driver inside pulls them out. They're frightened and walk off, and it counts as **assault** (2 stars) with the driver as the victim.
- A taken or wrecked car leaves island life for the session, and its owner walks.

## Roads as a graph

`world::roads::RoadGraph` (`src/world/roads/RoadGraph.cpp`) builds the island's road network from the map's road segments:
- downtown crossings and T-junctions are split into shared nodes;
- dead ends that stop within 15 m of another road are linked. This is how the trailer park's lane, which stops 10 m short of Roka's street, joins up.

It gives:
- the nearest road point;
- shortest routes with lane offsets. Drive on the right, a quarter of a two-lane road's width, or 15% on wide streets so traffic clears the kerbside parking lane;
- `route_point` and `route_progress` for following a route.

## Other fixes on the way

- **NPC walking:** a body sliding back and forth against a wall (a doorway jamb, for example) moved enough each step to escape the old stuck check. It now also asks for a route when it makes no net progress toward its goal for 1.5 s.
- **Residents heading to their car** use navigation with one fixed goal (the car door) instead of switching between "the door" and "the car".

## Code

| Area | Where |
|---|---|
| Ownership | `assign_cars` in `LifePopulation.cpp`, `Island::cars` |
| Trip state | `Resident::car`, `Resident::trip` (Walk / ToCar / Driving) |
| Trips and autopilot | `src/game/life/LifeTraffic.cpp` (`LifeSimulation::bind_vehicles`, `take_car`, `drive`, `parking`) |
| Vehicle hooks | `VehicleSystem::set_autopilot` / `clear_autopilot` / `place` / `find` |
| Crime | `CrimeKind::CarTheft` |
| Renderer | binds the vehicles in Explore, refuses to board moving resident cars, reports theft and carjacking |

All distances and speeds are in `LifeConfig`: `drive_distance`, `car_reach`, `car_speed`, `car_radius`, `car_release`, `max_driven`.

## Tests

`outland_traffic_tests` (`tests/integration/life/test_traffic.cpp`) covers:
- **The road graph:**
  - every town is reachable from downtown, and routes stay on roads;
  - the trailer park's lane is connected;
  - downtown routes turn at crossings;
  - eastbound traffic keeps right.
- **Ownership:** owners and cars agree, and every car is parked near home.
- **An off-screen weekday morning:** 18 residents drive, cars keep to the roads, and every long-distance commuter's car ends up parked at work.
- **On-screen:**
  - the owner walks out of the house to the car and the real car pulls away under autopilot;
  - it stops for someone stepping into the road;
  - once the player leaves, it finishes the trip off-screen and parks by work with the driver walking in.
- **Carjacking and theft:**
  - pulling a driver out frightens them and the car leaves island life;
  - a stolen parked car has no one inside;
  - cars are home again next session;
  - unbinding releases every autopilot.
