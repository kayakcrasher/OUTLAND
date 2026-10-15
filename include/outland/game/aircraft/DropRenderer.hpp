#pragma once
#include "outland/game/aircraft/HelicopterDrop.hpp"
#include <raylib.h>

// Drawing the drop: the three helicopters with spinning rotors, parachutes, and the bots on their
// way down (seen from afar: a figure and its canopy). Inside BeginMode3D.
namespace outland::game::aircraft {

// The model-space rotor hubs of assets/verda/aircraft/drop_helicopter.glb (tools/generate_drop_models.py).
inline constexpr Vector3 rotor_hub{0, 3.75F, .2F}, tail_hub{.32F, 3.05F, -7.55F};
inline constexpr float rotor_radius = 7.4F, tail_rotor_radius = 1.15F;

// `helicopter` may be null (a stand-in shape is drawn). The helicopters lean nose-down and roll
// gently as they fly.
void draw_helicopters(const FlightPath& path, float seconds, const Model* helicopter, Vector3 camera);
// A parachute whose harness is at `harness`, facing `heading`; `open` 0..1 inflates it, `swing`
// and `bank` tilt it (radians). `chute` may be null.
void draw_parachute(const Model* chute, Vector3 harness, float heading, float open, float swing, float bank);
// A bot in the air: a simple figure (belly-down or hanging) and its canopy.
void draw_bot_in_air(const BotInAir& bot, const Model* chute, Color clothes);

} // namespace outland::game::aircraft
