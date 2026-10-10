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
