#include "outland/game/TouchLayoutEditor.hpp"
#include <algorithm>
#include <cmath>

namespace outland::game {
namespace {
const input::TouchCustom& custom_of(input::TouchOverrides& work, const input::TouchElementInfo& e) {
    auto found = work.find(e.key);
    if (found == work.end()) {
        const auto& d = input::TouchLayout{}.*e.member;
        found = work.emplace(e.key, input::TouchCustom{d.x, d.y, 1}).first;
    }
    return found->second;
}
void label(const Font* font, const char* text, Vector2 centre, float size, Color colour) {
    if (font && font->texture.id) {
        const Vector2 m = MeasureTextEx(*font, text, size, 1);
        DrawTextEx(*font, text, {centre.x - m.x / 2, centre.y - m.y / 2}, size, 1, colour);
    } else {
        const int s = static_cast<int>(size);
        DrawText(text, static_cast<int>(centre.x - MeasureText(text, s) / 2.0F), static_cast<int>(centre.y - size / 2), s, colour);
    }
}
}

void TouchLayoutEditor::open(const GameSettings& settings) {
    open_ = true;
    warm_ = true;
    work_ = settings.touch_layout;
    selected_ = dragging_ = -1;
    driving_ = false;
    was_down_ = false;
}

Rectangle TouchLayoutEditor::tool(int index, int width, int height) {
    const float s = std::clamp(std::min(width / 1280.0F, height / 720.0F), .5F, 1.6F);
    const float w = 128 * s, h = 40 * s, gap = 8 * s;
    const float total = tool_count * w + (tool_count - 1) * gap;
    const float x0 = std::max(4.0F, (static_cast<float>(width) - total) / 2);
    return {x0 + static_cast<float>(index) * (w + gap), 10 * s, w, h};
}

Vector2 TouchLayoutEditor::centre(const input::TouchElementInfo& e, const input::TouchLayout& layout, int width, int height) {
    const auto& el = layout.*e.member;
    return {el.x * static_cast<float>(width), el.y * static_cast<float>(height)};
}

float TouchLayoutEditor::radius(const input::TouchElementInfo& e, const input::TouchLayout& layout, float button_size, int width, int height) {
    return (e.stick ? 92.0F : 43.0F) * input::touch_scale(width, height) * (layout.*e.member).scale * button_size;
}

TouchLayoutEditor::Result TouchLayoutEditor::update(GameSettings& settings, int width, int height, bool focused) {
    if (!open_) return Result::None;
    // One pointer: the first finger, else the mouse.
    const bool touching = GetTouchPointCount() > 0;
    const Vector2 p = touching ? GetTouchPosition(0) : GetMousePosition();
    const bool down = focused && (touching || IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    const bool pressed = down && !was_down_ && !warm_;
    was_down_ = down;
    warm_ = false;
    const auto& elements = input::touch_elements();
    const auto in_set = [&](const input::TouchElementInfo& e) { return driving_ ? e.driving : e.on_foot; };
    const auto resize = [&](float by) {
        if (selected_ < 0) return;
        const auto& e = elements[static_cast<std::size_t>(selected_)];
        auto c = custom_of(work_, e);
        c.scale = std::clamp(std::round((c.scale + by) * 10) / 10, input::custom_scale_min, input::custom_scale_max);
        work_[e.key] = c;
    };
    if (focused && IsKeyPressed(KEY_ESCAPE)) { open_ = false; return Result::Cancel; }
    if (focused && (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))) resize(.1F);
    if (focused && (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))) resize(-.1F);
    if (pressed) {
        for (int t = 0; t < tool_count; ++t) {
            if (!CheckCollisionPointRec(p, tool(t, width, height))) continue;
            switch (t) {
                case 0: driving_ = !driving_; selected_ = -1; break;
                case 1: resize(-.1F); break;
                case 2: resize(.1F); break;
                case 3: if (selected_ >= 0) work_.erase(elements[static_cast<std::size_t>(selected_)].key); break;
                case 4: work_.clear(); break;
                case 5: open_ = false; return Result::Cancel;
                default:
                    settings.touch_layout = work_;
                    open_ = false;
                    return Result::Done;
            }
            return Result::None;
        }
        // Grab the nearest button under the pointer.
        const auto layout = input::customised(work_);
        float best = 1e9F;
        dragging_ = -1;
        for (std::size_t i = 0; i < elements.size(); ++i) {
            if (!in_set(elements[i])) continue;
            const Vector2 c = centre(elements[i], layout, width, height);
            const float d = std::hypot(p.x - c.x, p.y - c.y);
            if (d < radius(elements[i], layout, settings.button_size, width, height) * 1.1F && d < best) {
                best = d;
                dragging_ = static_cast<int>(i);
                grab_ = {c.x - p.x, c.y - p.y};
            }
        }
        if (dragging_ >= 0) selected_ = dragging_;
    }
    if (down && dragging_ >= 0) {
        const auto& e = elements[static_cast<std::size_t>(dragging_)];
        auto c = custom_of(work_, e);
        c.x = std::clamp((p.x + grab_.x) / std::max(1.0F, static_cast<float>(width)), .03F, .97F);
        c.y = std::clamp((p.y + grab_.y) / std::max(1.0F, static_cast<float>(height)), .1F, .97F);
        work_[e.key] = c;
    }
    if (!down) dragging_ = -1;
    return Result::None;
}

void TouchLayoutEditor::draw(const GameSettings& settings, int width, int height, const Font* font) const {
    if (!open_) return;
    DrawRectangle(0, 0, width, height, Color{10, 14, 12, 215});
    const auto layout = input::customised(work_);
    const auto& elements = input::touch_elements();
    const float s = std::clamp(std::min(width / 1280.0F, height / 720.0F), .5F, 1.6F);
    for (std::size_t i = 0; i < elements.size(); ++i) {
        const auto& e = elements[i];
        if (driving_ ? !e.driving : !e.on_foot) continue;
        const Vector2 c = centre(e, layout, width, height);
        const float r = radius(e, layout, settings.button_size, width, height);
        const bool chosen = static_cast<int>(i) == selected_;
        DrawCircleV(c, r, chosen ? Color{230, 170, 60, 150} : Color{90, 110, 90, 120});
        DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), r, chosen ? Color{255, 210, 120, 255} : Color{220, 230, 210, 200});
        label(font, e.label, c, std::max(10.0F, 15 * s * std::min(1.4F, r / (43 * s))), RAYWHITE);
    }
    const char* tools[]{driving_ ? "AT THE WHEEL" : "ON FOOT", "SMALLER -", "BIGGER +", "RESET", "RESET ALL", "CANCEL", "DONE"};
    for (int t = 0; t < tool_count; ++t) {
        const auto r = tool(t, width, height);
        const bool on = t == tool_count - 1 || (t == 0);
        DrawRectangleRounded(r, .25F, 6, on ? Color{185, 202, 139, 255} : Color{46, 58, 48, 255});
        label(font, tools[t], {r.x + r.width / 2, r.y + r.height / 2}, 15 * s, on ? Color{20, 28, 20, 255} : RAYWHITE);
    }
    const char* hint = "Drag a button to move it; pick one and press - / + to size it.";
    label(font, hint, {static_cast<float>(width) / 2, 64 * s}, 15 * s, Color{200, 210, 190, 255});
    if (selected_ >= 0) {
        const auto& e = elements[static_cast<std::size_t>(selected_)];
        const auto found = work_.find(e.key);
        const float scale = found == work_.end() ? 1.0F : found->second.scale;
        label(font, TextFormat("%s  ·  %d%%", e.label, static_cast<int>(std::round(scale * 100))), {static_cast<float>(width) / 2, 86 * s}, 17 * s,
              Color{255, 214, 120, 255});
    }
}

} // namespace outland::game
