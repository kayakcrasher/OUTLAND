#include "outland/game/casino/PokerTable.hpp"
#include "outland/input/PointerEvents.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::casino {

namespace {
float scale(int w, int h) { return std::max(.1F, std::min(w / 900.0F, h / 720.0F)); }
Vector2 at(float x, float y, int w, int h) {
    const float s = scale(w, h);
    return {(w - 900 * s) / 2 + x * s, (h - 720 * s) / 2 + y * s};
}
Rectangle box(float x, float y, float bw, float bh, int w, int h) {
    const float s = scale(w, h);
    const Vector2 p = at(x, y, w, h);
    return {p.x, p.y, bw * s, bh * s};
}
void text(const char* t, Vector2 centre, int size, Color color) {
    DrawText(t, static_cast<int>(centre.x) - MeasureText(t, size) / 2, static_cast<int>(centre.y) - size / 2, size, color);
}
Color suit_color(int suit) { return suit == 1 || suit == 2 ? Color{196, 30, 40, 255} : Color{24, 24, 30, 255}; }
// Suits drawn from shapes (the default font has no suit glyphs).
void draw_suit(Vector2 c, float r, int suit) {
    const Color col = suit_color(suit);
    switch (suit) {
        case 1: // heart
            DrawCircleV({c.x - r * .5F, c.y - r * .2F}, r * .55F, col);
            DrawCircleV({c.x + r * .5F, c.y - r * .2F}, r * .55F, col);
            DrawTriangle({c.x - r * 1.02F, c.y}, {c.x, c.y + r * 1.1F}, {c.x + r * 1.02F, c.y}, col);
            break;
        case 2: // diamond
            DrawTriangle({c.x, c.y - r * 1.1F}, {c.x - r * .75F, c.y}, {c.x + r * .75F, c.y}, col);
            DrawTriangle({c.x - r * .75F, c.y}, {c.x, c.y + r * 1.1F}, {c.x + r * .75F, c.y}, col);
            break;
        case 3: // club
            DrawCircleV({c.x, c.y - r * .5F}, r * .45F, col);
            DrawCircleV({c.x - r * .5F, c.y + r * .15F}, r * .45F, col);
            DrawCircleV({c.x + r * .5F, c.y + r * .15F}, r * .45F, col);
            DrawTriangle({c.x, c.y}, {c.x - r * .35F, c.y + r * 1.0F}, {c.x + r * .35F, c.y + r * 1.0F}, col);
            break;
        default: // spade
            DrawCircleV({c.x - r * .45F, c.y + r * .15F}, r * .5F, col);
            DrawCircleV({c.x + r * .45F, c.y + r * .15F}, r * .5F, col);
            DrawTriangle({c.x, c.y - r * 1.05F}, {c.x - r * .95F, c.y + r * .1F}, {c.x + r * .95F, c.y + r * .1F}, col);
            DrawTriangle({c.x, c.y + r * .1F}, {c.x - r * .35F, c.y + r * 1.0F}, {c.x + r * .35F, c.y + r * 1.0F}, col);
            break;
    }
}
// Seat centres on the 900 x 720 layout: you at the bottom, the Krimuloj left, top and right.
Vector2 seat_at(std::size_t seat) {
    static constexpr Vector2 spots[]{{450, 520}, {130, 300}, {450, 105}, {770, 300}};
    return spots[seat % 4];
}
}

Rectangle PokerTable::button(int i, int w, int h) {
    const float x[]{40, 210, 380, 550, 740};
    return box(x[std::clamp(i, 0, 4)], 640, i == 4 ? 120 : 160, 56, w, h);
}

std::vector<Vector2> PokerTable::clicks(bool focused) {
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
    return out;
}

PokerInput PokerTable::input(const Poker& poker, int w, int h, bool focused) {
    PokerInput result = PokerInput::None;
    const bool turn = poker.human_turn(), between = !poker.hand_running();
    for (const auto& p : clicks(focused)) {
        if (turn && CheckCollisionPointRec(p, button(0, w, h))) result = PokerInput::Fold;
        if (turn && CheckCollisionPointRec(p, button(1, w, h))) result = PokerInput::Call;
        if (turn && poker.can_raise(0) && CheckCollisionPointRec(p, button(2, w, h))) result = PokerInput::Raise;
        if (between && CheckCollisionPointRec(p, button(3, w, h))) result = PokerInput::Deal;
        if (CheckCollisionPointRec(p, button(4, w, h))) result = PokerInput::Leave;
    }
    if (focused && !warm_) {
        if (turn && IsKeyPressed(KEY_F)) result = PokerInput::Fold;
        if (turn && IsKeyPressed(KEY_C)) result = PokerInput::Call;
        if (turn && poker.can_raise(0) && IsKeyPressed(KEY_R)) result = PokerInput::Raise;
        if (between && (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ENTER))) result = PokerInput::Deal;
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_L)) result = PokerInput::Leave;
    }
    warm_ = false;
    return result;
}

void PokerTable::draw_card(Rectangle r, Card card, bool face_up) {
    const float round = .18F;
    if (!face_up) {
        DrawRectangleRounded(r, round, 6, Color{150, 24, 34, 255});
        DrawRectangleRoundedLinesEx({r.x + r.width * .1F, r.y + r.height * .08F, r.width * .8F, r.height * .84F}, round, 6, 2, Color{235, 205, 120, 255});
        DrawCircleV({r.x + r.width / 2, r.y + r.height / 2}, r.width * .16F, Color{46, 204, 96, 255}); // the Green Hand's mark
        return;
    }
    DrawRectangleRounded(r, round, 6, Color{250, 248, 240, 255});
    DrawRectangleRoundedLinesEx(r, round, 6, 1.5F, Color{90, 90, 90, 255});
    const std::string rank = rank_name(card.rank);
    const int size = static_cast<int>(r.height * .3F);
    DrawText(rank.c_str(), static_cast<int>(r.x + r.width * .1F), static_cast<int>(r.y + r.height * .06F), size, suit_color(card.suit));
    draw_suit({r.x + r.width * .62F, r.y + r.height * .66F}, r.width * .2F, card.suit);
}

void PokerTable::draw(const Poker& poker, long long wallet, const std::string& house, int w, int h, float time) const {
    DrawRectangle(0, 0, w, h, Color{10, 14, 12, 225});
    const float s = scale(w, h);
    // The table: walnut rail round green felt.
    const Vector2 centre = at(450, 320, w, h);
    DrawEllipse(static_cast<int>(centre.x), static_cast<int>(centre.y), 410 * s, 235 * s, Color{92, 52, 30, 255});
    DrawEllipse(static_cast<int>(centre.x), static_cast<int>(centre.y), 385 * s, 212 * s, Color{24, 112, 58, 255});
    DrawEllipseLines(static_cast<int>(centre.x), static_cast<int>(centre.y), 330 * s, 165 * s, Color{40, 140, 76, 255});
    text(("LA KAZINO DE " + house).c_str(), at(450, 22, w, h), static_cast<int>(22 * s), Color{235, 205, 120, 255});
    text("Texas Hold'em - limit 10/20 - blinds 5/10", at(450, 48, w, h), static_cast<int>(14 * s), Color{200, 210, 200, 255});
    // Board and pot.
    const auto& board = poker.board();
    const float cw = 58 * s, ch = 82 * s;
    for (int i = 0; i < 5; ++i) {
        const Vector2 p = at(450 - 2.5F * 66 + static_cast<float>(i) * 66 + 4, 268, w, h);
        const Rectangle r{p.x, p.y, cw, ch};
        if (static_cast<std::size_t>(i) < board.size()) draw_card(r, board[static_cast<std::size_t>(i)], true);
        else DrawRectangleRoundedLinesEx(r, .18F, 6, 1.5F, Color{60, 150, 90, 255});
    }
    text(TextFormat("POT %d", poker.pot()), at(450, 380, w, h), static_cast<int>(22 * s), Color{250, 230, 150, 255});
    // Seats.
    const auto& seats = poker.seats();
    const bool showdown = poker.street() == Street::Showdown && poker.hands() > 0;
    for (std::size_t i = 0; i < seats.size() && i < 4; ++i) {
        const auto& seat = seats[i];
        const Vector2 c = at(seat_at(i).x, seat_at(i).y, w, h);
        const bool acting = poker.hand_running() && poker.to_act() == static_cast<int>(i);
        const Rectangle plate{c.x - 95 * s, c.y + 44 * s, 190 * s, 52 * s};
        DrawRectangleRounded(plate, .3F, 6, acting ? Color{64, 54, 20, 235} : Color{18, 24, 20, 225});
        DrawRectangleRoundedLinesEx(plate, .3F, 6, acting ? 3 : 1, acting ? Color{250, 215, 90, static_cast<unsigned char>(180 + 75 * std::sin(time * 6))} : Color{90, 110, 95, 255});
        text(seat.name.c_str(), {c.x, c.y + 58 * s}, static_cast<int>(16 * s), seat.folded ? GRAY : RAYWHITE);
        text(TextFormat("%d", seat.stack), {c.x, c.y + 80 * s}, static_cast<int>(16 * s), Color{250, 230, 150, 255});
        if (poker.hands() > 0 && !(seat.folded && !seat.human)) {
            const bool face = seat.human || (showdown && !seat.folded && poker.live_seats() > 1);
            for (int k = 0; k < 2; ++k)
                draw_card({c.x - cw - 2 * s + static_cast<float>(k) * (cw + 4 * s), c.y - ch / 2 - 6 * s, cw, ch}, seat.hole[static_cast<std::size_t>(k)], face);
        }
        if (!seat.last.empty()) text(seat.last.c_str(), {c.x, c.y + 108 * s}, static_cast<int>(14 * s), Color{200, 230, 200, 255});
        if (static_cast<int>(i) == poker.dealer()) {
            const Vector2 d{c.x + 110 * s, c.y + 60 * s};
            DrawCircleV(d, 13 * s, RAYWHITE);
            text("D", d, static_cast<int>(14 * s), BLACK);
        }
        if (seat.bet > 0 && poker.hand_running()) {
            const Vector2 chip = Vector2Lerp({c.x, c.y + 70 * s}, centre, .6F);
            DrawCircleV(chip, 12 * s, Color{200, 40, 40, 255});
            DrawCircleLinesV(chip, 9 * s, RAYWHITE);
            text(TextFormat("%d", seat.bet), {chip.x, chip.y + 22 * s}, static_cast<int>(14 * s), RAYWHITE);
        }
    }
    if (!poker.result().empty()) {
        const Vector2 r = at(450, 420, w, h);
        const int size = static_cast<int>(17 * s);
        const int width = MeasureText(poker.result().c_str(), size);
        DrawRectangle(static_cast<int>(r.x) - width / 2 - 12, static_cast<int>(r.y) - size, width + 24, size * 2, Color{0, 0, 0, 170});
        text(poker.result().c_str(), r, size, Color{255, 225, 120, 255});
    }
    // Buttons.
    const bool turn = poker.human_turn();
    const int owed = poker.to_call(0);
    const char* labels[]{"FOLD (F)", owed > 0 ? TextFormat("CALL %d (C)", owed) : "CHECK (C)",
                         TextFormat("%s %d (R)", poker.to_call(0) > 0 || poker.street() == Street::Preflop ? "RAISE" : "BET", poker.raise_size()),
                         poker.hands() == 0 ? "DEAL (N)" : "NEXT HAND (N)", "LEAVE"};
    const bool enabled[]{turn, turn, turn && poker.can_raise(0), !poker.hand_running() && poker.stack() > 0, true};
    for (int i = 0; i < 5; ++i) {
        const Rectangle r = button(i, w, h);
        DrawRectangleRounded(r, .25F, 6, enabled[i] ? (i == 2 ? Color{140, 90, 20, 255} : i == 0 ? Color{110, 34, 34, 255} : Color{36, 92, 52, 255}) : Color{40, 44, 42, 200});
        DrawRectangleRoundedLinesEx(r, .25F, 6, 1, enabled[i] ? LIGHTGRAY : DARKGRAY);
        text(labels[i], {r.x + r.width / 2, r.y + r.height / 2}, static_cast<int>(16 * s), enabled[i] ? RAYWHITE : GRAY);
    }
    text(TextFormat("Wallet %lld kr   -   at the table %d", wallet, poker.stack()), at(450, 712, w, h), static_cast<int>(14 * s), Color{200, 210, 200, 255});
}

} // namespace outland::game::casino
