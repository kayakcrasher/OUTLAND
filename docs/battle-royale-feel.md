# Battle-royale feel: performance, gunplay, controls, driving

This round tuned Outland toward how PUBG plays. It was built from how that game behaves, not from its code: PUBG's code is proprietary and wasn't decompiled.

## Performance

Measured in the software-rendered desktop build (a stand-in for a weak phone GPU). Turn on `OUTLAND_PERF=1` to see the timing panel and a log line every two seconds.

| Mode | Before | After |
|---|---|---|
| Explore downtown | 40 fps | ~50 fps |
| Battle Royale | ~26 fps (bots alone 20–25 ms a frame) | ~45 fps (bots 1–2 ms) |

- **Dynamic resolution:** the 3D world draws into a scaled render target and is stretched to the screen; the HUD stays at full resolution.
  - The scale lowers when frames are slower than about 50 fps and the GPU is the bottleneck. It rises again above about 58 fps.
  - `OUTLAND_RENDER_SCALE=0.6` fixes the scale (allowed range 0.35–1).
- **4× MSAA is off by default.** On phones it multiplies the pixel work of every frame. `OUTLAND_MSAA=1` turns it back on.
- **Static meshes:** procedural houses, street trees, roads and the Esperanto signs are recorded once into GPU meshes (`engine::GeometryRecorder`, `engine::StaticWorld`). Before, they were rebuilt vertex by vertex every frame.
  - New recordings are capped at 6 per frame, so arriving somewhere new never stalls.
  - The map builder still draws them directly while editing.
- **Collision index:** `WorldCollision` keeps a 16 m spatial index of static assets. Walking, driving and bot queries now test nearby assets instead of all ~1,700.
  - Vehicles are checked separately because they move.
  - The index rebuilds when assets are added or removed. Map-builder edits call `WorldCollision::invalidate_index()`.
- **Bots:** looking for a flanking spot used to simulate a full collision walk to each candidate; that was 81% of bot time. Now a bot only checks whether the spot is standable and visible, and searches at most every 1.5 s.

## Gunplay

- **Recoil moves your aim.** Each shot lifts `player.pitch` and kicks it sideways at random. The climb grows through a spray (+7% per shot for the rifle, up to 10 shots), and you pull it down.
  - When you let go, about 55% of the climb settles back by itself.
  - Crouching reduces recoil by 20%. Moving and jumping make it worse.
- **Spread:**
  - Hip fire is wide and blooms as you keep shooting.
  - Aimed fire is tight, and the first aimed shot is near perfect.
  - Moving and being in the air widen both.
- **Aiming down sights zooms per weapon:** rifle 46° field of view, pistol 55°. Look sensitivity slows in proportion to the zoom.
- **Damage zones:** heads ×2.35 (rifle) or ×2.1 (pistol), legs ×0.9. Bots use the same zones.
- **Weapon stats:**

  | Weapon | Damage | Fire rate | Muzzle velocity | Magazine |
  |---|---|---|---|---|
  | VR30 rifle | 41 (three body hits kill an unarmoured player) | 700 rounds/min | 880 m/s | 30 |
  | V9 pistol | 35 | semi-automatic, 0.135 s between shots | 380 m/s | 15 |

  Bullet drop and travel time were already simulated.
- **Fire mode:** the AUTO/SINGLE button (keyboard **B**) switches automatic weapons between full-auto and single shots.

## Controls

- **Left FIRE button** above the movement stick: the left thumb fires while the right thumb keeps aiming.
- **Aim button:** a tap toggles aiming down sights, a hold aims only while held. Sprinting drops the sights.

## Driving

| Control | Touch | Keyboard |
|---|---|---|
| Handbrake | JUMP | Space |
| Boost | SPRINT | Shift |
| Parking brake | CROUCH | — |
| Horn | — | H |

- **Drift:** the car's velocity lags its heading, and tyre grip pulls the two back together. The handbrake almost removes rear grip and swings the tail round.
- **Engine:** strong pull off the line that tapers toward top speed, plus air drag.
  - Boost gives +45% pull and +12% top speed.
  - The hatchback tops out at 130 km/h, up from 100.
- **Crashes:** damage grows with impact speed, and a crash above about 50 km/h also hurts the driver.
- Residents' autopilot brakes with the foot brake, so it never drifts into pedestrians.

## Tests

- **`outland_combat_tests`:** recoil grows through a spray, crouching steadies it, fire-mode toggling, and the zone and zoom values. The tests also reflect the new damage, magazine and fire-rate numbers.
- **`outland_vehicle_tests`:** boost pulls harder, the handbrake produces several times more sideways slip, and a wall crash costs health and stops the car.
- **`outland_traffic_tests`** still checks that resident cars stop for someone in the road.

## Combat feedback, aim assist and grenades

`game::combat::CombatFeedback` (`src/game/combat/CombatFeedback.cpp`) holds what the player feels
in a fight; the renderer draws it in every mode, not only Battle Royale:

- **Damage numbers** float up from whatever the player hit. Pellets and bursts on one body merge
  into one number. Headshots are orange; the killing blow is red with an X.
- **Kills** put a red cross over the crosshair, labelled ELIMINATED, then DOUBLE KILL, TRIPLE KILL
  or RAMPAGE for kills within 4 s of each other.
- **Being hurt** by anything flashes the screen edges red and shakes the camera. That covers bot
  and police shots, zombie bites, grenades, crashes and the zone. When there is an attacker, an
  arrow points to it. Under 30% health, the edges pulse.

**Touch aim assist** applies only when a touch screen is in use. With a hostile near the crosshair
(within 7° from the hip, 4° aimed, plus their body width), the look slows to 55%. While aiming or
firing, the sight drifts onto their chest. Walls hide targets from the assist. Hostiles are Battle
Royale bots, hostile and creature pools (zombies), and police who are chasing you.

**Frag grenades** (`src/game/combat/Grenades.cpp`):
- **Throwing:** G, or the NADE button between AIM and the look pad. A level lob lands about 25 m
  away; grenades bounce off the ground and walls.
- **Blast:** 2.8 s fuse. Lethal (160 damage) within 2.5 m, falling to nothing at 8.5 m. Walls
  shield people from the blast, and it hurts the player too, unless they are in a vehicle.
- **Targets:** it damages bots, residents (a crime if seen), police, zombies and cars, and bots
  hear it.
- **Supply:** grenades are a bag item, `frag_grenade` (stacks of 6; the model comes from
  `tools/generate_grenade_model.py`). The NADES count is the bag's, and each throw takes one from
  it.
  - A new bag starts with 2. Battle Royale starts a new bag every match; Explore and Zombie
    Survival keep their saved bags.
  - Loot: weapons caches (1-2), general loot, and zombie-mode camps.
  - Shops: general stores sell them at 45 kronoj (they buy them back at 18; 6 in stock).
  - The Dev Lab has 99.

Bots are less accurate past 40 m: their aim cone widens by 1/80 per metre beyond that. Bot and
police guns flash as they fire.

## Sights
Guns start on iron sights (the rifle's holographic sight and the SMG's red dot are no longer built
in); the sniper keeps its own fixed scope. Three optics are bag items (`utility`, stacks of 3),
found in weapons caches (red dot weight 8, 2x 6, 4x 3) and now and then in general loot, and sold
at general stores (red dot 60, 2x 90, 4x 150 kronoj):

| Optic | Fits | Aiming |
|---|---|---|
| Red dot | every gun but the sniper | the gun's own aim zoom, a red dot in an open reflex window |
| 2x scope | rifle, SMG, shotgun | 2x the hip view (38.6° FOV), a red dot inside a red ring |
| 4x scope | rifle, SMG | 4x (19.9° FOV), an illuminated chevron with 200–500 m drop marks |

Each copy in the bag can sit on one gun. A newly found optic goes on the held gun if it still wears
irons, else on another gun that does (as battle royales attach a pickup to an empty slot). X on a
keyboard, or the SIGHT button (between BAG and VIEW, labelled IRON, DOT, 2X or 4X), cycles the held
gun through the optics it fits and the bag holds. Selling or dropping an optic takes it off its
gun. Scopes are viewed through, filling the screen like the sniper's; the gun model shows the
mounted optic on its rail. Look speed while aiming scales with the zoom. The Dev Lab has every
optic. Logic: `game/combat/Sights` (tested by `outland_sight_tests`); models: `tools/generate_sight_models.py`.

## Bots on the road
At the start of a match, up to seven bots that dropped within 140 m of a running car take it
(`game::ai::BotDrivers`) and drive the roads in toward the zone (a road-graph route to a point
well inside the circle), at up to ~60 km/h on the straight and slower on bends, backing off when
stuck. A driver gets out and fights on foot when it arrives, when the player is within 55 m and
in sight, after four failed tries at getting unstuck, or after four minutes. While driving a bot
is *mounted*: it does not think or shoot, other bots cannot hit it, its body is hidden and its
car takes the bullets - shots through the windows (or into the cabin) hurt the driver, and a
wrecked car throws it out hurt. The car is handed back at the end of the match.

## The zone wall
The circle's edge stands in the world as a wall of blue light (`game::draw_zone_wall`): a curtain
120 m tall following the ground, strong at the foot and fading upward, streaks drifting along it,
a bright band along the ground; only the stretch within ~950 m of the camera is drawn. Outside
it the screen edges glow blue, pulsing, with "OUTSIDE THE ZONE - n m TO SAFETY - n HP/s", and
the zone bites harder as it closes: 6 HP/s at first, up to 24 at the end (`zone_damage`). Dev:
`OUTLAND_DEV_ZONE=<metres>` starts the circle smaller.

## Loot
- **Rarity**: common (white), uncommon (green: pistol, rifle, red dot, grenades, bandages),
  rare (blue: SMG, shotgun, 2x, light vest, antibiotics, backpack) and epic (purple: sniper, 4x,
  plate carrier) - `rarity_of`. Anything better than common glows in a ring of its colour, turns
  and bobs; rare and epic finds send up a beam seen from 140 m. The pickup prompt shows the tier.
- **Loot in buildings**: every enterable building (kit buildings, procedural and model houses,
  Main Street rows) has a loot spot on its ground floor - 55% general, 20% medical, 25% weapons;
  541 loot spots on the island, up from 43. Pickups are capped at 4096.
- **Fallen bots drop their kit**: their gun (loaded), two magazines of ammo, and sometimes
  bandages, a vest or a grenade.
- **Armour**: a light vest (armour I, 30% less damage) and a plate carrier (armour II, 45%) from
  weapons caches, general loot, bots and stores (120 / 260 kronoj). The best vest in the bag
  soaks bullets, blasts and bites (not the zone or crashes); the HUD shows ARMOUR I / II.
  Models: `tools/generate_armour_models.py`.
