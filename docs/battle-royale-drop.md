# The Battle Royale drop

Every Battle Royale match starts in the air. A **row of three helicopters** flies a straight line
across Verda, everyone aboard, and each player jumps when the line passes over where they want to
land, falls belly-down, opens a parachute and glides in. Code: `game::aircraft` (`HelicopterDrop`
for the logic, `DropRenderer` for drawing), wired into `Renderer.cpp`. Tests:
`outland_helicopter_drop_tests`.

## The route
`make_flight(seed)` picks one of **eight routes**, a fresh one every match:

| | |
|---|---|
| north to south | south to north |
| east to west | west to east |
| south-west to north-east | north-east to south-west |
| north-west to south-east | south-east to north-west |

The line crosses the whole island, starting and finishing out over the sea, up to 550 m to one
side of the capital so it is not always over Verda City. The helicopters fly at 650 m (well above
every hill) at 58 m/s, about 90 s from coast to coast. They fly **three abreast, 42 m apart**, the
middle one a little ahead and the outer two a little lower; each rolls and bobs gently, nose
down, main and tail rotors spinning (`draw_helicopters`).

The route shows on the map (M) as a yellow dashed line with the three helicopters on it, and at
the top of the screen: **ROUTE: WEST TO EAST | n ABOARD THE 3 HELICOPTERS**.

## Aboard
You ride at the middle helicopter's open door; the camera sits back and up so you see the whole
row and the island sliding past below. Look around freely. **SPACE** (or the touch JUMP button,
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
The 23 bots are placed within gliding reach of the route (`BattleRoyaleBots::start` with a
preference), each near a town, then `plan_bot_jumps` gives each one a jump: from the helicopter on
its side of the line, a little before the line passes its spot, falling at the same speeds, opening
at 150 m and landing exactly there. Until it lands a bot is *mounted* (hidden, it does not think,
shoot or get hit); in the air it is drawn as a small figure, then under its canopy
(`draw_bot_in_air`). Once all of them are down, some take cars toward the zone.

## Seeing the island from up high
While dropping, the near clip plane moves out to 0.6 m (the depth buffer stays precise), houses,
roads and street trees draw up to 2.4 times further, and roads are lifted a little so the coarse
far terrain does not swallow them.

## Rules
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
