#include "outland/game/MapScreen.hpp"
#include "outland/input/PointerEvents.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace outland::game {
namespace {
constexpr int map_pixels = 320;
const Color panel{14, 20, 16, 235}, ink{236, 240, 226, 255}, dim{168, 178, 160, 255}, accent{185, 202, 139, 255};
float scale_of(int width, int height) { return std::clamp(std::min(width / 1280.0F, height / 720.0F), .5F, 1.6F); }
void text(const Font* font, const char* value, Vector2 at, float size, Color color) {
    if (font && font->texture.id) DrawTextEx(*font, value, at, size, 1, color);
    else DrawText(value, static_cast<int>(at.x), static_cast<int>(at.y), static_cast<int>(size), color);
}
float measure(const Font* font, const char* value, float size) {
    return font && font->texture.id ? MeasureTextEx(*font, value, size, 1).x : static_cast<float>(MeasureText(value, static_cast<int>(size)));
}
void centred(const Font* font, const char* value, Vector2 at, float size, Color color) {
    text(font, value, {at.x - measure(font, value, size) / 2, at.y - size / 2}, size, color);
}
void button(Rectangle r, const char* label, bool active, const Font* font, float s) {
    DrawRectangleRounded(r, .25F, 6, active ? accent : Color{46, 58, 48, 255});
    centred(font, label, {r.x + r.width / 2, r.y + r.height / 2}, 17 * s, active ? Color{20, 28, 20, 255} : ink);
}
}

// ---------------------------------------------------------------- layout
Rectangle MapScreen::map_button(int width, int) {
    return {static_cast<float>(width - 174 - 104), 44, 96, 34};
}
Rectangle MapScreen::map_area(int width, int height) {
    const float s = scale_of(width, height), margin = 16 * s;
    const float side = std::min(height - 2 * margin, width - 330 * s - 3 * margin);
    return {margin, (height - side) / 2, side, side};
}
Rectangle MapScreen::settings_button(int index, int width, int height) {
    // 0 sensitivity -, 1 sensitivity +, 2 volume -, 3 volume +, 4 mute, 5 auto, 6 fast, 7 sharp
    const float s = scale_of(width, height);
    const auto area = map_area(width, height);
    const float x = area.x + area.width + 16 * s, w = width - x - 16 * s;
    const float row[]{172, 172, 247, 247, 292, 377, 377, 377};
    const float y = area.y + row[index] * s, h = 38 * s;
    if (index <= 3) return {index % 2 == 0 ? x : x + w - 56 * s, y, 56 * s, h};
    if (index == 4) return {x, y, w, h};
    const float third = (w - 16 * s) / 3;
    return {x + (index - 5) * (third + 8 * s), y, third, h};
}
Rectangle MapScreen::close_button(int width, int height) {
    const float s = scale_of(width, height);
    return {width - 16 * s - 150 * s, height - 16 * s - 50 * s, 150 * s, 50 * s};
}
Vector2 MapScreen::to_map(Rectangle area, float x, float z) {
    return {area.x + (x + extent) / (2 * extent) * area.width, area.y + (z + extent) / (2 * extent) * area.height};
}

// ---------------------------------------------------------------- input
std::vector<Vector2> MapScreen::clicks(bool focused) {
    std::vector<Vector2> out;
    std::unordered_set<int> current;
    const auto buffered = input::PointerEvents::presses();
    if (focused && !warm_) for (const auto& p : buffered) out.push_back(p);
    const int count = std::max(0, GetTouchPointCount());
    for (int i = 0; i < count; ++i) {
        const int id = GetTouchPointId(i);
        if (id < 0 || !current.insert(id).second) continue;
        if (focused && !warm_ && buffered.empty() && !previous_.contains(id)) out.push_back(GetTouchPosition(i));
    }
    if (focused && !warm_ && buffered.empty() && count == 0 && previous_.empty() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        out.push_back(GetMousePosition());
    previous_ = std::move(current);
    warm_ = false;
    return out;
}
bool MapScreen::tapped_map_button(int width, int height, bool focused) {
    bool hit = false;
    for (const auto& p : clicks(focused)) hit |= CheckCollisionPointRec(p, map_button(width, height));
    return hit;
}
MapScreen::Change MapScreen::update(GameSettings& settings, int width, int height, bool focused) {
    if (!open_) return Change::None;
    const bool fresh = warm_;
    Change change = Change::None;
    const auto set = [&](auto& field, auto value) { if (field != value) { field = value; change = Change::Settings; } };
    for (const auto& p : clicks(focused)) {
        if (CheckCollisionPointRec(p, close_button(width, height))) change = Change::Close;
        for (int i = 0; i < 8 && change != Change::Close; ++i) {
            if (!CheckCollisionPointRec(p, settings_button(i, width, height))) continue;
            switch (i) {
                case 0: case 1: {
                    // Whichever look the player uses: the mouse on a PC, swipes on a touch screen.
                    float& field = settings.mouse_look ? settings.mouse_sensitivity : settings.look_sensitivity;
                    const float lo = settings.mouse_look ? GameSettings::min_mouse : GameSettings::min_sensitivity;
                    const float hi = settings.mouse_look ? GameSettings::max_mouse : GameSettings::max_sensitivity;
                    set(field, std::clamp(std::round((field + (i == 0 ? -.1F : .1F)) * 10) / 10, lo, hi));
                    break;
                }
                case 2: set(settings.volume, std::max(0.0F, std::round((settings.volume - .1F) * 10) / 10)); break;
                case 3: set(settings.volume, std::min(1.0F, std::round((settings.volume + .1F) * 10) / 10)); break;
                case 4: set(settings.muted, !settings.muted); break;
                default: set(settings.graphics, static_cast<GameSettings::Graphics>(i - 5)); break;
            }
        }
    }
    if (focused && !fresh && (IsKeyPressed(KEY_M) || IsKeyPressed(KEY_ESCAPE))) change = Change::Close;
    if (change == Change::Close) close();
    return change;
}

// ---------------------------------------------------------------- drawing
void MapScreen::bake(const world::VerdaRegion& region) {
    // Height and zone per pixel: sea, then land tinted by its zone and shaded by slope, with the
    // zone borders inked in.
    std::vector<float> height(map_pixels * map_pixels);
    std::vector<int> zone(map_pixels * map_pixels);
    const auto world = [](int i) { return -extent + (static_cast<float>(i) + .5F) / map_pixels * 2 * extent; };
    for (int j = 0; j < map_pixels; ++j) for (int i = 0; i < map_pixels; ++i) {
        height[j * map_pixels + i] = world::terrain::TerrainHeight::sample(world(i), world(j));
        zone[j * map_pixels + i] = static_cast<int>(world::zones::zone_at(world(i), world(j)));
    }
    Image image = GenImageColor(map_pixels, map_pixels, BLANK);
    std::array<Vector2, world::zones::zone_count> sum{};
    std::array<int, world::zones::zone_count> count{};
    const float cell = 2 * extent / map_pixels;
    for (int j = 0; j < map_pixels; ++j) for (int i = 0; i < map_pixels; ++i) {
        const int k = j * map_pixels + i;
        const float h = height[k];
        Color c;
        if (h <= world::layout::sea_level) {
            const float depth = std::clamp((world::layout::sea_level - h) / 40.0F, 0.0F, 1.0F);
            c = ColorLerp(Color{70, 150, 176, 255}, Color{24, 62, 96, 255}, depth);
        } else {
            const int z = zone[k];
            sum[z] = Vector2Add(sum[z], {world(i), world(j)});++count[z];
            const float east = height[j * map_pixels + std::min(i + 1, map_pixels - 1)] - h;
            const float south = height[std::min(j + 1, map_pixels - 1) * map_pixels + i] - h;
            const float light = std::clamp(1.0F + (-east - south) / cell * .9F, .62F, 1.25F);
            const float beach = std::clamp(1 - (h - world::layout::sea_level) / 4.0F, 0.0F, 1.0F);
            auto base = ColorLerp(world::zones::zones[static_cast<std::size_t>(z)].color, Color{214, 200, 150, 255}, beach * .8F);
            c = {static_cast<unsigned char>(std::clamp(base.r * light, 0.0F, 255.0F)),
                 static_cast<unsigned char>(std::clamp(base.g * light, 0.0F, 255.0F)),
                 static_cast<unsigned char>(std::clamp(base.b * light, 0.0F, 255.0F)), 255};
            const bool border = (i + 1 < map_pixels && zone[k + 1] != z && height[k + 1] > world::layout::sea_level) ||
                                (j + 1 < map_pixels && zone[k + map_pixels] != z && height[k + map_pixels] > world::layout::sea_level);
            if (border) c = Color{38, 30, 24, 255};
        }
        ImageDrawPixel(&image, i, j, c);
    }
    // Zone names go where there is room: deep inside the zone (far from its border and the sea),
    // and clear of the towns, whose names are drawn too.
    std::vector<int> inland(map_pixels * map_pixels, 0);
    std::vector<int> frontier;
    for (int k = 0; k < map_pixels * map_pixels; ++k) {
        const int i = k % map_pixels, j = k / map_pixels;
        const bool land = height[k] > world::layout::sea_level;
        bool edge = !land || i == 0 || j == 0 || i == map_pixels - 1 || j == map_pixels - 1;
        if (!edge) for (const int n : {k - 1, k + 1, k - map_pixels, k + map_pixels})
            if (zone[n] != zone[k] || height[n] <= world::layout::sea_level) edge = true;
        if (edge) frontier.push_back(k); else inland[k] = -1;
    }
    for (std::size_t f = 0; f < frontier.size(); ++f) {
        const int k = frontier[f], i = k % map_pixels, j = k / map_pixels;
        for (const auto [di, dj] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
            const int ni = i + di, nj = j + dj;
            if (ni < 0 || nj < 0 || ni >= map_pixels || nj >= map_pixels) continue;
            const int n = nj * map_pixels + ni;
            if (inland[n] == -1) {inland[n] = inland[k] + 1; frontier.push_back(n);}
        }
    }
    zone_labels_.assign(world::zones::zone_count, {});
    std::vector<float> best(world::zones::zone_count, -1);
    for (int k = 0; k < map_pixels * map_pixels; ++k) {
        if (height[k] <= world::layout::sea_level) continue;
        const float x = world(k % map_pixels), z = world(k / map_pixels);
        float clear = static_cast<float>(inland[k]) * cell;
        for (const auto& town : region.settlements())
            clear = std::min(clear, std::hypot((x - town.center.x) * .6F, z - town.center.z) - 120);
        const int zz = zone[k];
        // County names keep clear of the capital district, whose own name sits inside the city.
        if (zz != static_cast<int>(world::zones::ZoneId::Verdkerno) && std::hypot(x, z) < 850) continue;
        if (clear > best[zz]) {best[zz] = clear; zone_labels_[zz] = {x, z};}
    }
    // The capital district is small: its name goes inside it, under Verda's own dot.
    zone_labels_[static_cast<std::size_t>(world::zones::ZoneId::Verdkerno)] = {0, world::zones::capital_radius * .8F};
    best[static_cast<std::size_t>(world::zones::ZoneId::Verdkerno)] = 1;
    for (std::size_t z = 0; z < sum.size(); ++z)
        if (best[z] < 0 && count[z]) zone_labels_[z] = Vector2Scale(sum[z], 1.0F / static_cast<float>(count[z]));
    texture_ = LoadTextureFromImage(image);
    SetTextureFilter(texture_, TEXTURE_FILTER_BILINEAR);
    UnloadImage(image);
    baked_ = true;
}
void MapScreen::unload() {
    if (baked_) UnloadTexture(texture_);
    baked_ = false;
}
void MapScreen::draw(const world::VerdaRegion& region, const View& view, const GameSettings& settings, int width, int height) {
    if (!open_) return;
    if (!baked_) bake(region);
    const float s = scale_of(width, height);
    const Font* font = view.font;
    DrawRectangle(0, 0, width, height, Color{8, 12, 10, 236});
    const auto area = map_area(width, height);
    DrawTexturePro(texture_, {0, 0, static_cast<float>(texture_.width), static_cast<float>(texture_.height)}, area, {0, 0}, 0, WHITE);
    DrawRectangleLinesEx(area, 2, Color{90, 104, 86, 255});
    // Grid every kilometre.
    for (float g = -2000; g <= 2000; g += 1000) {
        const auto a = to_map(area, g, -extent), b = to_map(area, g, extent);
        DrawLineEx(a, b, 1, Color{255, 255, 255, 28});
        const auto c = to_map(area, -extent, g), d = to_map(area, extent, g);
        DrawLineEx(c, d, 1, Color{255, 255, 255, 28});
    }
    // Zone names over their land.
    for (std::size_t z = 0; z < zone_labels_.size(); ++z) {
        const auto& info = world::zones::zones[z];
        auto at = to_map(area, zone_labels_[z].x, zone_labels_[z].y);
        const std::string name = "ZONO " + std::string(info.name);
        const float half = measure(font, name.c_str(), 22 * s) / 2 + 6 * s;
        at.x = std::clamp(at.x, area.x + half, area.x + area.width - half);
        centred(font, name.c_str(), {at.x, at.y - 30 * s}, 22 * s, Color{255, 255, 255, 150});
        centred(font, std::string(info.meaning).c_str(), {at.x, at.y - 10 * s}, 14 * s, Color{255, 255, 255, 120});
    }
    // Roads, then towns.
    for (const auto& town : region.settlements()) for (const auto& road : town.roads) {
        const Color c = road.type == world::RoadType::Asphalt ? Color{58, 58, 56, 230} :
                        road.type == world::RoadType::Gravel ? Color{146, 128, 98, 230} : Color{120, 96, 66, 220};
        DrawLineEx(to_map(area, road.start.x, road.start.z), to_map(area, road.end.x, road.end.z),
                   std::max(1.5F, road.width / (2 * extent) * area.width * 2.5F), c);
    }
    for (const auto& town : region.settlements()) {
        const auto at = to_map(area, town.center.x, town.center.z);
        DrawCircleV(at, 6 * s, Color{250, 248, 236, 255});
        DrawCircleLinesV(at, 6 * s, Color{30, 30, 30, 255});
        // Names read to the right of their dot unless that would run off the map.
        const float wide = measure(font, town.name.c_str(), 18 * s);
        const float label_x = at.x + 9 * s + wide > area.x + area.width - 4 ? at.x - 9 * s - wide : at.x + 9 * s;
        text(font, town.name.c_str(), {label_x + 1, at.y - 8 * s}, 18 * s, Color{0, 0, 0, 170});
        text(font, town.name.c_str(), {label_x, at.y - 9 * s}, 18 * s, ink);
    }
    // The bus line: stops (H) and the buses on their way.
    for (const auto& stop : view.stops) {
        const auto at = to_map(area, stop.x, stop.z);
        DrawRectangleRounded({at.x - 6 * s, at.y - 6 * s, 12 * s, 12 * s}, .3F, 4, Color{250, 205, 40, 255});
        centred(font, "H", {at.x, at.y}, 11 * s, Color{25, 25, 25, 255});
    }
    for (const auto& [where, heading] : view.buses) {
        const auto at = to_map(area, where.x, where.z);
        const Vector2 f{std::sin(heading * DEG2RAD), std::cos(heading * DEG2RAD)}, r{-f.y, f.x};
        const auto corner = [&](float a, float b) { return Vector2Add(at, Vector2Add(Vector2Scale(f, a * s), Vector2Scale(r, b * s))); };
        DrawTriangle(corner(9, -4), corner(-9, 4), corner(9, 4), Color{0, 140, 60, 255});
        DrawTriangle(corner(9, -4), corner(-9, -4), corner(-9, 4), Color{0, 140, 60, 255});
        DrawTriangle(corner(9, 4), corner(-9, 4), corner(9, -4), Color{0, 140, 60, 255});
        DrawTriangle(corner(-9, 4), corner(-9, -4), corner(9, -4), Color{0, 140, 60, 255});
        DrawCircleV(corner(9, 0), 2.2F * s, Color{255, 255, 255, 255});
    }
    // La Mafio: each crew's hideout, a diamond in its gang's colour.
    for (const auto& [where, color] : view.turf) {
        const auto at = to_map(area, where.x, where.z);
        const float r = 7 * s;
        DrawTriangle({at.x, at.y - r}, {at.x - r, at.y}, {at.x + r, at.y}, BLACK);
        DrawTriangle({at.x - r, at.y}, {at.x, at.y + r}, {at.x + r, at.y}, BLACK);
        DrawTriangle({at.x, at.y - r + 2}, {at.x - r + 2, at.y}, {at.x + r - 2, at.y}, color);
        DrawTriangle({at.x - r + 2, at.y}, {at.x, at.y + r - 2}, {at.x + r - 2, at.y}, color);
    }
    // Battle Royale: the safe circle.
    if (view.circle_radius > 0) {
        const auto centre = to_map(area, 0, 0);
        DrawRing(centre, view.circle_radius / (2 * extent) * area.width - 1.5F, view.circle_radius / (2 * extent) * area.width + 1.5F, 0, 360, 96, Color{120, 190, 255, 230});
    }
    // You: an arrow pointing the way you face, ringed so it reads on any ground.
    const auto you = to_map(area, view.player.x, view.player.z);
    const Vector2 forward{std::sin(view.yaw), std::cos(view.yaw)}, side{-forward.y, forward.x};
    const float pulse = 10 * s + 3 * s * std::sin(static_cast<float>(GetTime()) * 4);
    DrawCircleLinesV(you, pulse + 6 * s, Color{255, 230, 90, 200});
    const auto tip = Vector2Add(you, Vector2Scale(forward, 14 * s));
    const auto left = Vector2Add(Vector2Subtract(you, Vector2Scale(forward, 8 * s)), Vector2Scale(side, 9 * s));
    const auto right = Vector2Subtract(Vector2Subtract(you, Vector2Scale(forward, 8 * s)), Vector2Scale(side, 9 * s));
    DrawTriangle(tip, right, left, Color{255, 220, 60, 255});
    DrawTriangle(tip, left, right, Color{255, 220, 60, 255});
    DrawTriangleLines(tip, left, right, Color{40, 30, 0, 255});
    // North arrow and a 500 m scale bar.
    const Vector2 north{area.x + area.width - 30 * s, area.y + 34 * s};
    DrawTriangle({north.x, north.y - 16 * s}, {north.x - 8 * s, north.y + 4 * s}, {north.x + 8 * s, north.y + 4 * s}, ink);
    centred(font, "N", {north.x, north.y + 16 * s}, 16 * s, ink);
    const float bar = 500 / (2 * extent) * area.width;
    const Vector2 bar_at{area.x + 16 * s, area.y + area.height - 22 * s};
    DrawRectangleV(bar_at, {bar, 4 * s}, ink);
    text(font, "500 m", {bar_at.x, bar_at.y - 20 * s}, 15 * s, ink);

    // Right panel: where you are, then settings.
    const float x = area.x + area.width + 16 * s, w = width - x - 16 * s;
    DrawRectangleRounded({x - 8 * s, area.y, w + 16 * s, area.height}, .03F, 6, panel);
    const auto& here = world::zones::zone(world::zones::zone_at(view.player));
    text(font, "MAPO DE VERDA", {x, area.y + 12 * s}, 24 * s, accent);
    const std::string zone_line = "ZONO " + std::string(here.name);
    text(font, zone_line.c_str(), {x, area.y + 46 * s}, 22 * s, ink);
    text(font, std::string(here.meaning).c_str(), {x, area.y + 72 * s}, 15 * s, dim);
    const char* where = TextFormat("%.0f m %s  %.0f m %s", std::abs(view.player.x), view.player.x < 0 ? "W" : "E",
                                   std::abs(view.player.z), view.player.z < 0 ? "N" : "S");
    text(font, where, {x, area.y + 94 * s}, 14 * s, dim);
    text(font, "AGORDOJ  SETTINGS", {x, area.y + 122 * s}, 17 * s, accent);
    const auto row_label = [&](int index, const char* label, const char* value) {
        const auto r = settings_button(index, width, height);
        text(font, label, {x, r.y - 22 * s}, 15 * s, dim);
        centred(font, value, {x + w / 2, r.y + r.height / 2}, 20 * s, ink);
    };
    button(settings_button(0, width, height), "-", false, font, s);
    button(settings_button(1, width, height), "+", false, font, s);
    row_label(0, settings.mouse_look ? "Mouse sensitivity" : "Touch look sensitivity",
              TextFormat("%.1f", settings.mouse_look ? settings.mouse_sensitivity : settings.look_sensitivity));
    button(settings_button(2, width, height), "-", false, font, s);
    button(settings_button(3, width, height), "+", false, font, s);
    row_label(2, "Volume", TextFormat("%d%%", static_cast<int>(std::round(settings.volume * 100))));
    button(settings_button(4, width, height), settings.muted ? "SOUND OFF" : "SOUND ON", !settings.muted, font, s);
    text(font, "Graphics", {x, settings_button(5, width, height).y - 22 * s}, 15 * s, dim);
    const char* modes[]{"AUTO", "FAST", "SHARP"};
    for (int i = 0; i < 3; ++i) button(settings_button(5 + i, width, height), modes[i], static_cast<int>(settings.graphics) == i, font, s);
    text(font, "AUTO adapts resolution to keep play smooth.", {x, settings_button(5, width, height).y + 46 * s}, 13 * s, dim);
    // Legend.
    float y = settings_button(5, width, height).y + 76 * s;
    for (const auto& zone : world::zones::zones) {
        DrawRectangleRec({x, y, 14 * s, 14 * s}, zone.color);
        text(font, TextFormat("%s - %s", std::string(zone.name).c_str(), std::string(zone.meaning).c_str()), {x + 22 * s, y - 1 * s}, 14 * s, ink);
        y += 20 * s;
    }
    if (!view.stops.empty()) {
        DrawRectangleRounded({x, y, 14 * s, 14 * s}, .3F, 4, Color{250, 205, 40, 255});
        centred(font, "H", {x + 7 * s, y + 7 * s}, 10 * s, Color{25, 25, 25, 255});
        text(font, "Bus stop  ·  green: a bus on Linio 1", {x + 22 * s, y - 1 * s}, 14 * s, ink);
    }
    if (!view.turf.empty()) {
        y += 20 * s;
        DrawTriangle({x + 7 * s, y}, {x, y + 7 * s}, {x + 14 * s, y + 7 * s}, Color{46, 204, 96, 255});
        DrawTriangle({x, y + 7 * s}, {x + 7 * s, y + 14 * s}, {x + 14 * s, y + 7 * s}, Color{46, 204, 96, 255});
        text(font, "Krimuloj crew (gang colour) - the police are paid off", {x + 22 * s, y - 1 * s}, 14 * s, ink);
    }
    button(close_button(width, height), "CLOSE  (M)", true, font, s);
}

}
