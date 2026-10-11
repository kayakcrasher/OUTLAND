#include "outland/game/hud/Instruments.hpp"
#include <algorithm>
#include <cmath>

namespace outland::game::hud {

float bearing(Vector3 direction) {
    if (std::abs(direction.x) < 1e-6F && std::abs(direction.z) < 1e-6F) return 0;
    float degrees = std::atan2(direction.x, -direction.z) * RAD2DEG;
    if (degrees < 0) degrees += 360;
    return degrees >= 360 ? degrees - 360 : degrees;
}

const char* point_name(float bearing_degrees) {
    static const char* names[]{"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    const int index = static_cast<int>(std::floor(std::fmod(bearing_degrees + 22.5F + 720, 360.0F) / 45));
    return names[index % 8];
}

void draw_compass(float heading, int screen_width, int top, int width) {
    const float centre = screen_width * .5F, half = width * .5F;
    constexpr float span = 120; // degrees shown across the strip
    const float pixels = width / span;
    const int height = 26;
    DrawRectangle(static_cast<int>(centre - half), top, width, height, Fade(BLACK, .45F));
    BeginScissorMode(static_cast<int>(centre - half), top, width, height);
    const int first = static_cast<int>(std::floor((heading - span / 2) / 5)) * 5;
    for (int mark = first; mark <= heading + span / 2 + 5; mark += 5) {
        const float x = centre + (static_cast<float>(mark) - heading) * pixels;
        const int at = ((mark % 360) + 360) % 360;
        // Fade toward the ends so the strip reads as a curve.
        const float edge = 1 - std::min(1.0F, std::abs(x - centre) / half);
        const unsigned char alpha = static_cast<unsigned char>(90 + 165 * edge);
        if (at % 45 == 0) {
            const char* name = point_name(static_cast<float>(at));
            const bool cardinal = at % 90 == 0;
            const int size = cardinal ? 18 : 14;
            const Color colour = at == 0 ? Color{235, 70, 55, alpha} : Color{255, 255, 255, alpha};
            DrawText(name, static_cast<int>(x - MeasureText(name, size) / 2.0F), top + (cardinal ? 4 : 6), size, colour);
        } else if (at % 15 == 0) {
            const char* number = TextFormat("%d", at);
            DrawText(number, static_cast<int>(x - MeasureText(number, 10) / 2.0F), top + 9, 10, Color{220, 220, 220, alpha});
        } else {
            DrawLine(static_cast<int>(x), top + height - 7, static_cast<int>(x), top + height - 2, Color{255, 255, 255, static_cast<unsigned char>(alpha / 2)});
        }
    }
    EndScissorMode();
    // The heading: a caret under the strip and the bearing in figures.
    DrawTriangle({centre, top + height - 4.0F}, {centre - 6, top + height + 4.0F}, {centre + 6, top + height + 4.0F}, Color{255, 214, 90, 255});
    const char* figures = TextFormat("%03d", static_cast<int>(std::lround(heading)) % 360);
    DrawText(figures, static_cast<int>(centre - MeasureText(figures, 14) / 2.0F), top + height + 6, 14, Color{255, 214, 90, 255});
}

int dial_limit(float top_speed) {
    const int kmh = static_cast<int>(std::ceil(std::max(1.0F, top_speed) * 3.6F * 1.1F));
    return std::max(60, (kmh + 19) / 20 * 20);
}

void draw_speedometer(const Speedometer& gauge, Vector2 c, float r) {
    const int limit = dial_limit(gauge.top_speed);
    const float kmh = std::abs(gauge.speed) * 3.6F;
    // A 240-degree dial from bottom-left round to bottom-right (raylib angles: 0 = right, clockwise).
    constexpr float start = 150, sweep = 240;
    const auto angle_of = [&](float value) { return (start + sweep * std::clamp(value / limit, 0.0F, 1.0F)) * DEG2RAD; };
    DrawCircleV(c, r, Fade(BLACK, .55F));
    DrawRing(c, r - 4, r, start, start + sweep, 48, Fade(RAYWHITE, .5F));
    // The red band in the last eighth.
    DrawRing(c, r - 9, r - 4, start + sweep * .875F, start + sweep, 16, Color{220, 60, 50, 200});
    for (int mark = 0; mark <= limit; mark += 10) {
        const float a = angle_of(static_cast<float>(mark));
        const bool major = mark % 20 == 0;
        const float inner = r - (major ? 14 : 9);
        DrawLineEx({c.x + std::cos(a) * inner, c.y + std::sin(a) * inner}, {c.x + std::cos(a) * (r - 4), c.y + std::sin(a) * (r - 4)}, major ? 2.0F : 1.0F, RAYWHITE);
        if (major && (limit <= 140 || mark % 40 == 0)) {
            const char* label = TextFormat("%d", mark);
            const float lr = r - 26;
            DrawText(label, static_cast<int>(c.x + std::cos(a) * lr - MeasureText(label, 10) / 2.0F), static_cast<int>(c.y + std::sin(a) * lr - 5), 10, Fade(RAYWHITE, .8F));
        }
    }
    const float a = angle_of(kmh);
    DrawLineEx(c, {c.x + std::cos(a) * (r - 10), c.y + std::sin(a) * (r - 10)}, 3, Color{240, 70, 50, 255});
    DrawCircleV(c, 5, Color{240, 70, 50, 255});
    const char* figures = TextFormat("%d", static_cast<int>(std::lround(kmh)));
    DrawText(figures, static_cast<int>(c.x - MeasureText(figures, 26) / 2.0F), static_cast<int>(c.y + r * .22F), 26, RAYWHITE);
    DrawText("km/h", static_cast<int>(c.x - MeasureText("km/h", 10) / 2.0F), static_cast<int>(c.y + r * .22F + 26), 10, Fade(RAYWHITE, .7F));
    const char* gear = gauge.handbrake ? "P" : gauge.speed < -.3F ? "R" : std::abs(gauge.speed) < .3F ? "N" : "D";
    DrawText(gear, static_cast<int>(c.x - MeasureText(gear, 16) / 2.0F), static_cast<int>(c.y - r * .5F), 16,
             gear[0] == 'R' ? Color{255, 180, 80, 255} : Color{140, 230, 150, 255});
    if (gauge.boosting) DrawText("BOOST", static_cast<int>(c.x - MeasureText("BOOST", 12) / 2.0F), static_cast<int>(c.y - r * .5F - 16), 12, Color{90, 200, 255, 255});
    // Fuel: a short bar under the dial.
    const float w = r * 1.2F;
    DrawRectangle(static_cast<int>(c.x - w / 2), static_cast<int>(c.y + r + 6), static_cast<int>(w), 6, Fade(BLACK, .6F));
    const float fuel = std::clamp(gauge.fuel, 0.0F, 1.0F);
    DrawRectangle(static_cast<int>(c.x - w / 2), static_cast<int>(c.y + r + 6), static_cast<int>(w * fuel), 6,
                  fuel < .2F ? Color{230, 80, 60, 255} : Color{230, 200, 80, 255});
    DrawText("FUEL", static_cast<int>(c.x - w / 2 - 30), static_cast<int>(c.y + r + 4), 10, Fade(RAYWHITE, .7F));
}

} // namespace outland::game::hud
