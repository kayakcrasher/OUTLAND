# The outbreak (Zombie Survival's story)

Zombie Survival on a single player's island is now a story: the morning Verda fell. LAN zombie
games are unchanged (no story).

## The opening

1. **Wake up.** The screen comes up from black in Uncle Ron's house in Okcidenta Verda (Verda City).
   Ron shakes you awake: the city is gone, people are eating each other in the street. Dario
   called - he is the only one of La Verda Mano who made it; they hit his bar first and it fell in
   minutes. He is holed up in the barn at the Garza place, Ranĉo Verdaj Kampoj, north of the city.
   (USE moves the lines on.)
2. **Get in the pickup.** Ron's pickup is on the drive. Get behind the wheel of it (or any vehicle
   near Ron) and he climbs in beside you; he rides in the passenger seat.
3. **Drive him to the barn.** The compass and the objective point to the farm, 550 m north.
4. **The standoff.** Stop in the farm yard. Ron gets out, Dario is at the barn doors, and the Garza
   family come out with shotguns and rifles and tell you all to turn round or be shot. Dario shoots
   them first. Then they get back up, and the farmhands come out of the house with them.
5. **Clear the farm.** Nine infected; the objective counts them down. Dario fires on any that come
   at him.
6. **The barn is yours.** Dario hands over Garza's hunting rifle (and 30 rounds); the barn is your
   hideout. From here on, dying puts you back at the barn doors. Dying before the barn is yours
   starts the morning again.

## The island

- **The map is red** except round you (260 m), your hideout and the holdouts you have found.
  Verda City is marked **GROUND ZERO**.
- **Verda City is ground zero:** 22 nests round the capital instead of 8 (about twice the infected
  of an ordinary game there), 260 infected island-wide.
- **Holdouts** (game::outbreak::holdouts). Each has a leader to talk to (USE, three lines in turn)
  and guards who shoot any infected that come inside their ground; no infected are sent there and
  none come back once killed there. Found ones go on the map and the compass.

  | Holdout | Who | Leader |
  |---|---|---|
  | Espera | Montaj Lupoj (gang) | Tomaso Lupo |
  | Porto Luma docks | La Ora Ankro (gang) | Kapitano Ines Moreno |
  | Suda Haveno | Marĉaj Ranoj (gang) | Jakobo Ranulo |
  | Roka | Feraj Hundoj (gang) | Maks Ferro |
  | Sovaĝejo, in the forest | La Verda Stelo (Esperantist families) | Klara Zamen |
  | Ŝtata Malliberejo | the last of the Policio | Ĉefo Marko Forta |
  | Verda Transporto's depot, Verda City | the Trucking Brotherhood | Ivo Kovač |

- **The Brotherhood** keeps its camp in Verda City and has two rigs on the road, driving from
  holdout to holdout. Nobody knows who pays their bills.
- **The black tower** downtown (the dark glass tower with no windows and no doors) belongs to the
  company behind it all (`company` in Outbreak.hpp, not yet named in play). A red light blinks on
  its roof; it goes on the map as **???** once you have been close.
- **Fuel is scarce.** Each Marnafto station holds what it holds, between a third of a tank and
  three and a half tanks, fixed by the station. Pull up at a pump and take what is left - free,
  nobody is there to pay. Every fill comes out of it, and a dry station stays dry (saved).

## Talking at the hideout

Ron and Dario take turns with what they know: the gangs' towns, the prison, ground zero, the
Brotherhood, the tower, the families in the forest, the fuel.

## Save

Beside the Zombie Survival loot profile: `<profile>.outbreak` - the stage, where you were, Ron's
pickup and the Brotherhood's rigs, the holdouts found, what is left at each station drawn from,
and whether you have seen the tower. Saved at each step of the story, when something is found,
every two minutes at the hideout, on Save and on leaving.

## Code

- `game::outbreak` (`Outbreak.hpp`): the story's stages and lines, the holdouts, the tower, fuel,
  the red map's clearings, the save.
- `game::outbreak::Run` (`OutbreakRun.hpp`): the cast in play (NpcSystem bots 975000+), Ron riding
  along, the farm scenes, guards and Dario shooting, the Brotherhood's rigs, discovery.
- `game::zombies::ZombieEcology`: `capital_nests`/`town_nests`, `spawn_at` (the story's infected:
  awake, hunting, never replaced), `set_safe` (holdouts and the hideout).
- Testing: `OUTLAND_DEV_OUTBREAK=farm` starts at the wheel with Ron, just short of the farm.
- Tests: `outland_outbreak_tests`.
