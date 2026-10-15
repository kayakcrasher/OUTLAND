#pragma once

#include "outland/input/PlayerInput.hpp"
#include "outland/input/TouchLayout.hpp"

namespace outland::input {

class TouchHUD {
public:
    void draw(
        const PlayerInput& input,
        const TouchLayout& layout,
        int screen_width,
        int screen_height,
        bool gameplay = true,
        const char* fire_mode = nullptr,  // "AUTO" / "SINGLE" for automatic weapons, else hidden
        const char* sight = nullptr,      // the held gun's optic ("IRON", "DOT", "2X", "4X"), else hidden
        bool driving = false,             // at the wheel: steering stick, pedals and the pan area
        bool look_stick = false,          // the Creator's look stick (play and building swipe instead)
        bool skydiving = false            // the Battle Royale drop: the stick and JUMP, and a label on it
    ) const;
};

}
