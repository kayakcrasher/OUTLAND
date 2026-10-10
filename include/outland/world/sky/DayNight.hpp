#pragma once
#include <raylib.h>
#include <vector>

namespace outland::world { class VerdaRegion; }

// Time of day for Verda. The renderer has no lighting shader (phones), so day and night are made
// from three cheap pieces driven by one SkyState: a sky gradient drawn behind the world, a single
// full-screen multiply by the ambient colour after the world, and additive light sprites on top
// (lamp pools and bulbs, fire glow, headlights, moon and stars) that the multiply cannot dim.
namespace outland::world::sky {

struct SkyState {
    float hour{12};
    Color zenith{}, horizon{};     // what the sky should look like on screen
    Color ambient{WHITE};          // world multiply: WHITE at noon, deep blue at midnight
    float daylight{1};             // 0 night .. 1 full day
    float lamps{0};                // street lamps and windows: 0 off .. 1 fully lit
    float stars{0};                // 0 .. 1
    Vector3 sun{0, 1, 0}, moon{0, -1, 0}; // unit directions toward them
    Color sun_color{WHITE};
};

// Pure function of the hour (0..24, wraps). Continuous: no visible jumps from minute to minute.
SkyState sky_at(float hour);
// Colour that, after the world multiply by `ambient`, shows as `wanted` (for drawing the sky first).
Color before_multiply(Color wanted, Color ambient);

enum class LightKind : unsigned char { StreetLamp, Fire };
struct LightSource {
    LightKind kind{LightKind::StreetLamp};
    Vector3 bulb{};    // where the glow is
    Vector3 ground{};  // centre of the pool of light on the ground
    float radius{8};   // pool radius
};
// Every light-emitting world asset (street lights, fires). Cheap enough to rebuild when the map changes.
std::vector<LightSource> collect_lights(const VerdaRegion& region);

// Draw helpers (inside BeginMode3D). They set and restore additive blending themselves.
void draw_sky_objects(const SkyState& sky, Vector3 camera);                 // sun, moon, stars
void draw_lights(const std::vector<LightSource>& lights, const SkyState& sky, Vector3 camera, float time_seconds);
void draw_headlights(Vector3 car, float yaw_degrees, const SkyState& sky);
}
