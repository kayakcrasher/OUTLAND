# Paul Acosta, "the walking man"

An Explore character with a routine of his own. Code: `game::life::WalkingMan`
(`include/outland/game/life/WalkingMan.hpp`), wired into `Renderer.cpp` next to La Mafio.
Tests: `outland_walking_man_tests`.

## His day
- **Home**: a trailer at **Ruldomaro**, the trailer-park hamlet on the eastern ring road. His car
  is the hatchback parked beside it (`vehicle_spawn_hatchback_ruldomaro_1`). Nobody else drives it:
  residents don't get it, gangs' hit squads skip it (`BotDrivers::reserve`).
- **8:30**: he drives toward town and leaves the car on the shoulder **just outside Verda City's
  limits** (35 m past the Green Core's edge, on the road home), then **walks** in.
- **Until 15:00**: poker at **the casino** (La Verda Mano's table in Verda City).
- **15:00-22:00**: **the bar** (the nearest other pub in town).
- **22:00**: about every other night (seeded per day) he has had too many. Sober, he walks back to
  the car and drives home. **Drunk, he stumbles home on foot**, weaving across the road, stopping
  now and then, all the way to the trailer park. The car stays outside town overnight, so the next
  morning he walks out to it (and on into town). After a very late night he sleeps in.

Near the player (within 250 m) he walks at a real pace (1.45 m/s, 1.1 m/s drunk). Out of sight his
walks run 6 times faster, so a walk across town fits the island's quick clock (an hour every two
minutes). He drives for real when the player is within 230 m of him or of where he is going;
otherwise the car is simply moved. When the clock jumps (sleeping till morning) he is wherever his
day says.

Close by, a tag over him reads **PAUL ACOSTA "the walking man"** and what he is up to ("playing
poker at the casino", "drunk, stumbling home to the trailer park"...).

## Self defence
He carries a **pistol** in his waistband and never starts trouble. He draws it and fights back
(the Battle Royale combat brain, Medium) when:
- you **hurt him** (bullets, a grenade, a shot through his windscreen);
- you **shoot at or past him** (a round within about 2 m);
- you **keep a gun aimed at him** (aiming down the sights, within 16 m): after a moment he warns
  you ("Whoa - easy, friend. Put that down."), and if you keep it on him he draws.

His quarrel is with you alone; the gangs leave him be and he leaves them be. It blows over 40 s
after the last provocation (or when you die): he puts the pistol away and carries on with his day
from wherever he is.

Killed, he is back in his trailer the next morning.

## How it works
- He is a `CombatBot` in the shared `ai::BattleRoyaleBots` with his own body (`character_27_hm`).
  While calm he is "asleep" there and `WalkingMan` moves him (`CombatBot::puppet`, which keeps his
  walk animation); his pistol is holstered (`WalkingMan::dress`).
- `Mafio::set_outsiders` lets the Mafio's hostility rule ask `WalkingMan::hostile_to` about anyone
  outside the gangs.
- Walks follow the road graph on the right-hand verge, then through the door to his seat.
- Drives use `BotDrivers::dispatch` with his car only.
