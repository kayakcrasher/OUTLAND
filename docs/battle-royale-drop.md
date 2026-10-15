# The Battle Royale drop

Every Battle Royale match starts in the air with **100 players** (you and 99 bots). **Four
helicopters**, each alone on its own straight line across Verda and each going a different way,
carry everyone; each player jumps when their line passes over where they want to land, falls
belly-down, opens a parachute and glides in. Code: `game::aircraft` (`HelicopterDrop`
for the logic, `DropRenderer` for drawing), wired into `Renderer.cpp`. Tests:
`outland_helicopter_drop_tests`.

## The routes
`make_fleet(seed)` sends **four helicopters**, a fresh set every match, each on its own route
(`make_flight` with a heading). The eight routes:

| | |
|---|---|
| north to south | south to north |
| east to west | west to east |
| south-west to north-east | north-east to south-west |
| north-west to south-east | south-east to north-west |

The four take the four different lines across the island (north-south, east-west and both
diagonals), each flown one way or the other at random, so no two go the same direction. Each line
crosses the whole island, starting and finishing out over the sea, up to 550 m to one side of the
capital. They fly at 650-716 m (staggered 22 m apart so crossing lines never meet, well above every
hill) at 58 m/s, about 90 s from coast to coast; each rolls and bobs gently, nose down, main and
tail rotors spinning (`draw_helicopters`).

The routes show on the map (M) as dashed lines with each helicopter on its line; yours is the
bright yellow one. At the top of the screen: **HELICOPTER 2 OF 4: WEST TO EAST | n ABOARD**.

## Aboard
You start in one of the four and ride at its open door; the camera sits back and up so you see
the helicopter and the island sliding past below. Look around freely. For the first 30 seconds
**R** (touch: **LOAD**) moves you to the next helicopter, so you can pick your route. **SPACE** (or the touch JUMP button,
or USE) jumps. Nobody rides past the last land under the line: over the last stretch a
countdown shows **LAST LAND AHEAD - AUTO JUMP IN n**, and everyone still aboard goes then.

## Freefall
`Skydiver` (pure logic; the game feeds it the ground height under you, roofs included):

- You leave the door with a little of the helicopter's speed and settle **belly-down, arms and
  legs spread** (a skydiving pose added to the skeleton, `BodyPose::skydive`, with the whole body
  tilted about the hips).
- Gravity against air resistance brings the fall up to **~46 m/s** with no jolt.
- **Forward glides**: ~31 m/s across at a slower fall (~33 m/s) - this is how you reach a far town.
- **Look down and push forward to dive**: down at up to ~68 m/s, head-first, to land first.
- **Back** holds you over a spot; the stick's sideways drift moves you sideways.
- The body turns to face where you look. The camera rides above your back; the view widens and
  shakes slightly at speed; the wind roars louder and higher the faster you fall.
- An **altimeter** on the right shows your height above the ground, your speed, and an orange mark
  at the automatic-opening height.

A full glide from the door reaches well over 700 m from the line.

## The parachute
**SPACE / JUMP opens it** (after the first second out of the door, and not lower than 18 m), and
it **opens by itself at 150 m** above whatever is below. It bursts out of the pack and fills over
1.7 s (`chute_open.wav`, a buzz on phones), snatching the fall down to a glide - smoothly, under
12 g. Then:

- It **steers toward where you look** (up to ~1.15 rad/s) and with the stick, **banking into
  turns**.
- **Forward** speeds it up (17.5 m/s across, sinking 9.8 m/s), **back** brakes (3.5 m/s across,
  sinking 4.6 m/s); hands off it glides at 11 m/s, sinking 6.8 m/s.
- You hang under it with your hands up on the toggles (`BodyPose::hang`), swinging like a
  pendulum when it speeds up or brakes.
- The canopy (`assets/verda/aircraft/parachute.glb`) is a ram-air wing in Verda's green and
  white, with its lines.

Touchdown is soft (under 8 m/s); you land on roofs too. Over the sea the water is the ground.

## The bots
The 99 bots are placed within gliding reach of one of the routes (`BattleRoyaleBots::start` with a
preference), each near a town, then `plan_bot_jumps` gives each one a jump: from the helicopter
whose line passes nearest its spot (`nearest_flight`), a little before it passes, falling at the same speeds, opening
at 150 m and landing exactly there. Until it lands a bot is *mounted* (hidden, it does not think,
shoot or get hit); in the air it is drawn as a small figure, then under its canopy
(`draw_bot_in_air`). Once all of them are down, up to 14 take cars toward the zone.

A hundred brains stay cheap: bots more than 260 m from you think and move one frame in four (taking
turns, with the time they skipped), and every frame when a sound reaches them.

## Seeing the island from up high
While dropping, the land is streamed farther round the camera: 6 rings of 128 m chunks instead of
3, coarser beyond the third (`TerrainWorld::set_radius`). Houses, roads and street trees draw
farther with it, up to 2.2 times, but never past the edge of the streamed land. (An earlier build
let them reach past it, so distant roads and houses hung in the sky.) The near clip plane moves out
to 0.6 m so the depth buffer stays precise, and roads are lifted a little so the coarse far terrain
does not swallow them. The lift is never recorded into the cached road meshes. An earlier build did
record it, and roads drawn for the first time during the drop stayed metres up in the air after
landing.

## Rules
- Everyone drops with **only a pistol** (a V9, loaded, and two spare magazines of 9mm): no
  rifle, no grenades, no water. Everything else is found on the ground (`LootSession::start`).
- The gas cannot touch you while you ride or fall (the route starts out over the sea); the first
  circle waits two minutes while the helicopters fly.
- No shooting, reloading or vehicles until you land; looking around always works.
- Loot appears around you once you are on the ground.
- Dev: `OUTLAND_DEV_NO_DROP=1` (or an `OUTLAND_DEV_SPAWN`) starts Battle Royale on the ground,
  as before.

## Models and sounds
- `tools/generate_drop_models.py` writes `assets/verda/aircraft/drop_helicopter.glb` (an olive
  transport helicopter in Verda's colours, both side doors open, bench seats, the flag on the
  boom; the rotors are drawn by the game) and `parachute.glb`. CTest checks they are current.
- `tools/generate_weapon_audio.py` adds `rotor_loop.wav`, `wind_loop.wav` and `chute_open.wav`.
