# Controls, settings and the pause menu

OUTLAND plays like other shooters: on a PC with the keyboard and mouse, and on phones and tablets
with on-screen buttons and swipe-to-look. Both can be changed from the pause menu.

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
| Switch weapon | Q or the mouse wheel |
| Throw a grenade | G |
| Fire mode (auto / single) | B |
| Change sight | X |
| Bag / inventory | TAB or I |
| Camera view (first / third person) | V |
| Map | M |
| Build (Explore) | K. While building: left click places, right click removes, the wheel picks the piece |
| Pause menu | ESC |
| Mute | N |

Every action except ESC and N can be rebound on the **CONTROLS** page. Each action has two slots:

- Click a slot, then press the new key or mouse button. A key moves to its new action; it is never bound twice.
- ESC cancels and BACKSPACE clears the slot.
- **RESET KEYS** brings back the defaults.

The bottom-left hints (`LMB FIRE  RMB AIM  R RELOAD  Q SWAP`) follow the bindings.

## Phones and tablets

- **Move:** the left thumb works the stick. Put it down anywhere in the lower left, off the buttons, and the stick appears under it.
- **Look:** swipe anywhere on the right of the screen. The view turns by how far the finger slides: half the screen's height is 90° at sensitivity 1.0. A still finger doesn't turn.
- **Fire:** hold FIRE. Sliding the thumb on FIRE also steers the sights, so you can aim while you shoot.
- **Aim down sights:** tap AIM to toggle it, or hold it.
- The other buttons are unchanged.
- **MENU** (top right, where AUDIO was) opens the pause menu.

The on-screen controls appear when the game runs on a phone or tablet: Android, or a browser whose main pointer is a finger. They also appear as soon as a touch screen is used. SETTINGS can turn them on or off for good.

## Pause menu

ESC, the **MENU** button, or losing the mouse pointer opens it, and the world stops while it is open.

- **RESUME** continues. The click doesn't fire: a mouse button still held from a menu does nothing until it is let go.
- **SETTINGS** holds the rows below.
- **CONTROLS** is the key bindings, as above.
- **SAVE GAME** saves the bag, money, weapons, vehicles and your builds now ("GAME SAVED").
- **QUIT TO MENU** saves and goes back to the home screen.
- **QUIT GAME** saves and closes the game. It is on PCs only; a browser tab or a phone app is closed its own way.

ESC goes back a page, and from the first page resumes.

| Setting | Values | Default |
|---|---|---|
| Mouse sensitivity | 0.1–4.0: radians per pixel = 0.0012 × setting (about 0.07° per pixel at 1.0) | 1.0 |
| Touch look sensitivity | 0.2–3.0 (half the screen's height = 90° × setting) | 1.0 |
| Aiming sensitivity | 0.30–1.50, on top of the zoom's own slow-down while aiming | 0.80 |
| Invert look up/down | on / off | off |
| Aim (right mouse) | HOLD / TOGGLE | HOLD |
| Touch aim assist | on / off: near an enemy the look slows, and while aiming or firing it drifts onto them (touch only) | on |
| On-screen controls | AUTO / ON / OFF | AUTO |
| Touch button size | 70–140% | 100% |
| Volume, Sound | 10% steps; on / off | 80%, on |
| Graphics | AUTO / FAST / SHARP | AUTO |

- **RESET CONTROLS** restores the defaults for everything except the sound and graphics.
- Settings save to `profiles/settings.cfg` with the key bindings (`bind fire -1 0`; mouse buttons are negative and the wheel is -20).
- The old look stick's `look_sensitivity` line is ignored, because swipes and the mouse are measured differently.

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
