#pragma once

namespace outland::input {

struct PlayerInput {
    // Movement: -1.0 to +1.0
    float move_x{0.0F};
    float move_y{0.0F};

    // Camera/look: -1.0 to +1.0
    float look_x{0.0F};
    float look_y{0.0F};

    // Movement actions
    bool jump{false};
    bool sprint{false};
    bool crouch{false};

    // Combat actions
    bool fire{false};
    bool aim{false};
    bool reload{false};

    // World interaction
    bool interact{false};

    // Camera
    bool toggle_view{false};

    void clear_frame_actions() {
        jump = false;
        reload = false;
        interact = false;
        toggle_view = false;
    }
};

}
