#pragma once
#include "outland/game/culture/Signage.hpp"
#include <raylib.h>
#include <string>
#include <vector>

namespace outland::game::culture {

// Draws Signage inside BeginMode3D. Needs a GL context: load() after the window opens.
// Boards and posts go out untextured, then all lettering in one batch with the font atlas, then
// the flags with their cloth texture, so a street full of signs costs a few draw calls.
class SignRenderer {
public:
    ~SignRenderer();
    // `application_dir` is searched first for assets/fonts/DejaVuSans-Bold.ttf, then the working
    // directory. Without the font the signs draw with raylib's default font (no ĉ ĝ ĥ ĵ ŝ ŭ).
    void load(const std::string& application_dir);
    void unload();
    void set(Signage signage);
    const Signage& signage() const { return signage_; }
    const Font& font() const { return font_; } // also for HUD text with Esperanto letters
    void draw(Vector3 camera, Vector3 camera_target, float time_seconds) const;

    static constexpr float sign_range = 120;   // boards drawn within this
    static constexpr float text_range = 75;    // lettering drawn within this
    static constexpr float flag_range = 500;

private:
    struct Measured { float width{0}, height{0}; std::vector<float> scale; std::vector<float> back_scale; };
    float measure(const std::string& text, float height) const;
    void layout();
    void text_line(const std::string& text, float height, float scale, float y_top, Color ink) const;

    Signage signage_;
    std::vector<Measured> measured_;
    Font font_{};
    bool own_font_{false};
    Texture2D flag_{};
    bool loaded_{false};
};
}
