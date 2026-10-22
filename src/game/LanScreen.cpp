#include "outland/game/LanScreen.hpp"
#include "outland/game/GameMode.hpp"
#include "outland/game/ai/CombatBot.hpp"
#include "outland/input/PointerEvents.hpp"
#include <algorithm>
#include <vector>

namespace outland::game {
namespace {
float scale(int w, int h) { return std::max(.1F, std::min(w / 900.0F, h / 720.0F)); }
Rectangle box(float x, float y, float w, float h, int width, int height) {
    const float s = scale(width, height);
    return {(width - 900 * s) / 2 + x * s, (height - 720 * s) / 2 + y * s, w * s, h * s};
}
void button(Rectangle r, const char* text, float s, bool lit = false) {
    const bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRounded(r, .15F, 8, lit ? Color{70, 120, 70, 240} : hovered ? Color{80, 100, 72, 240} : Color{38, 45, 38, 230});
    DrawRectangleRoundedLines(r, .15F, 8, WHITE);
    const int font = std::max(10, static_cast<int>(20 * s));
    DrawText(text, static_cast<int>(r.x + r.width / 2 - MeasureText(text, font) / 2), static_cast<int>(r.y + (r.height - font) / 2), font, RAYWHITE);
}
const char* mode_label(std::uint8_t mode) {
    switch (static_cast<GameMode>(mode)) {
    case GameMode::BattleRoyale: return "BATTLE ROYALE";
    case GameMode::ZombieSurvival: return "ZOMBIE SURVIVAL";
    case GameMode::Explore: return "EXPLORE VERDA";
    default: return "?";
    }
}
}

Rectangle LanScreen::host_button(int w, int h) { return box(240, 200, 420, 58, w, h); }
Rectangle LanScreen::game_button(int i, int w, int h) { return box(160, 330 + static_cast<float>(i) * 62, 580, 52, w, h); }
Rectangle LanScreen::back_button(int w, int h) { return box(300, 664, 300, 46, w, h); }
Rectangle LanScreen::mode_button(int w, int h) { return box(200, 420, 500, 50, w, h); }
Rectangle LanScreen::sides_button(int w, int h) { return box(200, 478, 500, 50, w, h); }
Rectangle LanScreen::level_button(int w, int h) { return box(200, 536, 500, 50, w, h); }
Rectangle LanScreen::start_button(int w, int h) { return box(200, 598, 500, 54, w, h); }

LanScreen::Action LanScreen::update(int w, int h, const net::LanSession& lan) {
    std::vector<Vector2> points;
    const auto buffered = input::PointerEvents::presses();
    points.assign(buffered.begin(), buffered.end());
    std::unordered_set<int> current;
    const int count = std::max(0, GetTouchPointCount());
    for (int i = 0; i < count; ++i) {
        const int id = GetTouchPointId(i);
        if (id >= 0 && current.insert(id).second && buffered.empty() && !previous_.contains(id)) points.push_back(GetTouchPosition(i));
    }
    if (buffered.empty() && count == 0 && previous_.empty() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) points.push_back(GetMousePosition());
    previous_ = std::move(current);
    if (IsKeyPressed(KEY_ESCAPE)) return {lan.active() ? Kind::Leave : Kind::Back, -1};
    for (const auto p : points) {
        if (CheckCollisionPointRec(p, back_button(w, h))) return {lan.active() ? Kind::Leave : Kind::Back, -1};
        if (lan.active()) {
            if (!lan.hosting()) continue;
            if (CheckCollisionPointRec(p, mode_button(w, h))) return {Kind::CycleMode, -1};
            if (CheckCollisionPointRec(p, level_button(w, h))) return {Kind::CycleLevel, -1};
            if (CheckCollisionPointRec(p, sides_button(w, h))) return {Kind::CycleSides, -1};
            if (CheckCollisionPointRec(p, start_button(w, h))) return {Kind::Start, -1};
            continue;
        }
        if (CheckCollisionPointRec(p, host_button(w, h))) return {Kind::Host, -1};
        const int games = std::min<int>(listed, static_cast<int>(lan.hosts().size()));
        for (int i = 0; i < games; ++i)
            if (CheckCollisionPointRec(p, game_button(i, w, h)) && lan.hosts()[static_cast<std::size_t>(i)].beacon.open) return {Kind::Join, i};
    }
    return {};
}

void LanScreen::draw(int w, int h, const net::LanSession& lan, const std::string& name) const {
    ClearBackground(Color{24, 31, 25, 255});
    DrawRectangleGradientV(0, 0, w, h, Color{52, 73, 56, 255}, Color{17, 20, 17, 255});
    const float s = scale(w, h);
    const int big = std::max(16, static_cast<int>(54 * s)), small = std::max(10, static_cast<int>(20 * s));
    const auto title = box(0, 50, 900, 60, w, h);
    const char* heading = lan.active() ? (lan.hosting() ? "YOUR LAN GAME" : "LAN LOBBY") : "LAN PLAY";
    DrawText(heading, w / 2 - MeasureText(heading, big) / 2, static_cast<int>(title.y), big, RAYWHITE);
    const auto line = box(0, 120, 900, 30, w, h);
    const std::string you = "You are " + name;
    DrawText(you.c_str(), w / 2 - MeasureText(you.c_str(), small) / 2, static_cast<int>(line.y), small, LIGHTGRAY);
    const auto status = box(0, 150, 900, 30, w, h);
    DrawText(lan.status().c_str(), w / 2 - MeasureText(lan.status().c_str(), small) / 2, static_cast<int>(status.y), small, Color{240, 210, 120, 255});
    if (lan.active()) {
        const auto& roster = lan.roster();
        const auto list = box(200, 190, 500, 222, w, h);
        const auto sides = static_cast<net::Sides>(roster.sides);
        DrawRectangleRounded(list, .06F, 8, Color{20, 26, 20, 200});
        int row = 0;
        for (const auto& p : roster.players) {
            const std::string text = (p.slot == 0 ? "HOST  " : "      ") + p.name + (p.slot == lan.slot() ? "  (you)" : "");
            const int y = static_cast<int>(list.y + (12 + 26 * static_cast<float>(row++)) * s);
            DrawText(text.c_str(), static_cast<int>(list.x + 20 * s), y, small, RAYWHITE);
            // Who is on whose side.
            if (sides != net::Sides::Free) {
                const std::uint8_t team = net::side_of(sides, p.slot);
                const char* tag = sides == net::Sides::Squad ? "SQUAD" : net::team_name(team);
                const Color ink = sides == net::Sides::Squad || team == net::side_of(sides, lan.slot()) ? Color{120, 230, 140, 255} : Color{240, 140, 110, 255};
                DrawText(tag, static_cast<int>(list.x + list.width - 20 * s) - MeasureText(tag, small), y, small, ink);
            }
        }
        if (lan.hosting()) {
            button(mode_button(w, h), TextFormat("MODE: %s", mode_label(roster.mode)), s);
            button(level_button(w, h), TextFormat("BOTS: %s", ai::bot_level_name(static_cast<ai::BotLevel>(roster.level))), s);
            button(sides_button(w, h), TextFormat("SIDES: %s", net::sides_name(sides)), s);
            button(start_button(w, h), roster.players.size() > 1 ? "START THE MATCH" : "START (ALONE FOR NOW)", s, true);
        } else {
            const std::string waiting = std::string(mode_label(roster.mode)) + " - bots " + ai::bot_level_name(static_cast<ai::BotLevel>(roster.level)) +
                                        " - " + net::sides_name(sides);
            const auto info = box(0, 470, 900, 30, w, h);
            DrawText(waiting.c_str(), w / 2 - MeasureText(waiting.c_str(), small) / 2, static_cast<int>(info.y), small, RAYWHITE);
            const char* hint = "Waiting for the host to start...";
            DrawText(hint, w / 2 - MeasureText(hint, small) / 2, static_cast<int>(info.y + 40 * s), small, LIGHTGRAY);
        }
        button(back_button(w, h), "LEAVE", s);
        return;
    }
    button(host_button(w, h), "HOST A GAME", s, true);
    const auto found = box(0, 290, 900, 30, w, h);
    const char* games = lan.hosts().empty() ? "No games found yet - same Wi-Fi as the host?" : "GAMES ON THIS NETWORK";
    DrawText(games, w / 2 - MeasureText(games, small) / 2, static_cast<int>(found.y), small, LIGHTGRAY);
    for (int i = 0; i < std::min<int>(listed, static_cast<int>(lan.hosts().size())); ++i) {
        const auto& host = lan.hosts()[static_cast<std::size_t>(i)];
        const std::string text = host.name + " - " + mode_label(host.beacon.mode) +
                                 (host.beacon.sides ? std::string(" - ") + net::sides_name(static_cast<net::Sides>(host.beacon.sides)) : std::string()) +
                                 " - " + std::to_string(host.beacon.players) + (host.beacon.open ? " in lobby" : " playing");
        button(game_button(i, w, h), text.c_str(), s);
    }
    button(back_button(w, h), "BACK", s);
}

}
