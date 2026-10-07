# OUTLAND firearms and bullets

The original **V9 pistol** and **VR30 rifle** are usable in first and third person.
The pistol fires one round per press; the rifle fires automatically while held.
Both have procedural meshes, iron-sight details, an armed stance, muzzle flash,
gunshot audio, recoil, aiming zoom, movement/airborne spread and reload feedback.

## Ammunition

**Dev Lab has unlimited bullets with no magazine depletion or forced reloads.**
Both guns display `AMMO UNLIMITED - DEV LAB`. Other modes use normal ammunition:

| Weapon | Magazine | Reserve | Reload | Fire rate |
| --- | ---: | ---: | ---: | --- |
| V9 pistol | 12 | 60 | 1.6 seconds | Up to 250 rounds/minute; one press per shot |
| VR30 rifle | 30 | 120 | 2.1 seconds | 600 rounds/minute; hold to fire |

Reloads transfer available reserve; they cannot create rounds.
Switching cancels a pending reload without transferring ammunition.
Sprinting prevents firing; walking allows it. Aiming slows movement and tightens
spread. Full touch-stick deflection retains auto-sprint; keyboard walking now
requires Shift to sprint. Entering a new mode resets the loadout.

## Bullets and cover

Shots spawn projectiles with 350m/s pistol or 760m/s rifle initial speed, gravity
and drag. Damage decreases with retained bullet energy. There are at most 96
active bullets and 64 impacts, with no new GPU meshes created per shot.
Bullets expire after four seconds or 900 metres.

Small simulation steps and collision checks along the travelled segment stop
fast rounds skipping thin walls or targets. Closest contact wins: terrain,
the central training structure, rotated house walls and pitched roofs,
tree trunks, or live targets. Leaves do not block bullets. There is no
penetration or ricochet yet; trunks use simple upright cylinder colliders.
House doors/windows remain solid exterior decoration.

The camera centre selects an aiming point and the muzzle points toward it.
Spread applies to the shot; bullets still follow their own ballistic path.
Nearby cover is checked between player and muzzle, and the third-person camera
shortens when obstructed. Aim above distant targets to compensate for drop.
Recoil accumulates during firing and recovers between shots; look input can
counter it.

The nine range targets have 100 health. Head hits deal double damage.
Targets change colour, lose health and fall; hits show a marker and damage number.
Dev Lab resets depleted targets after four seconds; **T** resets them immediately.
Other modes leave them down until a new session. Enemy AI, armour and player
damage remain separate future systems.

## Controls

| Action | Keyboard / mouse | Touch / Termux X11 HUD |
| --- | --- | --- |
| Fire | F; left click outside the HUD | FIRE |
| Aim | Hold Q or right mouse button | Hold AIM |
| Reload | R | LOAD |
| Switch gun | Tab | GUN |
| Switch view | V | VIEW |
| Reset targets | T in Dev Lab | Automatic reset after four seconds |

The existing look stick remains. Keyboard fire/aim can be held while dragging
that stick in X11. Native touch supports simultaneous look/aim/fire; X11
inherits the platform's single-pointer limits. LOAD/GUN trigger once per press.
Audio mute/volume affects combat sounds too.

## Assets and testing

The asset generator also exports original pistol/rifle OBJ assets with three
LODs and the shared Verda palette. Gun pivots are at the muzzle, barrel direction
is +Z, Y is up and units are metres. The game draws its procedural guns directly.
Gunshots, reload, hit and empty-trigger clicks are original synthesized
sound-design sketches. A small sound pool supports overlapping gun reports.

From the repository root:

```bash
bash scripts/build_verda.sh
bash scripts/run_x11.sh
```

Choose Dev Lab, training ground (**1**). Step beside the central structure
(for example X=8) for a clear shot at the range boards; it blocks the central lane.
Check automatic/semi fire, head/body hits, nearby walls, both camera views, and
long-range drop. Hold rifle fire past a magazine in Dev Lab: firing must continue.
Then enter Explore to check finite ammo, empty trigger, reload time and reserve
consumption. Try LOAD/GUN via touch/X11. Report firing FPS, crosshair mismatch,
audio distortion and visual issues.

Headless tests cover swept/nearest collision, roof/rotated-wall cover, drop,
travel time, damage, fire cadence, magazines/reserves, reload interruption,
sprint lockout, unlimited Dev ammo and target resets. Input tests cover native
touch/X11 press edges, keyboard/mouse actions, walking/sprint and UI exclusion.
Audio tests include combat muting and cleanup. Asset validation checks all 45
OBJ LODs and 15 WAVs. Syntax checks use raylib 6.0.

Live graphics, listening and FPS testing still need Termux/X11: the cloud
workspace lacks the raylib library and CMake.
