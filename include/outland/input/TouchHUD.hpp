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
        int screen_height
    ) const;
};

}
