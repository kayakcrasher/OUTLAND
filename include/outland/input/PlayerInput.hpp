#pragma once

namespace outland::input {

struct PlayerInput {
    // Movement: -1.0 to +1.0
    float move_x{0.0F};
    float move_y{0.0F};

    // Camera/look: -1.0 to +1.0 (a stick held over: turn at a rate; the driving pan and Creator)
    float look_x{0.0F};
    float look_y{0.0F};
    // Camera/look: radians this frame from the mouse or a swipe, sensitivity and invert applied.
    float look_dx{0.0F};
    float look_dy{0.0F};

    // Movement actions
    bool jump{false};
    bool brake{false}; // Held Jump/Space, used only by vehicle controls.
    // At the wheel on a touch screen: the right thumb's pedals (0..1). Keys keep W/S on move_y.
    float gas{0.0F}, brake_pedal{0.0F};
    bool sprint{false};
    bool crouch{false};

    // Combat actions
    bool fire{false};
    bool aim{false};
    bool reload{false};
    bool grenade{false};     // throw a frag grenade
    bool heal{false};        // quick heal: the best medical item in the bag
    bool lights{false};      // a police car's emergency lights and siren, on / off
    bool tag{false};         // spray the gang's tag on the wall in front
    bool backup{false};      // an officer radios for backup
    bool radio{false};       // at the wheel: the next radio station (or off)
    bool hitch{false};       // at the wheel of a big rig: drop the trailer, or hitch it again
    bool sight{false};       // change the held gun's optic (iron, red dot, 2x, 4x)
    bool next_weapon{false};
    bool fire_mode{false};   // switch automatic / single shots

    // World interaction
    bool interact{false};
    bool inventory{false};

    // Camera
    bool toggle_view{false};

    void clear_frame_actions() {
        jump = false;
        reload = false;
        next_weapon = false;
        fire_mode = false;
        sight = false;
        interact = false;
        inventory = false;
        toggle_view = false;
        heal = false;
        lights = false;
        tag = false;
        backup = false;
        radio = false;
        hitch = false;
    }
};

}
