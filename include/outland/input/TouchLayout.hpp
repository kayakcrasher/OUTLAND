#pragma once

#include <algorithm>

namespace outland::input {

// Shared by hit testing, HUD and modal panels; keep geometry identical at every size.
inline float touch_scale(int width, int height) {
    return std::clamp(std::min(width / 1280.0F, height / 720.0F), 0.35F, 1.35F);
}

struct TouchElementLayout {
    float x{0.0F};
    float y{0.0F};
    float scale{1.0F};
    float opacity{0.50F};
};

struct TouchLayout {

    // ========================================================
    // OUTLAND MOBILE CONTROLS V2
    // ========================================================
    //
    // PUBG-inspired ergonomics, OUTLAND layout.
    //
    // LEFT THUMB
    //   movement
    //
    // RIGHT THUMB
    //   free look
    //   fire
    //   aim
    //   jump / vault
    //   reload
    //
    // Upper controls are kept away from the combat cluster.
    // ========================================================


    // --------------------------------------------------------
    // MOVEMENT
    // --------------------------------------------------------

    TouchElementLayout movement{
        0.16F,
        0.76F,
        1.30F,
        0.34F
    };


    // --------------------------------------------------------
    // CAMERA / FREE LOOK
    // --------------------------------------------------------
    //
    // Keep the existing look control alive for now.
    // The HUD can later render this almost invisibly while
    // InputSystem expands the right-side drag area.

    TouchElementLayout look{
        0.66F,
        0.72F,
        1.15F,
        0.12F
    };


    // --------------------------------------------------------
    // PRIMARY FIRE
    // --------------------------------------------------------
    //
    // Large combat button at the upper-right edge of the
    // right thumb's natural sweep.

    TouchElementLayout fire{
        0.91F,
        0.35F,
        1.28F,
        0.52F
    };


    // --------------------------------------------------------
    // AIM / ADS
    // --------------------------------------------------------

    TouchElementLayout aim{
        0.79F,
        0.43F,
        0.92F,
        0.42F
    };


    // --------------------------------------------------------
    // JUMP / VAULT
    // --------------------------------------------------------
    //
    // One contextual control:
    //
    // normal terrain -> JUMP
    // valid window   -> VAULT
    //
    // Renderer/WorldCollision decide which action occurs.

    TouchElementLayout jump{
        0.90F,
        0.60F,
        1.05F,
        0.48F
    };


    // --------------------------------------------------------
    // RELOAD
    // --------------------------------------------------------

    TouchElementLayout reload{
        0.78F,
        0.59F,
        0.82F,
        0.40F
    };


    // --------------------------------------------------------
    // WEAPON SWITCH
    // --------------------------------------------------------
    //
    // Higher than the main combat cluster so it is harder
    // to accidentally switch weapons during a firefight.

    TouchElementLayout weapon{
        0.82F,
        0.23F,
        0.82F,
        0.38F
    };


    // --------------------------------------------------------
    // CAMERA VIEW
    // --------------------------------------------------------

    TouchElementLayout view{
        0.69F,
        0.22F,
        0.76F,
        0.32F
    };


    // --------------------------------------------------------
    // CAMERA FEEL
    // --------------------------------------------------------

    TouchElementLayout sprint{0.08F, 0.43F, 0.82F, 0.40F};
    TouchElementLayout crouch{0.91F, 0.81F, 0.82F, 0.40F};
    TouchElementLayout inventory{0.52F, 0.22F, 0.82F, 0.40F};
    TouchElementLayout interact{0.49F, 0.40F, 0.82F, 0.40F};

    float look_sensitivity{2.25F};


    void reset() {
        *this = TouchLayout{};
    }
};

} // namespace outland::input
