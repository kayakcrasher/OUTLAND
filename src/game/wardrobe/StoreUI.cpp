#include "outland/game/wardrobe/StoreUI.hpp"
#include "outland/input/PointerEvents.hpp"
#include <algorithm>

namespace outland::game::wardrobe {
namespace {
// The panel: 340 x 600 design units against the right edge, scaled to the screen.
float scale(int w, int h) { return std::max(.1F, std::min(w / 900.0F, h / 720.0F)); }
Rectangle panel(int w, int h) {
    const float s = scale(w, h);
    return {w - 360 * s, (h - 600 * s) / 2, 350 * s, 600 * s};
}
Rectangle at(float x, float y, float bw, float bh, int w, int h) {
    const Rectangle p = panel(w, h);
    const float s = p.height / 600;
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
void text(const char* t, float x, float y, float size, Color colour, int w, int h) {
    const Rectangle r = at(x, y, 1, size, w, h);
    write(t, r.x, r.y, std::max(10.0F, r.height), colour);
}
void label(Rectangle r, const char* t, Color fill, Color ink) {
    const float s = r.height / 44;
    DrawRectangleRec(r, fill);
    DrawRectangleLinesEx(r, 1, Color{200, 200, 190, 255});
    const float size = std::max(10.0F, 17 * s);
    write(t, r.x + (r.width - measure(t, size)) / 2, r.y + 13 * s, size, ink);
}
const Color green{0, 110, 60, 255}, panel_fill{18, 22, 20, 235};
}

void StoreUI::open(int look, int hat) {
    look_ = std::max(0, look);
    hat_ = hat;
    warm_ = true;
}

// 0 prev look, 1 next look, 2 buy/wear look, 3 prev hat, 4 next hat, 5 buy/wear hat, 6 no hat, 7 close
Rectangle StoreUI::button(int i, int w, int h) {
    switch (i) {
        case 0: return at(16, 214, 100, 44, w, h);
        case 1: return at(234, 214, 100, 44, w, h);
        case 2: return at(16, 270, 318, 48, w, h);
        case 3: return at(16, 420, 100, 44, w, h);
        case 4: return at(234, 420, 100, 44, w, h);
        case 5: return at(16, 476, 200, 44, w, h);
        case 6: return at(224, 476, 110, 44, w, h);
        default: return at(16, 540, 318, 44, w, h);
    }
}

std::vector<Vector2> StoreUI::clicks(bool focused) {
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

StoreAction StoreUI::input(const Catalogue& catalogue, int w, int h, bool focused) {
    StoreAction action;
    const int looks = static_cast<int>(catalogue.looks().size()), hats = static_cast<int>(catalogue.hats().size());
    if (looks == 0) {action.close = true; return action;}
    const auto step_look = [&](int d) { look_ = (look_ + d + looks) % looks; };
    const auto step_hat = [&](int d) { hat_ = hats ? (hat_ + 1 + d + hats + 1) % (hats + 1) - 1 : -1; };   // -1 is "none"
    for (const auto& p : clicks(focused)) {
        for (int i = 0; i < 8; ++i) {
            if (!CheckCollisionPointRec(p, button(i, w, h))) continue;
            if (i == 0) step_look(-1);
            if (i == 1) step_look(1);
            if (i == 2) action.buy_look = true;
            if (i == 3) step_hat(-1);
            if (i == 4) step_hat(1);
            if (i == 5 && hat_ >= 0) action.buy_hat = true;
            if (i == 6) {hat_ = -1; action.no_hat = true;}
            if (i == 7) action.close = true;
        }
    }
    if (focused && !warm_) {
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) step_look(-1);
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) step_look(1);
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) step_hat(-1);
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) step_hat(1);
        action.buy_look |= IsKeyPressed(KEY_ENTER);
        if (IsKeyPressed(KEY_T) && hat_ >= 0) action.buy_hat = true;
        if (IsKeyPressed(KEY_X)) {hat_ = -1; action.no_hat = true;}
        action.close |= IsKeyPressed(KEY_I) || IsKeyPressed(KEY_E);
    }
    look_ = std::clamp(look_, 0, looks - 1);
    warm_ = false;
    return action;
}

void StoreUI::draw(const Catalogue& catalogue, const economy::EconomyState& state, bool member, const std::string& status,
                   int w, int h) const {
    if (catalogue.looks().empty()) return;
    font = &font_;
    const Rectangle p = panel(w, h);
    DrawRectangleRec(p, panel_fill);
    DrawRectangleLinesEx(p, 2, green);
    DrawRectangleRec(at(0, 0, 350, 56, w, h), green);
    text("VESTAĴEJO VERDA", 16, 10, 24, WHITE, w, h);
    text(TextFormat("%lld kr", static_cast<long long>(state.money)), 16, 64, 20, GOLD, w, h);
    // The look being tried on.
    const auto& look = catalogue.looks()[static_cast<std::size_t>(look_)];
    const bool owned = owns(state, catalogue, look.id);
    const auto* on = wearing(state, catalogue);
    text(TextFormat("VESTOJ  %d / %d", look_ + 1, static_cast<int>(catalogue.looks().size())), 16, 100, 16, LIGHTGRAY, w, h);
    text(look.name.c_str(), 16, 124, 24, WHITE, w, h);
    const char* price = owned ? (on && on->id == look.id ? "You have it on" : "Yours") :
        look.green_hand && !member ? "La Verda Mano only" : TextFormat("%d kr", look.price);
    text(price, 16, 158, 20, owned ? Color{140, 220, 150, 255} : look.green_hand && !member ? Color{230, 120, 110, 255} : GOLD, w, h);
    label(button(0, w, h), "<", Color{40, 50, 44, 255}, WHITE);
    label(button(1, w, h), ">", Color{40, 50, 44, 255}, WHITE);
    label(button(2, w, h), owned ? "WEAR / ENTER" : "BUY / ENTER", green, WHITE);
    // Hats.
    DrawRectangleRec(at(16, 334, 318, 2, w, h), Color{60, 70, 64, 255});
    text("ĈAPELOJ", 16, 344, 16, LIGHTGRAY, w, h);
    if (hat_ < 0) text("Neniu ĉapelo", 16, 368, 22, WHITE, w, h);
    else {
        const auto& hat = catalogue.hats()[static_cast<std::size_t>(hat_)];
        text(hat.name.c_str(), 16, 368, 22, WHITE, w, h);
        const bool mine = owns(state, catalogue, hat.id);
        text(mine ? (state.hat == hat.id ? "You have it on" : "Yours") : TextFormat("%d kr", hat.price), 200, 372, 18,
             mine ? Color{140, 220, 150, 255} : GOLD, w, h);
    }
    label(button(3, w, h), "<", Color{40, 50, 44, 255}, WHITE);
    label(button(4, w, h), ">", Color{40, 50, 44, 255}, WHITE);
    const bool hat_owned = hat_ >= 0 && owns(state, catalogue, catalogue.hats()[static_cast<std::size_t>(hat_)].id);
    label(button(5, w, h), hat_ < 0 ? "-" : hat_owned ? "WEAR / T" : "BUY / T", hat_ < 0 ? Color{40, 50, 44, 255} : green, WHITE);
    label(button(6, w, h), "NO HAT / X", Color{40, 50, 44, 255}, WHITE);
    label(button(7, w, h), "CLOSE / ESC", Color{70, 40, 40, 255}, WHITE);
    if (!status.empty()) text(status.c_str(), 16, 590, 15, GOLD, w, h);
    font = nullptr;
}

} // namespace outland::game::wardrobe
