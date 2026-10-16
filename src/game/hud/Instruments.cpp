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

float relative_bearing(float heading, float bearing) {
    float d = std::fmod(bearing - heading + 540.0F, 360.0F);
    if (d < 0) d += 360;
    return d - 180;
}

StripSpot strip_spot(float heading, float bearing, int width, bool lead) {
    constexpr float span = 120;
    const float half = width * .5F, edge = half - 10;
    const float x = relative_bearing(heading, bearing) * (width / span);
    if (std::abs(x) <= edge) return {x, false, true};
    if (!lead) return {x, false, false};
    return {x < 0 ? -edge : edge, true, true};
}

void draw_glyph(Glyph glyph, Vector2 c, float r, Color colour) {
    const Color dark{16, 16, 16, colour.a};
    const Color white{255, 255, 255, colour.a};
    switch (glyph) {
        case Glyph::Objective:
            DrawPoly(c, 4, r + 1.5F, 0, dark);
            DrawPoly(c, 4, r, 0, colour);
            DrawPoly(c, 4, r * .4F, 0, white);
            break;
        case Glyph::Waypoint:   // a map pin
            DrawCircleV({c.x, c.y - r * .35F}, r * .8F + 1.5F, dark);
            DrawTriangle({c.x - r * .7F, c.y - r * .1F}, {c.x, c.y + r * 1.05F}, {c.x + r * .7F, c.y - r * .1F}, colour);
            DrawCircleV({c.x, c.y - r * .35F}, r * .8F, colour);
            DrawCircleV({c.x, c.y - r * .35F}, r * .3F, dark);
            break;
        case Glyph::Home: case Glyph::Ranch: {
            const Rectangle body{c.x - r * .75F, c.y - r * .1F, r * 1.5F, r * .95F};
            DrawRectangle(static_cast<int>(body.x - 1), static_cast<int>(body.y - 1), static_cast<int>(body.width + 2), static_cast<int>(body.height + 2), dark);
            DrawTriangle({c.x - r - 1, c.y - r * .05F}, {c.x + r + 1, c.y - r * .05F}, {c.x, c.y - r - 1.5F}, dark);
            DrawTriangle({c.x - r, c.y - r * .1F}, {c.x + r, c.y - r * .1F}, {c.x, c.y - r}, colour);
            DrawRectangleRec(body, colour);
            if (glyph == Glyph::Ranch) {   // a barn door's cross
                DrawLineEx({body.x + 1, body.y + 1}, {body.x + body.width - 1, body.y + body.height - 1}, 1.2F, white);
                DrawLineEx({body.x + body.width - 1, body.y + 1}, {body.x + 1, body.y + body.height - 1}, 1.2F, white);
            } else DrawRectangleRec({c.x - r * .2F, c.y + r * .3F, r * .4F, r * .55F}, dark);
            break;
        }
        case Glyph::Hideout:
            DrawPoly(c, 6, r + 1.5F, 0, dark);
            DrawPoly(c, 6, r, 0, colour);
            DrawCircleV(c, r * .35F, dark);
            break;
        case Glyph::Police: {   // a shield with a gold star at its heart
            DrawRectangleRec({c.x - r * .8F - 1, c.y - r - 1, r * 1.6F + 2, r + 2}, dark);
            DrawTriangle({c.x - r * .8F - 1, c.y - .5F}, {c.x, c.y + r + 1.5F}, {c.x + r * .8F + 1, c.y - .5F}, dark);
            DrawRectangleRec({c.x - r * .8F, c.y - r, r * 1.6F, r}, colour);
            DrawTriangle({c.x - r * .8F, c.y}, {c.x, c.y + r}, {c.x + r * .8F, c.y}, colour);
            DrawPoly({c.x, c.y - r * .15F}, 5, r * .38F, -90, Color{255, 214, 90, colour.a});
            break;
        }
        case Glyph::Hospital:
            DrawCircleV(c, r + 1.5F, dark);
            DrawCircleV(c, r, white);
            DrawRectangleRec({c.x - r * .62F, c.y - r * .2F, r * 1.24F, r * .4F}, colour);
            DrawRectangleRec({c.x - r * .2F, c.y - r * .62F, r * .4F, r * 1.24F}, colour);
            break;
        case Glyph::Shop:   // a carrier bag
            DrawRing({c.x, c.y - r * .3F}, r * .3F, r * .48F, 180, 360, 10, dark);
            DrawRectangleRec({c.x - r * .75F - 1, c.y - r * .35F - 1, r * 1.5F + 2, r * 1.25F + 2}, dark);
            DrawRectangleRec({c.x - r * .75F, c.y - r * .35F, r * 1.5F, r * 1.25F}, colour);
            break;
        case Glyph::Bus:
            DrawRectangleRounded({c.x - r - 1, c.y - r - 1, 2 * r + 2, 2 * r + 2}, .3F, 4, dark);
            DrawRectangleRounded({c.x - r, c.y - r, 2 * r, 2 * r}, .3F, 4, colour);
            DrawText("H", static_cast<int>(c.x - MeasureText("H", static_cast<int>(r * 1.5F)) / 2.0F), static_cast<int>(c.y - r * .72F),
                     static_cast<int>(r * 1.5F), dark);
            break;
        case Glyph::Dock:   // an anchor
            DrawRing({c.x, c.y - r * .7F}, r * .16F, r * .36F, 0, 360, 12, colour);
            DrawRectangleRec({c.x - r * .13F, c.y - r * .4F, r * .26F, r * 1.3F}, colour);
            DrawRectangleRec({c.x - r * .5F, c.y - r * .3F, r, r * .22F}, colour);
            DrawRing({c.x, c.y + r * .25F}, r * .55F, r * .8F, 0, 180, 12, colour);
            break;
        case Glyph::Zone:
            DrawRing(c, r - 2.5F, r + 1.5F, 0, 360, 24, dark);
            DrawRing(c, r - 1.5F, r + .5F, 0, 360, 24, colour);
            DrawCircleV(c, r * .3F, colour);
            break;
        case Glyph::Camp:   // a target
            DrawCircleV(c, r + 1.5F, dark);
            DrawCircleV(c, r, colour);
            DrawCircleV(c, r * .66F, white);
            DrawCircleV(c, r * .33F, colour);
            break;
    }
}

void draw_compass(float heading, int screen_width, int top, Vector3 from, const std::vector<CompassMark>& marks, int width) {
    draw_compass(heading, screen_width, top, width);
    const float centre = screen_width * .5F;
    const int height = 26;
    const float row = top + height * .5F + 2;
    const CompassMark* named = nullptr;   // the place straight ahead, named under the strip
    float named_off = 6;
    // The others first, the leads on top.
    for (int pass = 0; pass < 2; ++pass) {
        for (const auto& mark : marks) {
            if (mark.lead != (pass == 1)) continue;
            const Vector3 to{mark.at.x - from.x, 0, mark.at.z - from.z};
            const float distance = std::hypot(to.x, to.z);
            const auto spot = strip_spot(heading, bearing(to), width, mark.lead);
            if (!spot.visible) continue;
            const float x = centre + spot.x;
            const float edge = 1 - std::min(1.0F, std::abs(spot.x) / (width * .5F));
            Color colour = mark.colour;
            if (!mark.lead) colour.a = static_cast<unsigned char>(static_cast<float>(colour.a) * (.55F + .45F * edge));
            const float size = mark.lead ? 8.5F : 7.5F;
            draw_glyph(mark.glyph, {x, row}, size, colour);
            if (spot.pinned) {   // behind you: an arrow pointing round to it
                const float s = spot.x < 0 ? -1.0F : 1.0F;
                const float tip = x + s * 17;
                DrawTriangle({tip, row}, {tip - s * 7, row + (s > 0 ? -6.0F : 6.0F)}, {tip - s * 7, row + (s > 0 ? 6.0F : -6.0F)}, mark.colour);
            }
            if (mark.lead) {
                const char* far = distance >= 1000 ? TextFormat("%.1f km", distance / 1000) : TextFormat("%.0f m", distance);
                const int w = MeasureText(far, 12);
                // Under the strip, clear of the heading figures when it is dead ahead.
                const float y = std::abs(spot.x) < 26 ? top + height + 22.0F : top + height + 5.0F;
                DrawText(far, static_cast<int>(x - w / 2.0F) + 1, static_cast<int>(y) + 1, 12, Color{0, 0, 0, 200});
                DrawText(far, static_cast<int>(x - w / 2.0F), static_cast<int>(y), 12, mark.colour);
            }
            const float off = std::abs(relative_bearing(heading, bearing(to)));
            if (!mark.label.empty() && off < named_off && !spot.pinned) { named = &mark; named_off = off; }
        }
    }
    if (named) {
        const float distance = std::hypot(named->at.x - from.x, named->at.z - from.z);
        const char* line = TextFormat("%s  %.0f m", named->label.c_str(), distance);
        const int w = MeasureText(line, 13);
        const float y = top + height + 38.0F;
        DrawRectangle(static_cast<int>(centre - w / 2.0F - 6), static_cast<int>(y - 2), w + 12, 17, Fade(BLACK, .5F));
        DrawText(line, static_cast<int>(centre - w / 2.0F), static_cast<int>(y), 13, Color{named->colour.r, named->colour.g, named->colour.b, 255});
    }
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
