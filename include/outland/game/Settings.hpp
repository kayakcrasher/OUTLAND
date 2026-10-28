#pragma once
#include "outland/input/Bindings.hpp"
#include "outland/input/TouchCustom.hpp"
#include <string>

namespace outland::game {

// Player settings, kept beside the saves (profiles/settings.cfg) and changed from the pause
// menu (SETTINGS and CONTROLS) or the map's side panel.
struct GameSettings {
    enum class Graphics : int { Auto, Fast, Sharp };
    // Touch buttons and swipe-to-look: AUTO shows them on phones and tablets, and once a touch
    // screen is used; OFF plays with keyboard and mouse.
    enum class TouchControls : int { Auto, On, Off };

    float look_sensitivity{1.0F};   // touch: swipe across half the screen's height turns 90 degrees at 1.0
    float mouse_sensitivity{1.0F};  // mouse: .0012 radians per pixel at 1.0
    float aim_sensitivity{.8F};     // extra factor while aiming down sights (on top of the zoom)
    bool invert_y{false};
    bool aim_toggle{false};         // mouse: a click on AIM toggles the sights instead of holding
    bool aim_assist{true};          // touch only: the look slows and drifts onto enemies
    bool vibration{true};           // phones: shots, hits, crashes, blasts
    bool footsteps{true};           // your own footsteps
    TouchControls touch_controls{TouchControls::Auto};
    float button_size{1.0F};        // touch buttons
    float volume{.8F};
    bool muted{false};
    float radio_volume{.7F};        // the car radio, on top of the volume
    int radio_station{0};           // the car radio's station (-1 off), kept for the next drive
    Graphics graphics{Graphics::Auto};
    input::TouchOverrides touch_layout;   // buttons the player moved or resized (SETTINGS > Touch layout)
    input::Bindings bindings{input::Bindings::defaults()};
    bool mouse_look{false};         // at run time: the player looks with the mouse (not saved)

    static constexpr float min_sensitivity = .2F, max_sensitivity = 3.0F;
    static constexpr float min_mouse = .1F, max_mouse = 4.0F;
    static constexpr float min_aim = .3F, max_aim = 1.5F;
    static constexpr float min_buttons = .7F, max_buttons = 1.4F;
    static std::string path_for(const std::string& map_path);
    bool load(const std::string& path);
    bool save(const std::string& path) const;
    // Everything but the volume, the radio and graphics back to the defaults.
    void reset_controls();
};

} // namespace outland::game
