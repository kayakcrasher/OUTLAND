#pragma once
#include <raylib.h>
#include <string>

// Instruments on the HUD: the compass strip at the top of the screen (always shown) and the
// speedometer at the wheel. Verda's north is -z (Espera lies north of the capital), east is +x.
namespace outland::game::hud {

// Compass bearing (degrees, 0 = north, 90 = east, 0..360) of a horizontal direction.
float bearing(Vector3 direction);
// "N", "NE", "E", ... for the nearest of the eight points.
const char* point_name(float bearing_degrees);
// A strip of the horizon centred on `bearing_degrees`, `width` pixels wide, its top at `top`.
void draw_compass(float bearing_degrees, int screen_width, int top, int width = 420);

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
