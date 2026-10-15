#include "outland/game/aircraft/DropRenderer.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>

namespace outland::game::aircraft {
namespace {
float yaw_of(Vector3 direction) { return std::atan2(direction.x, direction.z); }

void blade(float length, float chord, Color colour) {
    // A flat blade from the hub along +X, a little twisted for the eye.
    DrawCube({length * .5F, 0, 0}, length, .05F, chord, colour);
}
} // namespace

void draw_helicopters(const std::vector<FlightPath>& fleet, float seconds, const Model* helicopter, Vector3 camera) {
    for (const auto& path : fleet) {
        const int index = path.index;
        const float yaw = yaw_of(path.direction) * RAD2DEG;
        const Vector3 at = helicopter_position(path, seconds);
        if (Vector3Distance(at, camera) > 3200) continue;
        const float roll = 2.5F * std::sin(seconds * .7F + static_cast<float>(index) * 1.3F);
        rlPushMatrix();
        rlTranslatef(at.x, at.y, at.z);
        rlRotatef(yaw, 0, 1, 0);
        rlRotatef(7, 1, 0, 0);          // nose down in forward flight
        rlRotatef(roll, 0, 0, 1);
        // The model's skids sit on y = 0; it flies its hub at `at`: lift it by the hub.
        Vector3 hub = rotor_hub, tail = tail_hub;
        if (helicopter) {
            hub = Vector3Transform(rotor_hub, helicopter->transform);
            tail = Vector3Transform(tail_hub, helicopter->transform);
            rlTranslatef(0, -2.2F, 0);
            DrawModel(*helicopter, {0, 0, 0}, 1, WHITE);
        } else {
            rlTranslatef(0, -2.2F, 0);
            DrawCube({0, 1.7F, 0}, 2.6F, 1.9F, 4.4F, Color{74, 88, 58, 255});
            DrawCube({0, 2.2F, -4.8F}, .5F, .5F, 5.4F, Color{74, 88, 58, 255});
        }
        // Main rotor: four blades turning fast, with a faint disc of blur.
        const float spin = std::fmod(seconds * 1500.0F + static_cast<float>(index) * 37, 360.0F);
        rlPushMatrix();
        rlTranslatef(hub.x, hub.y, hub.z);
        DrawCylinder({0, .02F, 0}, rotor_radius, rotor_radius, .01F, 36, Fade(Color{40, 40, 40, 255}, .16F));
        for (int b = 0; b < 4; ++b) {
            rlPushMatrix();
            rlRotatef(spin + b * 90.0F, 0, 1, 0);
            blade(rotor_radius, .32F, Color{36, 38, 36, 255});
            rlPopMatrix();
        }
        DrawCylinder({0, -.05F, 0}, .22F, .22F, .2F, 10, Color{50, 52, 50, 255});
        rlPopMatrix();
        // Tail rotor: across the boom, spinning about X.
        rlPushMatrix();
        rlTranslatef(tail.x + .05F, tail.y, tail.z);
        rlRotatef(90, 0, 0, 1);
        DrawCylinder({0, 0, 0}, tail_rotor_radius, tail_rotor_radius, .01F, 20, Fade(Color{40, 40, 40, 255}, .18F));
        for (int b = 0; b < 2; ++b) {
            rlPushMatrix();
            rlRotatef(spin * 1.6F + b * 180.0F, 0, 1, 0);
            blade(tail_rotor_radius, .14F, Color{36, 38, 36, 255});
            rlPopMatrix();
        }
        rlPopMatrix();
        rlPopMatrix();
    }
}

void draw_parachute(const Model* chute, Vector3 harness, float heading, float open, float swing, float bank) {
    open = std::clamp(open, 0.0F, 1.0F);
    if (open <= 0) return;
    // Bursting out of the pack: a narrow streamer that spreads and fills.
    const float fill = open * open * (3 - 2 * open);
    rlPushMatrix();
    rlTranslatef(harness.x, harness.y, harness.z);
    rlRotatef(heading * RAD2DEG, 0, 1, 0);
    rlRotatef(-swing * RAD2DEG, 1, 0, 0);
    rlRotatef(-bank * RAD2DEG, 0, 0, 1);
    rlScalef(.12F + .88F * fill, .35F + .65F * fill, .4F + .6F * fill);
    if (chute) {
        DrawModel(*chute, {0, 0, 0}, 1, WHITE);
    } else {
        DrawCube({0, 6.8F, 0}, 8.4F, .35F, 3.0F, Color{0, 158, 72, 255});
        DrawLine3D({-4, 6.6F, 0}, {0, 0, 0}, LIGHTGRAY);
        DrawLine3D({4, 6.6F, 0}, {0, 0, 0}, LIGHTGRAY);
    }
    rlPopMatrix();
}

void draw_bot_in_air(const BotInAir& bot, const Model* chute, Color clothes) {
    if (bot.stage != DiveStage::Freefall && bot.stage != DiveStage::Canopy) return;
    const Vector3 p = bot.position;
    const Color skin{196, 152, 120, 255}, legs{50, 56, 66, 255};
    rlPushMatrix();
    rlTranslatef(p.x, p.y, p.z);
    rlRotatef(bot.yaw * RAD2DEG, 0, 1, 0);
    if (bot.stage == DiveStage::Freefall) {
        // Belly-down: body level, arms and legs spread.
        rlRotatef(80, 1, 0, 0);
        DrawCapsule({0, -.35F, 0}, {0, .35F, 0}, .2F, 6, 4, clothes);
        DrawSphere({0, .7F, 0}, .13F, skin);
        DrawCapsule({-.15F, .3F, 0}, {-.7F, .6F, 0}, .06F, 4, 2, clothes);
        DrawCapsule({.15F, .3F, 0}, {.7F, .6F, 0}, .06F, 4, 2, clothes);
        DrawCapsule({-.1F, -.45F, 0}, {-.35F, -1.0F, -.2F}, .08F, 4, 2, legs);
        DrawCapsule({.1F, -.45F, 0}, {.35F, -1.0F, -.2F}, .08F, 4, 2, legs);
    } else {
        DrawCapsule({0, -.1F, 0}, {0, .55F, 0}, .2F, 6, 4, clothes);
        DrawSphere({0, .9F, 0}, .13F, skin);
        DrawCapsule({-.18F, .5F, 0}, {-.3F, 1.2F, 0}, .06F, 4, 2, clothes);
        DrawCapsule({.18F, .5F, 0}, {.3F, 1.2F, 0}, .06F, 4, 2, clothes);
        DrawCapsule({-.1F, -.2F, .1F}, {-.1F, -.95F, .2F}, .08F, 4, 2, legs);
        DrawCapsule({.1F, -.2F, .1F}, {.1F, -.95F, .2F}, .08F, 4, 2, legs);
    }
    rlPopMatrix();
    if (bot.stage == DiveStage::Canopy)
        draw_parachute(chute, Vector3Add(p, {0, .55F, 0}), bot.yaw, bot.canopy, 0, 0);
}

} // namespace outland::game::aircraft
