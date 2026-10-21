#include "outland/input/Bindings.hpp"
#include <raylib.h>
#include <cmath>

namespace outland::input {
namespace {
struct Info { const char* label; const char* key; int a, b; };
// The defaults follow PC shooters (PUBG, Fortnite, Call of Duty) where OUTLAND has the action.
constexpr std::array<Info, action_count> info{{
    {"MOVE FORWARD", "forward", KEY_W, KEY_UP},
    {"MOVE BACK", "back", KEY_S, KEY_DOWN},
    {"MOVE LEFT", "left", KEY_A, KEY_LEFT},
    {"MOVE RIGHT", "right", KEY_D, KEY_RIGHT},
    {"JUMP / HANDBRAKE", "jump", KEY_SPACE, 0},
    {"SPRINT / BOOST", "sprint", KEY_LEFT_SHIFT, 0},
    {"CROUCH", "crouch", KEY_C, KEY_LEFT_CONTROL},
    {"FIRE", "fire", Bindings::mouse(MOUSE_BUTTON_LEFT), 0},
    {"AIM DOWN SIGHTS", "aim", Bindings::mouse(MOUSE_BUTTON_RIGHT), 0},
    {"RELOAD", "reload", KEY_R, 0},
    {"USE / ENTER VEHICLE", "interact", KEY_E, KEY_F},
    {"THROW GRENADE", "grenade", KEY_G, 0},
    {"SWITCH WEAPON", "weapon", KEY_Q, Bindings::wheel},
    {"FIRE MODE", "fire_mode", KEY_B, 0},
    {"CHANGE SIGHT", "sight", KEY_X, 0},
    {"BAG / INVENTORY", "inventory", KEY_TAB, KEY_I},
    {"CAMERA VIEW", "view", KEY_V, 0},
    {"MAP", "map", KEY_M, 0},
    {"BUILD", "build", KEY_K, 0},
    {"QUICK HEAL", "heal", KEY_H, 0},
    {"EMERGENCY LIGHTS", "lights", KEY_L, 0},
    {"SPRAY TAG", "tag", KEY_T, 0},
    {"RADIO BACKUP (POLICE)", "backup", KEY_Z, 0},
}};

bool code_down(int code, bool mouse) {
    if (code > 0) return IsKeyDown(code);
    if (code == Bindings::wheel) return false;
    if (Bindings::is_mouse(code)) return mouse && IsMouseButtonDown(-1 - code);
    return false;
}
bool code_pressed(int code, bool mouse) {
    if (code > 0) return IsKeyPressed(code);
    if (code == Bindings::wheel) return mouse && GetMouseWheelMove() != 0;
    if (Bindings::is_mouse(code)) return mouse && IsMouseButtonPressed(-1 - code);
    return false;
}
} // namespace

Bindings Bindings::defaults() {
    Bindings b;
    for (std::size_t i = 0; i < action_count; ++i) b.codes[i] = {info[i].a, info[i].b};
    return b;
}

bool Bindings::down(Action a, bool mouse) const {
    const auto& c = (*this)[a];
    return code_down(c[0], mouse) || code_down(c[1], mouse);
}

bool Bindings::pressed(Action a, bool mouse) const {
    const auto& c = (*this)[a];
    return code_pressed(c[0], mouse) || code_pressed(c[1], mouse);
}

void Bindings::bind(Action a, int slot, int code) {
    if (code != 0)
        for (auto& pair : codes)
            for (auto& c : pair)
                if (c == code) c = 0;
    (*this)[a][static_cast<std::size_t>(slot & 1)] = code;
}

int Bindings::captured() {
    for (int k = GetKeyPressed(); k != 0; k = GetKeyPressed())
        if (k != KEY_ESCAPE) return k;
    for (int b = MOUSE_BUTTON_LEFT; b <= MOUSE_BUTTON_EXTRA; ++b)
        if (IsMouseButtonPressed(b)) return mouse(b);
    if (GetMouseWheelMove() != 0) return wheel;
    return 0;
}

const char* Bindings::label(Action a) { return info[static_cast<std::size_t>(a)].label; }
const char* Bindings::key(Action a) { return info[static_cast<std::size_t>(a)].key; }

std::string Bindings::name(int code) {
    if (code == 0) return "-";
    if (code == wheel) return "MOUSE WHEEL";
    if (is_mouse(code)) {
        static const char* buttons[]{"LEFT MOUSE", "RIGHT MOUSE", "MIDDLE MOUSE", "MOUSE 4", "MOUSE 5", "MOUSE 6", "MOUSE 7"};
        return buttons[-1 - code];
    }
    if ((code >= KEY_A && code <= KEY_Z) || (code >= KEY_ZERO && code <= KEY_NINE)) return std::string(1, static_cast<char>(code));
    if (code >= KEY_F1 && code <= KEY_F12) return "F" + std::to_string(code - KEY_F1 + 1);
    if (code >= KEY_KP_0 && code <= KEY_KP_9) return "NUM " + std::to_string(code - KEY_KP_0);
    switch (code) {
    case KEY_SPACE: return "SPACE";
    case KEY_LEFT_SHIFT: return "LEFT SHIFT";
    case KEY_RIGHT_SHIFT: return "RIGHT SHIFT";
    case KEY_LEFT_CONTROL: return "LEFT CTRL";
    case KEY_RIGHT_CONTROL: return "RIGHT CTRL";
    case KEY_LEFT_ALT: return "LEFT ALT";
    case KEY_RIGHT_ALT: return "RIGHT ALT";
    case KEY_TAB: return "TAB";
    case KEY_ENTER: return "ENTER";
    case KEY_BACKSPACE: return "BACKSPACE";
    case KEY_CAPS_LOCK: return "CAPS LOCK";
    case KEY_UP: return "UP";
    case KEY_DOWN: return "DOWN";
    case KEY_LEFT: return "LEFT";
    case KEY_RIGHT: return "RIGHT";
    case KEY_GRAVE: return "`";
    case KEY_MINUS: return "-";
    case KEY_EQUAL: return "=";
    case KEY_LEFT_BRACKET: return "[";
    case KEY_RIGHT_BRACKET: return "]";
    case KEY_SEMICOLON: return ";";
    case KEY_APOSTROPHE: return "'";
    case KEY_COMMA: return ",";
    case KEY_PERIOD: return ".";
    case KEY_SLASH: return "/";
    case KEY_BACKSLASH: return "\\";
    case KEY_INSERT: return "INSERT";
    case KEY_DELETE: return "DELETE";
    case KEY_HOME: return "HOME";
    case KEY_END: return "END";
    case KEY_PAGE_UP: return "PAGE UP";
    case KEY_PAGE_DOWN: return "PAGE DOWN";
    default: return "KEY " + std::to_string(code);
    }
}

} // namespace outland::input
