#include "outland/engine/render/SignalRender.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <cmath>

namespace outland::engine::render {
namespace {
using world::roads::Aspect;
// The lamps, top to bottom (metres up the pole).
constexpr float lamp_red = 5.22F, lamp_amber = 4.95F, lamp_green = 4.68F;

void head(Vector3 at, Aspect aspect, bool extra, bool shine) {
    // `at`: the head's face, in the head's own frame (it looks along +z); the matrix is set by the caller.
    if (extra && !shine) DrawCube({0, (lamp_red + lamp_green) * .5F, -.14F}, .34F, 1.0F, .26F, Color{28, 30, 32, 255});
    const struct { float y; Aspect lit; Color on, off; } lamps[] = {
        {lamp_red, Aspect::Red, {255, 40, 30, 255}, {70, 18, 16, 255}},
        {lamp_amber, Aspect::Amber, {255, 170, 20, 255}, {70, 50, 14, 255}},
        {lamp_green, Aspect::Green, {40, 255, 110, 255}, {14, 60, 30, 255}},
    };
    for (const auto& lamp : lamps) {
        const bool on = lamp.lit == aspect;
        if (shine && !on) continue;
        DrawCylinderEx({at.x, lamp.y, at.z}, {at.x, lamp.y, at.z + .03F}, .115F, .115F, 12, on ? lamp.on : lamp.off);
        if (on) DrawSphere({at.x, lamp.y, at.z + .06F}, .2F, Fade(lamp.on, .35F));
    }
}
}

void draw_signals(const std::vector<world::roads::Signal>& signals, double time, Vector3 camera, bool shine) {
    for (const auto& s : signals) {
        if (Vector3Distance(s.centre, camera) > 260) continue;
        const Aspect along_x = world::roads::aspect(s, false, time), along_z = world::roads::aspect(s, true, time);
        for (const auto& [pole, yaw] : s.poles) {
            rlPushMatrix();
            rlTranslatef(pole.x, pole.y, pole.z);
            rlRotatef(yaw, 0, 1, 0);
            // The model's head faces its +x: turn so the head's +z is the model's +x.
            rlPushMatrix();
            rlRotatef(90, 0, 1, 0);
            head({0, 0, .56F}, along_x, false, shine);
            rlPopMatrix();
            // The second head, on the pole, facing the model's +z (the cross road's traffic).
            rlPushMatrix();
            rlTranslatef(-.135F, 0, .3F);
            head({0, 0, .02F}, along_z, true, shine);
            rlPopMatrix();
            rlPopMatrix();
        }
    }
}

} // namespace outland::engine::render
