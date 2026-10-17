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
    // At the wheel with a pistol or SMG: a FIRE button for drive-bys.
    void set_drive_by(bool on) { drive_by_ = on; }
    // At the wheel of a police car: the LIGHTS button (lit while they run).
    void set_police_car(bool car, bool lights_on) { police_car_ = car; lights_on_ = lights_on; }
    // A gang member on foot: the TAG button.
    void set_tagger(bool on) { tagger_ = on; }
    // USE appears only when something is in reach to use.
    void set_usable(bool on) { usable_ = on; }
    // NADE and HEAL show only with a grenade to throw / something to heal with.
    void set_kit(bool grenade, bool heal) { grenade_ = grenade; heal_ = heal; }
    // MORE folded open: VIEW, the sight and the fire mode show (InputSystem::more_open).
    void set_more(bool open) { more_ = open; }

private:
    bool drive_by_{false}, police_car_{false}, lights_on_{false}, tagger_{false}, usable_{false}, grenade_{true}, heal_{true}, more_{false};
};

}
