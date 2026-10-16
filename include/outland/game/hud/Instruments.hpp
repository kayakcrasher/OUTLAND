#pragma once
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

// Instruments on the HUD: the compass strip at the top of the screen (always shown) and the
// speedometer at the wheel. The compass carries marks for the places that matter - the job or
// orders, the waypoint set on the map, hideouts, stations, the hospital, shops, the safe circle -
// each a small icon at its bearing; the ones that lead the way stay on the strip even behind you
// (pinned to its end, pointing round) and show how far. Verda's north is -z (Espera lies north of the capital), east is +x.
namespace outland::game::hud {

// Compass bearing (degrees, 0 = north, 90 = east, 0..360) of a horizontal direction.
float bearing(Vector3 direction);
// "N", "NE", "E", ... for the nearest of the eight points.
const char* point_name(float bearing_degrees);
// A strip of the horizon centred on `bearing_degrees`, `width` pixels wide, its top at `top`.
void draw_compass(float bearing_degrees, int screen_width, int top, int width = 420);

enum class Glyph : std::uint8_t { Objective, Waypoint, Home, Hideout, Police, Hospital, Shop, Bus, Ranch, Dock, Zone, Camp };
struct CompassMark {
    Vector3 at{};
    Glyph glyph{Glyph::Objective};
    Color colour{WHITE};
    std::string label;     // "HOSPITAL", "KILL RIVALS"
    bool lead{false};      // the way to go: always on the strip, with its distance
};
// Degrees from `heading` round to `bearing`, -180..180 (negative: to the left).
float relative_bearing(float heading, float bearing);
// Where a mark sits on a strip `width` wide centred on `heading` (pixels from the centre), and
// whether it is pinned to an end because it lies outside the strip's arc.
struct StripSpot { float x{0}; bool pinned{false}; bool visible{false}; };
StripSpot strip_spot(float heading, float bearing, int width, bool lead);
// An icon for a place, `size` its radius in pixels.
void draw_glyph(Glyph glyph, Vector2 centre, float size, Color colour);
// The compass with its marks, as seen from `from`.
void draw_compass(float bearing_degrees, int screen_width, int top, Vector3 from, const std::vector<CompassMark>& marks, int width = 420);

struct Speedometer {
    float speed{0};       // m/s, negative in reverse
    float top_speed{30};  // m/s, sets the dial's range
    float fuel{1};        // 0..1
    bool boosting{false}, handbrake{false};
};
// A round dial with a needle, the km/h in figures, the gear (D/R/N) and a fuel bar.
void draw_speedometer(const Speedometer& gauge, Vector2 centre, float radius);
// The dial's top mark in km/h: the top speed rounded up to the next 20.
int dial_limit(float top_speed);

} // namespace outland::game::hud
