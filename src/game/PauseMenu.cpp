#include "outland/game/PauseMenu.hpp"
#include "outland/input/PointerEvents.hpp"
#include <algorithm>
#include <cmath>

namespace outland::game {
namespace {
const Color veil{6, 10, 8, 170}, panel_colour{14, 20, 16, 240}, ink{236, 240, 226, 255}, dim{168, 178, 160, 255},
    accent{185, 202, 139, 255}, idle{46, 58, 48, 255}, hot{230, 170, 60, 255};
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
void button(Rectangle r, const char* label, bool active, const Font* font, float s, float size = 17) {
    DrawRectangleRounded(r, .25F, 6, active ? accent : idle);
    // Long names shrink to fit their button.
    float fit = size * s;
    while (fit > 9 && measure(font, label, fit) > r.width - 10 * s) fit -= 1;
    centred(font, label, {r.x + r.width / 2, r.y + r.height / 2}, fit, active ? Color{20, 28, 20, 255} : ink);
}
constexpr const char* main_labels[]{"RESUME", "SETTINGS", "CONTROLS", "SAVE GAME", "QUIT TO MENU", "QUIT GAME"};
bool toggle_row(PauseMenu::Row row) {
    using R = PauseMenu::Row;
    return row == R::InvertY || row == R::AimToggle || row == R::AimAssist || row == R::TouchControls || row == R::Sound || row == R::Graphics;
}
int rows_per_column() { return static_cast<int>((input::action_count + 1) / 2); }
} // namespace

void PauseMenu::open() {
    open_ = true;
    warm_ = true;
    page_ = Page::Main;
    capturing_ = -1;
}

Rectangle PauseMenu::menu_button(int width, int) {
    return {static_cast<float>(width - 174), 44.0F, 162.0F, 34.0F};
}

bool PauseMenu::tapped_menu_button(int width, int height, bool focused) {
    bool hit = false;
    for (const auto& p : clicks(focused)) hit |= CheckCollisionPointRec(p, menu_button(width, height));
    return hit;
}

Rectangle PauseMenu::panel(int width, int height, Page page) {
    const float s = scale_of(width, height), w = static_cast<float>(width), h = static_cast<float>(height);
    float pw = 420 * s, ph = 470 * s;
    if (page == Page::Settings) { pw = 760 * s; ph = (150 + row_count * 40) * s; }
    if (page == Page::Controls) { pw = 1120 * s; ph = (175 + rows_per_column() * 36) * s; }
    pw = std::min(pw, w - 16);
    ph = std::min(ph, h - 16);
    return {(w - pw) / 2, (h - ph) / 2, pw, ph};
}

Rectangle PauseMenu::main_button(int index, int width, int height) {
    const float s = scale_of(width, height);
    const auto p = panel(width, height, Page::Main);
    return {p.x + 40 * s, p.y + 82 * s + static_cast<float>(index) * 60 * s, p.width - 80 * s, 48 * s};
}

Rectangle PauseMenu::row_button(Row row, int side, int width, int height) {
    const float s = scale_of(width, height);
    const auto p = panel(width, height, Page::Settings);
    const float y = p.y + 66 * s + static_cast<float>(static_cast<int>(row)) * 40 * s, right = p.x + p.width - 24 * s;
    if (toggle_row(row)) return {right - 170 * s, y, 170 * s, 34 * s};
    return side == 0 ? Rectangle{right - 170 * s, y, 46 * s, 34 * s} : Rectangle{right - 46 * s, y, 46 * s, 34 * s};
}

Rectangle PauseMenu::slot(int action, int slot_index, int width, int height) {
    const float s = scale_of(width, height);
    const auto p = panel(width, height, Page::Controls);
    const int per = rows_per_column(), column = action / per, row = action % per;
    const float column_width = (p.width - 72 * s) / 2;
    const float x = p.x + 24 * s + static_cast<float>(column) * (column_width + 24 * s);
    const float label = column_width * .44F, slot_width = (column_width - label - 8 * s) / 2;
    return {x + label + static_cast<float>(slot_index) * (slot_width + 8 * s), p.y + 66 * s + static_cast<float>(row) * 36 * s,
            slot_width, 30 * s};
}

Rectangle PauseMenu::footer_button(int index, Page page, int width, int height) {
    const float s = scale_of(width, height);
    const auto p = panel(width, height, page);
    const float y = p.y + p.height - 58 * s;
    return index == 0 ? Rectangle{p.x + 24 * s, y, 170 * s, 42 * s} : Rectangle{p.x + p.width - 24 * s - 240 * s, y, 240 * s, 42 * s};
}

std::vector<Vector2> PauseMenu::clicks(bool focused) {
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

PauseMenu::Result PauseMenu::update(GameSettings& settings, int width, int height, bool focused, float dt, bool can_quit_game) {
    status_time_ = std::max(0.0F, status_time_ - dt);
    if (!open_) return Result::None;
    // Rebinding: the next key or mouse button (not the click that chose the slot) takes the slot.
    if (capturing_ >= 0) {
        clicks(focused); // swallow taps while waiting
        if (capture_wait_ > 0) { --capture_wait_; return Result::None; }
        if (!focused) return Result::None;
        if (IsKeyPressed(KEY_ESCAPE)) { capturing_ = -1; return Result::None; }
        const int code = input::Bindings::captured();
        if (code == 0) return Result::None;
        const auto action = static_cast<input::Action>(capturing_);
        settings.bindings.bind(action, capture_slot_, code == KEY_BACKSPACE || code == KEY_DELETE ? 0 : code);
        capturing_ = -1;
        return Result::SettingsChanged;
    }
    Result result = Result::None;
    const auto back = [&] {
        if (page_ == Page::Main) result = Result::Resume;
        else page_ = Page::Main;
    };
    if (focused && IsKeyPressed(KEY_ESCAPE) && !warm_) back();
    const auto changed = [&] { if (result == Result::None) result = Result::SettingsChanged; };
    const auto step = [&](float& value, float by, float lo, float hi) {
        const float next = std::clamp(std::round((value + by) * 100) / 100, lo, hi);
        if (next != value) { value = next; changed(); }
    };
    for (const auto& p : clicks(focused)) {
        if (page_ == Page::Main) {
            for (int i = 0; i < (can_quit_game ? 6 : 5); ++i) {
                if (!CheckCollisionPointRec(p, main_button(i, width, height))) continue;
                switch (i) {
                case 0: result = Result::Resume; break;
                case 1: page_ = Page::Settings; break;
                case 2: page_ = Page::Controls; break;
                case 3: result = Result::Save; break;
                case 4: result = Result::QuitHome; break;
                default: result = Result::QuitGame; break;
                }
            }
            continue;
        }
        if (CheckCollisionPointRec(p, footer_button(0, page_, width, height))) { page_ = Page::Main; continue; }
        if (CheckCollisionPointRec(p, footer_button(1, page_, width, height))) {
            if (page_ == Page::Settings) settings.reset_controls();
            else settings.bindings = input::Bindings::defaults();
            changed();
            continue;
        }
        if (page_ == Page::Controls) {
            for (int a = 0; a < static_cast<int>(input::action_count); ++a)
                for (int k = 0; k < 2; ++k)
                    if (CheckCollisionPointRec(p, slot(a, k, width, height))) { capturing_ = a; capture_slot_ = k; capture_wait_ = 1; }
            continue;
        }
        for (int r = 0; r < row_count; ++r) {
            const auto row = static_cast<Row>(r);
            const int sides = toggle_row(row) ? 1 : 2;
            for (int side = 2 - sides; side < 2; ++side) {
                if (!CheckCollisionPointRec(p, row_button(row, side, width, height))) continue;
                const float sign = side == 0 ? -1.0F : 1.0F;
                switch (row) {
                case Row::Mouse: step(settings.mouse_sensitivity, .1F * sign, GameSettings::min_mouse, GameSettings::max_mouse); break;
                case Row::Touch: step(settings.look_sensitivity, .1F * sign, GameSettings::min_sensitivity, GameSettings::max_sensitivity); break;
                case Row::AimSensitivity: step(settings.aim_sensitivity, .05F * sign, GameSettings::min_aim, GameSettings::max_aim); break;
                case Row::ButtonSize: step(settings.button_size, .1F * sign, GameSettings::min_buttons, GameSettings::max_buttons); break;
                case Row::Volume: step(settings.volume, .1F * sign, 0, 1); break;
                case Row::InvertY: settings.invert_y = !settings.invert_y; changed(); break;
                case Row::AimToggle: settings.aim_toggle = !settings.aim_toggle; changed(); break;
                case Row::AimAssist: settings.aim_assist = !settings.aim_assist; changed(); break;
                case Row::Sound: settings.muted = !settings.muted; changed(); break;
                case Row::TouchControls:
                    settings.touch_controls = static_cast<GameSettings::TouchControls>((static_cast<int>(settings.touch_controls) + 1) % 3);
                    changed();
                    break;
                case Row::Graphics:
                    settings.graphics = static_cast<GameSettings::Graphics>((static_cast<int>(settings.graphics) + 1) % 3);
                    changed();
                    break;
                case Row::Count: break;
                }
            }
        }
    }
    if (result == Result::Resume || result == Result::QuitHome || result == Result::QuitGame) close();
    return result;
}

void PauseMenu::draw(const GameSettings& settings, int width, int height, const Font* font, bool can_quit_game) const {
    const float s = scale_of(width, height);
    if (status_time_ > 0 && !status_.empty()) {
        const float size = 20 * s, tw = measure(font, status_.c_str(), size);
        const float alpha = std::min(1.0F, status_time_ * 2);
        DrawRectangleRounded({width / 2 - tw / 2 - 16 * s, 96 * s, tw + 32 * s, size + 16 * s}, .3F, 6, Fade(BLACK, .7F * alpha));
        text(font, status_.c_str(), {width / 2 - tw / 2, 104 * s}, size, Fade(accent, alpha));
    }
    if (!open_) return;
    DrawRectangle(0, 0, width, height, veil);
    const auto p = panel(width, height, page_);
    DrawRectangleRounded(p, .04F, 8, panel_colour);
    DrawRectangleRoundedLinesEx(p, .04F, 8, 2, Fade(accent, .5F));
    const char* titles[]{"PAUSED", "SETTINGS", "CONTROLS"};
    centred(font, titles[static_cast<int>(page_)], {p.x + p.width / 2, p.y + 34 * s}, 30 * s, accent);
    if (page_ == Page::Main) {
        for (int i = 0; i < (can_quit_game ? 6 : 5); ++i) button(main_button(i, width, height), main_labels[i], i == 0, font, s, 20);
        centred(font, "ESC resumes", {p.x + p.width / 2, p.y + p.height - 22 * s}, 14 * s, dim);
        return;
    }
    button(footer_button(0, page_, width, height), "BACK", false, font, s);
    button(footer_button(1, page_, width, height), page_ == Page::Settings ? "RESET CONTROLS" : "RESET KEYS", false, font, s);
    if (page_ == Page::Settings) {
        struct Line { Row row; const char* label; const char* hint; };
        const Line lines[]{
            {Row::Mouse, "Mouse sensitivity", "PC: how far the view turns per mouse movement"},
            {Row::Touch, "Touch look sensitivity", "phones: swipe on the right of the screen to look"},
            {Row::AimSensitivity, "Aiming sensitivity", "extra slow-down while aiming down sights"},
            {Row::InvertY, "Invert look up/down", nullptr},
            {Row::AimToggle, "Aim (right mouse)", nullptr},
            {Row::AimAssist, "Touch aim assist", "slows and nudges the view onto enemies"},
            {Row::TouchControls, "On-screen controls", "AUTO: on phones and touch screens"},
            {Row::ButtonSize, "Touch button size", nullptr},
            {Row::Volume, "Volume", nullptr},
            {Row::Sound, "Sound", nullptr},
            {Row::Graphics, "Graphics", "AUTO adapts resolution to keep play smooth"},
        };
        const char* touch_modes[]{"AUTO", "ON", "OFF"};
        const char* graphics_modes[]{"AUTO", "FAST", "SHARP"};
        for (const auto& line : lines) {
            const auto r = row_button(line.row, 1, width, height);
            text(font, line.label, {p.x + 24 * s, r.y + 2 * s}, 17 * s, ink);
            if (line.hint && p.width > 600 * s) text(font, line.hint, {p.x + 24 * s, r.y + 20 * s}, 11 * s, dim);
            const char* value = nullptr;
            bool on = false;
            switch (line.row) {
            case Row::Mouse: value = TextFormat("%.1f", settings.mouse_sensitivity); break;
            case Row::Touch: value = TextFormat("%.1f", settings.look_sensitivity); break;
            case Row::AimSensitivity: value = TextFormat("%.2f", settings.aim_sensitivity); break;
            case Row::ButtonSize: value = TextFormat("%d%%", static_cast<int>(std::round(settings.button_size * 100))); break;
            case Row::Volume: value = TextFormat("%d%%", static_cast<int>(std::round(settings.volume * 100))); break;
            case Row::InvertY: on = settings.invert_y; button(r, on ? "ON" : "OFF", on, font, s); break;
            case Row::AimToggle: on = settings.aim_toggle; button(r, on ? "TOGGLE" : "HOLD", on, font, s); break;
            case Row::AimAssist: on = settings.aim_assist; button(r, on ? "ON" : "OFF", on, font, s); break;
            case Row::Sound: on = !settings.muted; button(r, on ? "ON" : "OFF", on, font, s); break;
            case Row::TouchControls:
                button(r, touch_modes[static_cast<int>(settings.touch_controls)], settings.touch_controls != GameSettings::TouchControls::Off, font, s);
                break;
            case Row::Graphics: button(r, graphics_modes[static_cast<int>(settings.graphics)], true, font, s); break;
            case Row::Count: break;
            }
            if (value) {
                const auto minus = row_button(line.row, 0, width, height);
                button(minus, "-", false, font, s, 22);
                button(r, "+", false, font, s, 22);
                centred(font, value, {(minus.x + minus.width + r.x) / 2, r.y + r.height / 2}, 18 * s, accent);
            }
        }
        return;
    }
    // CONTROLS: every action with its two bindings.
    for (int a = 0; a < static_cast<int>(input::action_count); ++a) {
        const auto action = static_cast<input::Action>(a);
        const auto first = slot(a, 0, width, height);
        const float column_left = first.x - (first.width * 2 + 8 * s) * .44F / .56F;
        float size = 15 * s;
        const char* label = input::Bindings::label(action);
        while (size > 9 && measure(font, label, size) > first.x - column_left - 8 * s) size -= 1;
        text(font, label, {column_left, first.y + first.height / 2 - size / 2}, size, ink);
        for (int k = 0; k < 2; ++k) {
            const auto r = slot(a, k, width, height);
            if (capturing_ == a && capture_slot_ == k) {
                DrawRectangleRounded(r, .25F, 6, hot);
                centred(font, "PRESS A KEY", {r.x + r.width / 2, r.y + r.height / 2}, 13 * s, Color{30, 20, 10, 255});
            } else {
                button(r, input::Bindings::name(settings.bindings[action][static_cast<std::size_t>(k)]).c_str(), false, font, s, 14);
            }
        }
    }
    const char* note = capturing_ >= 0 ? "Press a key or mouse button.  ESC cancels, BACKSPACE clears the slot."
                                       : "Click a slot to change it.  The mouse looks around; ESC opens this menu.";
    centred(font, note, {p.x + p.width / 2, p.y + p.height - 82 * s}, 14 * s, dim);
}

} // namespace outland::game
