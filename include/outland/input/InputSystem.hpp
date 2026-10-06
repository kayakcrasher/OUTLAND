#pragma once

#include "outland/input/PlayerInput.hpp"
#include "outland/input/TouchLayout.hpp"

namespace outland::input {

class InputSystem {
public:
    InputSystem();

    void update(
        int screen_width,
        int screen_height
    );

    [[nodiscard]]
    const PlayerInput& player() const;

    [[nodiscard]]
    PlayerInput& player();

    [[nodiscard]]
    const TouchLayout& layout() const;

    [[nodiscard]]
    TouchLayout& layout();

    void reset_layout();

private:
    PlayerInput player_{};
    TouchLayout layout_{};

    bool previous_jump_{false};
    bool previous_view_{false};

    int movement_touch_id_{-1};
    int look_touch_id_{-1};

    // X11 development input.
    enum class MouseControl {
        None,
        Movement,
        Look,
        Jump,
        View,
        Fire,
        Aim
    };

    MouseControl mouse_control_{
        MouseControl::None
    };
};

}
