# Mobile controls handoff

Asset integration follow-up: see [downloaded-assets-handoff.md](downloaded-assets-handoff.md).
The build counts below record the original mobile-controls validation.

Based on checkpoint `0a11e6e` (persistent editable gameplay markers). No changes to
Creator serialization, buildings, assets, roads, marker gizmos or world collision.

## Controls and architecture

The existing flow remains `TouchLayout -> InputSystem -> PlayerInput -> Renderer /
WeaponSystem`, with `TouchHUD` drawing the same normalized layout. The live player
is Renderer’s `PlayerState`; the separate core `game::Player` remains unchanged.

- Left stick moves with preserved analog magnitude; right stick changes yaw/pitch
  at the existing sensitivity and frame-time-scaled rate. Full movement no longer
  implicitly sprints. HUD hit testing and drawing share `touch_scale`.
- Jump/vault, Reload, Inventory/BAG, Interact/USE, weapon switch and view are press
  actions. Sprint, Crouch, Aim and Fire are held actions. Crouch slows movement,
  lowers camera height, and takes priority over sprint. Existing weapon handling
  consumes Fire/Aim/Reload. Successful vaults face movement/camera forward and
  suppress the following jump impulse.
- Contacts acquire one role on touchdown using stable raylib IDs, never array
  indices or subsequent hit tests. A stick has one owner; other contacts cannot
  steal it. Role persists even beyond the control boundary. Off-control contacts
  never slide into controls or fire. New IDs independently generate action edges.
- Native touch suppresses mouse polling to avoid Android’s synthesized duplicate
  input. X11 mouse captures a HUD role on left press; right mouse aims. Keyboard:
  WASD, Space, Shift, C, F, Q, R, E, I, Tab and V remain available.
- Mode, focus and viewport changes quarantine held contacts until release.
  Equipment opening/closing explicitly cancels owners. Home tracks contacts
  without gameplay input, preventing a menu press from leaking into gameplay.
- DEV Creator reserves toolbar/hotbar/drawer regions before input capture and
  retains the navigation sticks/keyboard. Gameplay combat and action buttons are
  suppressed, including combat HUD hints. Its drawer blocks navigation. Creator buttons now fire once per
  press; rotate intent is applied once by the controller/renderer, not twice.
- Equipment is a modal panel showing both existing weapons and their magazines.
  BAG/I closes; GUN/Tab equips the next weapon. Other player actions are cleared.
- USE/E finds an enabled gameplay marker within 3 metres. A LootSpawn replenishes
  one magazine of the selected weapon’s reserve, capped at its starting reserve.
  Successful pickups are tracked by settlement/marker ID for this session only;
  full reserves do not consume the pickup. Other marker types show identification.
  Returning home and selecting a mode resets pickups with the existing loadout.
- Escape now returns to Home from a game, then exits from Home; the renderer owns
  this routing rather than raylib’s default immediate Escape shutdown.

## Every changed file

| File | Change |
| --- | --- |
| `CMakeLists.txt` | Export raylib includes/library paths to consumers; register DEV Creator touch tests with wrapped raylib input polling. |
| `include/outland/input/PlayerInput.hpp` | Add the frame inventory intent. |
| `include/outland/input/TouchLayout.hpp` | Add Sprint/Crouch/BAG/USE layouts and shared viewport scaling. |
| `include/outland/input/InputSystem.hpp` | Ownership state, gameplay/modal/region routing API and cancellation. |
| `src/input/InputSystem.cpp` | Stable contact capture, action edges, mouse deduplication, missing bindings and transition quarantine. |
| `include/outland/input/TouchHUD.hpp` | Navigation-only Creator rendering option. |
| `src/input/TouchHUD.cpp` | Draw added controls, share scale and suppress gameplay buttons in Creator. |
| `include/outland/creator/CreatorTouchUI.hpp` | Fresh-press tracking and reserved-region query. |
| `src/creator/CreatorTouchUI.cpp` | Press edges, modal drawer routing, synthesized-mouse exclusion and single rotate intent. |
| `src/engine/render/Renderer.cpp` | Route Creator before gameplay; equipment, crouch, interactions, focus/home handling and vault/Exit-key fixes. |
| `include/outland/game/combat/WeaponSystem.hpp` | Per-weapon magazine query and ammo collection API. |
| `src/game/combat/WeaponSystem.cpp` | Bounded reserve pickup using existing magazines. |
| `tests/integration/world/test_combat_input.cpp` | Stable/nonsequential IDs, contact reorder/crossing/release, stick ownership, all added actions, X11/native deduplication, modal/Creator/resize cancellation and small-screen analog geometry. |
| `tests/integration/creator/test_creator_touch_ui.cpp` | New DEV test for once-per-press tools, held drawer toggle, modal exclusion and Android mouse deduplication. |
| `tests/integration/world/test_combat.cpp` | Finite/full/unlimited reserve pickup checks. |
| `tests/integration/world/test_verda_environment.cpp` | Correct stale hollow-house collision fixture and check forward-facing window vaults. |
| `docs/mobile-controls-handoff.md` | This handoff. |

## Validation

Built raylib 5.5 locally because this worker lacked raylib, CMake and development
headers. Dependencies/build artifacts were installed under `/tmp`, outside the repo.
Repeated Debug DEV and Release non-DEV builds used GCC 14, C++20, CMake and pkg-config:

```sh
cmake -S . -B build-dev -DOUTLAND_DEV_TOOLS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-dev -j4
ctest --test-dir build-dev --output-on-failure
cmake -S . -B build-release -DOUTLAND_DEV_TOOLS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j4
ctest --test-dir build-release --output-on-failure
```

In this worker, set `PKG_CONFIG_PATH=/tmp/outland-raylib-install/lib/pkgconfig` and
use `/tmp/outland-build-dev` or `/tmp/outland-build-release` as the build directory.
DEV: **7/7 passing**; non-DEV: **5/5 passing**. Existing Creator map round-trip and
asset validation pass. The old environment collision assertion failed identically
in a separately extracted untouched checkpoint; its fixture was updated to test
the rotated shell wall and playable interior. Production collision was untouched.

The input regression executable also passed AddressSanitizer + UndefinedBehaviorSanitizer:

```sh
c++ -std=c++20 -g -fsanitize=address,undefined -fno-omit-frame-pointer -UNDEBUG \
  -I include -I /tmp/outland-raylib-install/include \
  tests/integration/world/test_combat_input.cpp src/input/InputSystem.cpp \
  -o /tmp/outland-input-sanitized
/tmp/outland-input-sanitized
```

X11 smoke uses Xvfb and real raylib/OpenGL rendering, with HUD/keyboard interaction
and captured screenshots under `/tmp/outland-final-*.png`. No audio playback device
exists here; audio’s existing mock integration tests pass. `git diff --check` passes.

## Next coworker / device checks

- No Android device, NDK/APK build, or physical multitouch was available. Test native
  Android finger IDs/cancel/background/resume with movement + look + Fire/Aim and
  simultaneous Jump/Reload. Test Termux:X11 with keyboard plus mouse HUD dragging.
- Crouch currently changes speed/camera, not the third-person model animation or a
  height-dependent collider. Sticks retain the existing fixed centers/deadzone.
- This is equipment for the existing two weapons, not a general item inventory.
  Loot pickup is session state; spawn/despawn, NPC conversations, vehicles,
  line-of-sight interaction rules and persistent inventory are still future work.
- The generated training region currently has no gameplay markers. USE provides
  empty-context feedback there; use live Creator-authored loot markers to exercise
  pickups after spending reserve ammo. The asset follow-up now enables saved-map
  loading in non-DEV gameplay.
- Tune ergonomics on real phone aspect ratios. Tiny windows may still overlap the
  existing top audio UI/Creator drawer or ammo panel; no broad UI redesign was made.
- Changes are on `codex/mobile-controls`, uncommitted and unpushed, for review.
