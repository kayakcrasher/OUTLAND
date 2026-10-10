# Navigation graph

Everything on foot can now find its way through doors, up stairs and round buildings: combat bots, hostile NPCs, and residents travelling to work or home. Police and zombies will use the same graph.

## How it works (`world::navigation::NavGrid`)

- **Lazy 2.5D grid of 0.5 m cells.** Each cell stores up to 4 surfaces where a body fits: terrain, building floors, stairs and roofs.
  - Surfaces are found from the top down with `WorldCollision::ground_height`, and must pass `body_blocked` (0.3 m radius).
  - Graph and collision always agree because they come from the same triangles.
- **Cells are evaluated the first time a search touches them, then cached.** Island-wide pre-gridding would be far too slow.
  - An 8 m occupancy index of buildings and assets marks open countryside as terrain only, with no collision queries.
- **Connections:** neighbouring cells (8 directions, no corner cutting) connect when their surfaces are within the 0.5 m step height.
  - The kit stairs rise about 0.36 m per cell, so stairwells and upper floors come out of the real geometry.
- **Search:** A* with a straight-line heuristic and a small climbing cost.
  - Unreachable goals (the sea, the inside of a solid tower) return a partial path that ends as close to the goal as the body can get.
  - A goal inside a wall or tree snaps to standing room within 6 m.
- **Smoothing:** string-pulling, kept only where the straight walk stays on standable cells with full body clearance.

## Following (`PathFollower`)

- **On demand:** a goal on about the same level is walked straight, which is cheap and natural in open ground.
  - A route is planned when the goal is more than 1.5 m above or below, or when the mover reports being stuck.
  - Bots ask after 1 s blocked; NPCs after 0.6 s.
- **Pure pursuit:** the mover aims 0.6 m ahead on the current segment, not at the waypoint.
  - Bodies that arrive off the line get pulled back onto it before doors and stair lanes.
  - Without this, half the test bodies caught their heads on the slab edge halfway up the stairs.
- **Re-planning:** when the goal moves more than 1.5 m, at most every 0.75 s.

## Budgets (in game)

- At most 2 searches per frame across all movers.
- At most 250 new cells per search: a cold search returns a partial path and continues 0.2 s later from the cache.
- At most 8,000 expansions per search.
- The graph is cleared on every mode start and when leaving the Creator builder.

**Measured** over 20 s of a 23-bot match on the dressed island, optimised build:

| Setup | Average bot update per frame |
|---|---|
| Without routes | 5.35 ms |
| With routes | 5.87 ms |

Routing doesn't add frame spikes.

## Who uses it

- **Combat bots:** cover, flank, push, retreat, investigate and roam trips.
  - Engagement footwork stays local.
  - If blocked on a route, a bot plans once more before abandoning the trip.
- **NPCs:** chasing a threat or noise, and residents travelling to a schedule anchor. Short wanders stay direct.

## Validation (`outland_navigation_tests`, DEV)

- From the street to the upper floor of 8 downtown buildings:
  - every path is complete and climbs the stairs;
  - a body with real collision walks each one;
  - the worst cold search is about 0.3 s in the debug build, before budgets.
- A route around (or through) a building.
- The sea is unwalkable, and partial paths head toward the goal.
- A budgeted cold search finishes over several calls with the same route as an unbudgeted one.
- Follower plans once for a still goal, is rate-limited, re-plans for a moved goal, walks same-level goals straight, and routes when stuck.
- A bot investigating a noise upstairs reaches the upper floor; without routes it stays on the street.
- A hostile NPC follows gunfire upstairs.

## Not done yet

- **Moving vehicles** aren't in the graph. Bodies steer around them by collision and re-plan when stuck.
- **Ladders, vaulting through windows and jumping down** aren't links; drops are limited to a step.
- **Cover and flank searches** in the bot brain still probe with straight-line moves. They cause the occasional slow frame that exists with or without routing, and should move onto the grid.
- **Long trips** (hundreds of metres across towns) are planned in budgeted pieces. There is no coarse road-level graph yet.
