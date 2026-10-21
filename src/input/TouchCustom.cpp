#include "outland/input/TouchCustom.hpp"

namespace outland::input {

const std::vector<TouchElementInfo>& touch_elements() {
    static const std::vector<TouchElementInfo> list{
        {"movement", "MOVE", &TouchLayout::movement, true, true, true},
        {"fire", "FIRE", &TouchLayout::fire, true, false, false},
        {"fire_left", "FIRE (LEFT)", &TouchLayout::fire_left, true, true, false},
        {"aim", "AIM", &TouchLayout::aim, true, false, false},
        {"jump", "JUMP", &TouchLayout::jump, true, false, false},
        {"reload", "LOAD", &TouchLayout::reload, true, false, false},
        {"weapon", "GUN", &TouchLayout::weapon, true, false, false},
        {"view", "MORE / VIEW", &TouchLayout::view, true, false, false},
        {"fire_mode", "AUTO", &TouchLayout::fire_mode, true, false, false},
        {"sight", "SIGHT", &TouchLayout::sight, true, false, false},
        {"sprint", "SPRINT / BOOST", &TouchLayout::sprint, true, true, false},
        {"crouch", "CROUCH", &TouchLayout::crouch, true, false, false},
        {"grenade", "NADE", &TouchLayout::grenade, true, false, false},
        {"heal", "HEAL", &TouchLayout::heal, true, false, false},
        {"inventory", "BAG", &TouchLayout::inventory, true, false, false},
        {"interact", "USE / EXIT", &TouchLayout::interact, true, true, false},
        {"tag", "TAG / BACKUP", &TouchLayout::tag, true, false, false},
        {"gas", "GAS", &TouchLayout::gas, false, true, false},
        {"brake_pedal", "BRAKE", &TouchLayout::brake_pedal, false, true, false},
        {"handbrake", "HAND", &TouchLayout::handbrake, false, true, false},
        {"lights", "LIGHTS", &TouchLayout::lights, false, true, false},
    };
    return list;
}

} // namespace outland::input
