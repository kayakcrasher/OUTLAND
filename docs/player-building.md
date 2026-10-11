# Building your own town (Explore)

In Explore, **BUILD · K** (beside MAP) opens the build bar. Walk and look as usual; a green ghost of
the chosen piece sits on the ground where the camera looks (within 45 m), red with the reason when
it can't go there.

| Piece | Notes |
|---|---|
| Road | PLACE (or START) at one end, PLACE at the other; it carries on from the last end. Ends snap onto a road within 6 m, so new streets join the island's road network (cars, buses). 6–160 m per run. |
| Cottage | Procedural single-storey home, enterable, 4 beds. |
| House | Two-storey home, enterable, 8 beds. |
| Bus stop | Snaps to the verge of the nearest road (within 14 m), facing it; none within 60 m of another stop. Linio 1 is laid out again and calls there. |
| Street lamp, tree | Dressing. |

Keys: F/Enter place, R turn (45°; homes face you until turned), Tab or 1–6 choose the piece,
Backspace remove (the most recent piece you built near the ghost; a home people live in stays),
K/Esc done. Touch: the bar's buttons.

Homes are checked for clear ground (no buildings, props or roads within the footprint and 1.5 m
of a road edge), slope (under 1.6 m across the lot) and the sea.

## Newcomers
Player homes stand empty: island life leaves `player_` buildings out of the population. When a
bus pulls in at one of the player's stops, a family (2–4 for a cottage, 4–6 for a house) steps off
and walks to the nearest empty player home within 600 m (`LifeSimulation::move_in`). They get jobs
at the nearest workplace, haunts and day plans like everyone else, and are saved with the home. A
bus comes round about every six minutes of play.

## Saving
`<profile>/explore.builds` beside the Explore loot profile (`OUTLAND_BUILDS 1`: one line per piece:
kind, serial, position, road end, yaw, residents). Pieces are applied to the nearest town
(`PlayerBuilds::apply`) at Explore start; other modes take them away. Each change re-applies,
reloads collision, clears the nav grid, rebuilds signs and the static world, and saves.

Code: `game/build/PlayerBuilds` (pieces, checks, apply, save) and `game/build/BuildMode` (the bar,
ghost and input). Tested by `outland_player_build_tests`.
