#include "outland/game/autos/AutoMenu.hpp"
#include "outland/input/PointerEvents.hpp"
#include <algorithm>

namespace outland::game::autos {
namespace {
// The panel: 380 x 620 design units against the right edge, scaled to the screen.
constexpr int visible_rows = 8;
float scale(int w, int h) { return std::max(.1F, std::min(w / 900.0F, h / 720.0F)); }
Rectangle panel(int w, int h) {
    const float s = scale(w, h);
    return {w - 390 * s, (h - 620 * s) / 2, 380 * s, 620 * s};
}
Rectangle at(float x, float y, float bw, float bh, int w, int h) {
    const Rectangle p = panel(w, h);
    const float s = p.height / 620;
    return {p.x + x * s, p.y + y * s, bw * s, bh * s};
}
const Font* font = nullptr;   // set for the length of a draw
float measure(const char* t, float size) {
    return font && font->baseSize > 0 ? MeasureTextEx(*font, t, size, size / 10).x : static_cast<float>(MeasureText(t, static_cast<int>(size)));
}
void write(const char* t, float x, float y, float size, Color colour) {
    if (font && font->baseSize > 0) DrawTextEx(*font, t, {x, y}, size, size / 10, colour);
    else DrawText(t, static_cast<int>(x), static_cast<int>(y), static_cast<int>(size), colour);
}
const Color accent{226, 110, 30, 255}, panel_fill{16, 20, 26, 238}, row_fill{34, 40, 50, 255}, row_pick{60, 74, 96, 255};
}

void AutoMenu::open(std::string title, std::string subtitle, std::vector<MenuRow> rows, std::string back_label, int selected) {
    title_ = std::move(title);
    subtitle_ = std::move(subtitle);
    back_label_ = std::move(back_label);
    rows_ = std::move(rows);
    selected_ = std::clamp(selected, 0, std::max(0, static_cast<int>(rows_.size()) - 1));
    first_ = std::max(0, selected_ - visible_rows + 1);
    open_ = true;
    warm_ = true;
}

void AutoMenu::set_rows(std::vector<MenuRow> rows) {
    rows_ = std::move(rows);
    selected_ = std::clamp(selected_, 0, std::max(0, static_cast<int>(rows_.size()) - 1));
}

Rectangle AutoMenu::row_rect(int i, int w, int h) const { return at(14, 118 + static_cast<float>(i - first_) * 54, 352, 50, w, h); }
Rectangle AutoMenu::back_rect(int w, int h) const { return at(14, 556, 352, 46, w, h); }

std::vector<Vector2> AutoMenu::clicks(bool focused) {
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

MenuAction AutoMenu::input(int w, int h, bool focused) {
    MenuAction action;
    if (!open_) return action;
    const int n = static_cast<int>(rows_.size());
    for (const auto& p : clicks(focused)) {
        if (CheckCollisionPointRec(p, back_rect(w, h))) { action.back = true; continue; }
        for (int i = first_; i < std::min(n, first_ + visible_rows); ++i)
            if (CheckCollisionPointRec(p, row_rect(i, w, h))) {
                selected_ = i;
                if (rows_[static_cast<std::size_t>(i)].enabled) action.chosen = i;
            }
    }
    if (focused && !warm_) {
        if (n > 0 && (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))) selected_ = (selected_ + n - 1) % n;
        if (n > 0 && (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))) selected_ = (selected_ + 1) % n;
        if (n > 0 && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E)) && rows_[static_cast<std::size_t>(selected_)].enabled) action.chosen = selected_;
        action.back |= IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE);
    }
    if (selected_ < first_) first_ = selected_;
    if (selected_ >= first_ + visible_rows) first_ = selected_ - visible_rows + 1;
    warm_ = false;
    return action;
}

void AutoMenu::draw(long long money, int w, int h) const {
    if (!open_) return;
    font = &font_;
    const Rectangle p = panel(w, h);
    const float s = p.height / 620;
    DrawRectangleRec(p, panel_fill);
    DrawRectangleLinesEx(p, 2, accent);
    const Rectangle head = at(0, 0, 380, 58, w, h);
    DrawRectangleRec(head, accent);
    write(title_.c_str(), head.x + 14 * s, head.y + 10 * s, 26 * s, WHITE);
    const Rectangle sub = at(14, 66, 1, 18, w, h);
    write(subtitle_.c_str(), sub.x, sub.y, std::max(10.0F, 16 * s), LIGHTGRAY);
    const char* cash = TextFormat("%lld kr", money);
    write(cash, p.x + p.width - 14 * s - measure(cash, 20 * s), sub.y + 20 * s, 20 * s, GOLD);
    const int n = static_cast<int>(rows_.size());
    for (int i = first_; i < std::min(n, first_ + visible_rows); ++i) {
        const auto& row = rows_[static_cast<std::size_t>(i)];
        const Rectangle r = row_rect(i, w, h);
        DrawRectangleRec(r, i == selected_ ? row_pick : row_fill);
        if (i == selected_) DrawRectangleLinesEx(r, 2, accent);
        if (row.current) DrawRectangle(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(5 * s), static_cast<int>(r.height), Color{120, 210, 140, 255});
        const Color ink = row.enabled ? WHITE : Color{130, 130, 130, 255};
        write(row.label.c_str(), r.x + 12 * s, r.y + 6 * s, 19 * s, ink);
        write(row.detail.c_str(), r.x + 12 * s, r.y + 28 * s, std::max(9.0F, 14 * s), Color{170, 176, 186, 255});
        if (!row.price.empty())
            write(row.price.c_str(), r.x + r.width - 10 * s - measure(row.price.c_str(), 18 * s), r.y + 6 * s, 18 * s,
                  row.current ? Color{140, 220, 150, 255} : row.enabled ? GOLD : Color{150, 120, 110, 255});
    }
    if (first_ > 0) write("^", p.x + p.width / 2, at(0, 102, 1, 1, w, h).y, 16 * s, LIGHTGRAY);
    if (first_ + visible_rows < n) write("v", p.x + p.width / 2, at(0, 540, 1, 1, w, h).y, 16 * s, LIGHTGRAY);
    const Rectangle back = back_rect(w, h);
    DrawRectangleRec(back, Color{70, 40, 40, 255});
    DrawRectangleLinesEx(back, 1, Color{200, 200, 190, 255});
    const std::string label = back_label_ + " / ESC";
    write(label.c_str(), back.x + (back.width - measure(label.c_str(), 18 * s)) / 2, back.y + 13 * s, 18 * s, WHITE);
    if (!status_.empty()) {
        const Rectangle note = at(14, 530, 1, 16, w, h);
        write(status_.c_str(), note.x, note.y - 2 * s, std::max(10.0F, 15 * s), GOLD);
    }
    font = nullptr;
}

} // namespace outland::game::autos
