# Street racing

At night (21:00-03:00) the island's young drivers meet at the Marnafto gas station on the ring
road (around -713, 1347): three cars parked on the verge with their mods on, and Fulmo, who runs it.

- **The field:** Mira 'Ŝtormo' Kovač (muscle car), Tomas 'Pneŭo' Varga (sedan) and Niko 'Pulvo'
  Petrov (hatchback). By day the cars are gone.
- **Getting in:** talk to Fulmo (USE). The first time he names the stakes; talk again with
  1,000 kr and the bet is on. Before nine he tells you to come back after dark.
- **The start:** drive to the flare on the grid and stop. Three, two, one, GO.
- **The race:** a quarter of the ring, about 2.4 km, through flares every 420 m. A flare only
  counts within 30 m, so no cutting across. The HUD shows place, field and flares.
- **The rivals** drive on autopilot (`game::ai::steer_along`), each in its own lane. They ease
  off when far ahead and push when far behind, so a race stays close.
- **Winning** pays the pot: 1,000 kr from each of the four. Losing costs your stake. Leaving your
  car or the road for 15 seconds forfeits.
- After a race the meet resets once you are well away from it. Results make the news and the
  island's gossip. A win brings a call from Uncle Dario.

Code: `game::racing::StreetRace` (`include/outland/game/racing/StreetRace.hpp`). Test:
`tests/integration/racing/test_street_race.cpp` (the real ring, a full race lost, then a race won).
Dev: `OUTLAND_DEV_RACE=1` puts the stake down.
