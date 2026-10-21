#include "outland/game/Settings.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace outland::game {

std::string GameSettings::path_for(const std::string& map_path) {
    return (std::filesystem::path(map_path).parent_path().parent_path() / "profiles" / "settings.cfg").string();
}

bool GameSettings::load(const std::string& path) {
    std::ifstream input(path);
    if (!input) return false;
    GameSettings read = *this;
    std::string line;
    const auto flag = [](std::istringstream& in, bool& out) { int v = 0; if (in >> v) out = v != 0; };
    while (std::getline(input, line)) {
        std::istringstream in(line);
        std::string key;
        if (!(in >> key)) continue;
        // "look_sensitivity" was the old look stick's speed; swipes and the mouse start afresh.
        if (key == "touch_sensitivity") in >> read.look_sensitivity;
        else if (key == "mouse_sensitivity") in >> read.mouse_sensitivity;
        else if (key == "aim_sensitivity") in >> read.aim_sensitivity;
        else if (key == "button_size") in >> read.button_size;
        else if (key == "volume") in >> read.volume;
        else if (key == "muted") flag(in, read.muted);
        else if (key == "invert_y") flag(in, read.invert_y);
        else if (key == "aim_toggle") flag(in, read.aim_toggle);
        else if (key == "aim_assist") flag(in, read.aim_assist);
        else if (key == "vibration") flag(in, read.vibration);
        else if (key == "graphics") { int v = 0; in >> v; read.graphics = static_cast<Graphics>(std::clamp(v, 0, 2)); }
        else if (key == "language") { int v = 0; in >> v; read.language = static_cast<Language>(std::clamp(v, 0, 1)); }
        else if (key == "touch_controls") { int v = 0; in >> v; read.touch_controls = static_cast<TouchControls>(std::clamp(v, 0, 2)); }
        else if (key == "touch") {
            std::string name; input::TouchCustom c;
            if (in >> name >> c.x >> c.y >> c.scale && std::isfinite(c.x) && std::isfinite(c.y) && std::isfinite(c.scale))
                read.touch_layout[name] = c;
        }
        else if (key == "bind") {
            std::string name; int a = 0, b = 0;
            if (!(in >> name >> a >> b)) continue;
            for (std::size_t i = 0; i < input::action_count; ++i)
                if (name == input::Bindings::key(static_cast<input::Action>(i))) read.bindings.codes[i] = {a, b};
        }
    }
    for (float* v : {&read.look_sensitivity, &read.mouse_sensitivity, &read.aim_sensitivity, &read.button_size, &read.volume})
        if (!std::isfinite(*v)) return false;
    read.look_sensitivity = std::clamp(read.look_sensitivity, min_sensitivity, max_sensitivity);
    read.mouse_sensitivity = std::clamp(read.mouse_sensitivity, min_mouse, max_mouse);
    read.aim_sensitivity = std::clamp(read.aim_sensitivity, min_aim, max_aim);
    read.button_size = std::clamp(read.button_size, min_buttons, max_buttons);
    read.volume = std::clamp(read.volume, 0.0F, 1.0F);
    read.mouse_look = mouse_look;
    *this = read;
    return true;
}

bool GameSettings::save(const std::string& path) const {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream output(path, std::ios::trunc);
    output << "touch_sensitivity " << look_sensitivity << "\nmouse_sensitivity " << mouse_sensitivity
           << "\naim_sensitivity " << aim_sensitivity << "\ninvert_y " << invert_y << "\naim_toggle " << aim_toggle
           << "\naim_assist " << aim_assist << "\nvibration " << vibration << "\ntouch_controls " << static_cast<int>(touch_controls)
           << "\nbutton_size " << button_size << "\nvolume " << volume << "\nmuted " << (muted ? 1 : 0)
           << "\ngraphics " << static_cast<int>(graphics) << "\nlanguage " << static_cast<int>(language) << '\n';
    for (std::size_t i = 0; i < input::action_count; ++i)
        output << "bind " << input::Bindings::key(static_cast<input::Action>(i)) << ' ' << bindings.codes[i][0] << ' '
               << bindings.codes[i][1] << '\n';
    for (const auto& [name, c] : touch_layout) output << "touch " << name << ' ' << c.x << ' ' << c.y << ' ' << c.scale << '\n';
    return static_cast<bool>(output);
}

void GameSettings::reset_controls() {
    GameSettings fresh;
    fresh.volume = volume;
    fresh.muted = muted;
    fresh.graphics = graphics;
    fresh.language = language;
    fresh.mouse_look = mouse_look;
    *this = fresh;
}

} // namespace outland::game
