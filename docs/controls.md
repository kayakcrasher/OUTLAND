# Controls, settings and the pause menu

OUTLAND plays like other shooters: on a PC with the keyboard and mouse, and on phones and tablets
with on-screen buttons and swipe-to-look. Both can be changed from the pause menu.

## A clear screen

The play screen keeps to what you need, and the rest shows only when it matters:
- **USE** appears on touch screens only with something in reach: a car, a pickup, a counter, a
  mannequin, a door to join at, the Chief.
- **NADE** appears only with a grenade to throw, and **HEAL** only when you are hurt and carry
  something to mend with, eat or drink.
- Always on a touch screen: the movement stick, swipe-to-look on the right, both FIRE buttons, AIM,
  JUMP, LOAD, CROUCH, SPRINT, GUN and BAG.
- **MORE** (where VIEW was) folds away the rarely used ones: tap it and VIEW, the sight (IRON / DOT
  / 2X / 4X) and the fire mode (AUTO / SINGLE) appear for a few seconds.
- On a touch screen the gun and its ammo are one short line under the health, not a panel under the stick.
- The money shows as a short "250 kr". There is no mode or camera label, and no standing list of
  keys (they are below and in SETTINGS). The FPS counter is in development builds only.
- The driving keys show for the first few seconds at the wheel.
- A gang's banner shows for a few seconds as you walk onto its turf, and again at its door.
- Name tags show over people within about 13 m.

**Doors:** a house's front door opens by itself as you walk up to it and swings shut behind you.
Shut, it is solid: no walking through it. Residents open the doors as they reach them too.

## PC: keyboard and mouse

The mouse looks around. The game captures the pointer while you play and lets it go in menus.
In a browser the first click captures it ("CLICK TO PLAY"). The browser's own ESC frees the
pointer, and that opens the pause menu.

| Action | Keys (default) |
|---|---|
| Move | W A S D (or the arrow keys) |
| Look | mouse |
| Fire | left mouse button |
| Aim down sights | right mouse button (hold, or toggle in SETTINGS) |
| Reload | R |
| Use / enter or leave a vehicle | E or F |
| Jump (handbrake when driving) | SPACE |
| Sprint (boost when driving) | LEFT SHIFT |
| Crouch | C or LEFT CTRL |
| Switch weapon | Q or the mouse wheel: through the guns you carry, then to empty hands (UNARMED), then back to the first gun |
| Throw a grenade | G |
| Fire mode (auto / single) | B |
| Change sight | X |
| Bag / inventory | TAB or I |
| Camera view (first / third person) | V |
| Map | M |
| Build (Explore) | K. While building: left click places, right click removes, the wheel picks the piece |
| Pause menu | ESC |
| Spray a gang tag (Explore, gang members) | T |
| Emergency lights (police car) | L |
| Car radio: next station, then off | O (touch: RADIO, left of HAND at the wheel). See [radio](radio.md) |
| Radio for backup (Explore, police officers) | Z (touch: BACKUP, where TAG goes) |
| Arrest a Krimulo (Explore, police officers) | E / USE beside him |
| Mute | N |

Every action except ESC and N can be rebound on the **CONTROLS** page. Each action has two slots:

- Click a slot, then press the new key or mouse button. A key moves to its new action; it is never bound twice.
- ESC cancels and BACKSPACE clears the slot.
- **RESET KEYS** brings back the defaults.

The bottom-left hints (`LMB FIRE  RMB AIM  R RELOAD  Q SWAP`) follow the bindings.

## Phones and tablets

- **Move:** the left thumb works the stick. Put it down anywhere in the lower left, off the buttons, and the stick appears under it.
- **Look:** swipe anywhere on the empty right side of the screen, as in PUBG Mobile. There is no look stick. The view turns by how far the finger slides: half the screen's height is 90° at sensitivity 1.0. A still finger doesn't turn. The same works while building. At the wheel, swiping above the pedals looks around the car. Only the developer Creator keeps a look stick.
- **Fire:** hold FIRE. Sliding the thumb on FIRE also steers the sights, so you can aim while you shoot.
- **Aim down sights:** tap AIM to toggle it, or hold it.
- The other buttons are unchanged.
- **MENU** (top right, where AUDIO was) opens the pause menu.

The on-screen controls appear when the game runs on a phone or tablet: Android, or a browser whose main pointer is a finger. They also appear as soon as a touch screen is used. SETTINGS can turn them on or off for good.

## Aiming down sights
Aim (right mouse, or the touch AIM button) brings the gun up to your eye in either camera, as in
PUBG: you look through its sights - iron sights, a red dot or a scope. On iron sights the
crosshair gives way to the sights and the edges of the view darken a little. The iron sights are
built on each gun's sight line, so the picture lines up with where the shot goes: the rifle and SMG
have a front post in protective ears with a tritium dot, seen through a rear peep ring; the pistol
has three-dot sights (a notch and a post); the shotgun a brass bead on a raised rib with a mid bead.

## The Battle Royale drop
Aboard a helicopter: **SPACE** / the touch **JUMP** button (or USE) jumps; for the first 30 s **R** / **LOAD** moves you to the next of the four helicopters (each flies its own route). In freefall the move
stick glides (forward), dives (forward while looking down) or holds back; **SPACE / JUMP opens the
parachute** (it opens by itself at 150 m). Under the canopy it steers where you look, forward
speeds up, back brakes. Shooting waits for the ground. Details: `docs/battle-royale-drop.md`.

## Pause menu

ESC, the **MENU** button, or losing the mouse pointer opens it, and the world stops while it is open.

- **RESUME** continues. The click doesn't fire: a mouse button still held from a menu does nothing until it is let go.
- **SETTINGS** holds the rows below.
- **CONTROLS** is the key bindings, as above.
- **SAVE GAME** saves now ("GAME SAVED"): the bag, money, weapons, vehicles, your builds and, in the campaign, where you are in the story and on the island. The campaign also saves itself (see [campaign.md](campaign.md)).
- **RESTART** saves, then starts the mode you are in again from the beginning: a new match, or the campaign from your last save. Tap it twice ("TAP AGAIN TO RESTART") so a stray tap never throws a match away. Not in a LAN match.
- **QUIT TO MENU** saves and goes back to the home screen.
- **QUIT GAME** saves and closes the game. It is on PCs only; a browser tab or a phone app is closed its own way.

ESC goes back a page, and from the first page resumes.

| Setting | Values | Default |
|---|---|---|
| Touch layout | EDIT: drag each on-screen button where your thumbs want it, and make it bigger or smaller (- / +, 50–200%). There is one set for on foot and one for at the wheel. RESET puts back one button, RESET ALL every one; DONE keeps the layout (saved with the settings), CANCEL leaves it. RESET CONTROLS also resets the layout | the default layout |
| Mouse sensitivity | 0.1–4.0: radians per pixel = 0.0012 × setting (about 0.07° per pixel at 1.0) | 1.0 |
| Touch look sensitivity | 0.2–3.0 (half the screen's height = 90° × setting) | 1.0 |
| Aiming sensitivity | 0.30–1.50, on top of the zoom's own slow-down while aiming | 0.80 |
| Invert look up/down | on / off | off |
| Aim (right mouse) | HOLD / TOGGLE | HOLD |
| Touch aim assist | on / off: near an enemy the look slows, and while aiming or firing it drifts onto them (touch only) | on |
| On-screen controls | AUTO / ON / OFF | AUTO |
| Touch button size | 70–140% | 100% |
| Vibration | on / off (phones) | on |
| Volume, Sound | 10% steps; on / off | 80%, on |
| Car radio | the radio's volume (on top of Volume), 10% steps | 70% |
| Graphics | AUTO / FAST / SHARP | AUTO |

- **RESET CONTROLS** restores the defaults for everything except the sound and graphics.
- Settings save to `profiles/settings.cfg` with the key bindings (`bind fire -1 0`; mouse buttons are negative and the wheel is -20).
- The old look stick's `look_sensitivity` line is ignored, because swipes and the mouse are measured differently.

## Feel: vibration and sound

On phones, vibration runs in the Android app and in Android browsers. iPhones' Safari can't vibrate from a web page. One buzz runs at a time; the strongest request wins, and a weaker one never cuts a stronger one short.

| What happens | Vibration | Sound |
|---|---|---|
| Your gun fires | a kick per shot: SMG lightest, shotgun and sniper heaviest | the report, a low punch under it, the report rolling back off the hills, the case |
| You are hit | a jolt, longer for bigger hits | the hit (and the armour plate) |
| Your round hits someone | — | the hit tick and a body thwack (sharper for a headshot) |
| A kill | a pulse | the kill chime |
| Your car crashes | in proportion to the impact | a light knock, or a heavy crunch with breaking glass |
| A grenade goes off | the closer, the longer and harder (within 40 m) | the blast; within 14 m the ears ring |
| A hard landing | a thud (from a fall, not an ordinary jump) | a landing thud |

The code is in `game::feel::Haptics` (`outland_haptics_tests`), `platform::vibrate` (JNI to `GameActivity.vibrate` on Android, which needs the `VIBRATE` permission, or `navigator.vibrate` in browsers) and `GunAudio`. The new sounds come from `tools/generate_weapon_audio.py`.

## Gunplay

The view takes 65% of each shot's kick, so a spray is easier to hold on target. The rest of the kick still shows in the crosshair's spread.

## Code

- `input::Bindings` (`include/outland/input/Bindings.hpp`): the actions, the defaults, their names, and rebinding.
- `input::InputSystem`: touch roles, swipe and fire-drag look, the floating stick, mouse look, and the bindings. `PlayerInput::look_dx/look_dy` are this frame's turn in radians; `look_x/look_y` remain for the Creator's look stick and the driving camera pan.
- `input::PointerEvents`:
  - capturing the mouse (desktop: a disabled cursor; browsers: pointer lock, with motion summed between frames and the jumps at the start of a lock dropped);
  - telling a touch screen apart;
  - buffering clicks.
- `game::GameSettings` (`include/outland/game/Settings.hpp`) and `game::PauseMenu`.
- Tests:
  - `outland_combat_input_tests`: swipe, fire-drag, floating stick, mouse look, held buttons after a menu, aim toggle, rebinding.
  - `outland_pause_menu_tests`: the layout at PC and phone sizes, and default keys with no key bound twice.
  - `outland_zones_map_tests`: settings and key bindings survive a save; bad values are clamped.
