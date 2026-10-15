#include "outland/game/ZoneWall.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <tuple>

namespace outland::game {

int zone_wall_segments(float radius) {
    return std::clamp(static_cast<int>(2 * PI * std::max(1.0F, radius) / 12), 64, 1440);
}

namespace {
unsigned char byte(float v) { return static_cast<unsigned char>(std::clamp(v, 0.0F, 255.0F)); }
Color mix(Color a, Color b, float t) {
    return {byte(a.r + (b.r - a.r) * t), byte(a.g + (b.g - a.g) * t), byte(a.b + (b.b - a.b) * t), 255};
}
}

void draw_zone_wall(Vector2 center, float radius, Vector3 camera, float t, float menace, bool moving) {
    if (!std::isfinite(radius) || radius <= 1 || !std::isfinite(center.x) || !std::isfinite(center.y)) return;
    const int segments = zone_wall_segments(radius);
    constexpr float reach = 1100, height = 320;
    menace = std::clamp(menace, 0.0F, 1.0F);
    // PUBG blue early, a deeper violet as the end nears; the crest glows paler.
    const Color body = mix({55, 125, 255, 255}, {120, 70, 235, 255}, menace);
    const Color crest = mix({170, 215, 255, 255}, {215, 170, 255, 255}, menace);
    const float drift = moving ? 3.2F : 1.3F; // the streaks race up a moving wall
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ALPHA);
    rlBegin(RL_QUADS);
    const auto vertex = [&](float angle, float y_off, float alpha, Color c) {
        const float x = center.x + std::cos(angle) * radius, z = center.y + std::sin(angle) * radius;
        const float ground = world::terrain::TerrainHeight::sample(x, z);
        // Streaks drift along the wall and up it; slow pulses travel round it.
        const float streak = .62F + .38F * std::sin(angle * radius * .18F + t * 1.7F) * std::sin(y_off * .045F - t * drift);
        const float pulse = .85F + .15F * std::sin(angle * 9 - t * 1.1F);
        rlColor4ub(c.r, c.g, c.b, byte(alpha * streak * pulse));
        rlVertex3f(x, std::max(ground, -40.0F) + y_off, z);
    };
    for (int i = 0; i < segments; ++i) {
        const float a0 = 2 * PI * i / segments, a1 = 2 * PI * (i + 1) / segments, mid = (a0 + a1) * .5F;
        const float near = std::hypot(center.x + std::cos(mid) * radius - camera.x, center.y + std::sin(mid) * radius - camera.z);
        if (near > reach) continue;
        // The curtain: strong at the foot, thinning upward, then a pale crest band.
        for (const auto& [y0, y1, alpha0, alpha1] : {std::tuple{-4.0F, 24.0F, 230.0F, 175.0F}, std::tuple{24.0F, 150.0F, 175.0F, 70.0F},
                                                    std::tuple{150.0F, height - 12, 70.0F, 30.0F}}) {
            vertex(a0, y0, alpha0, body); vertex(a1, y0, alpha0, body); vertex(a1, y1, alpha1, body); vertex(a0, y1, alpha1, body);
        }
        vertex(a0, height - 12, 40, crest); vertex(a1, height - 12, 40, crest); vertex(a1, height, 0, crest); vertex(a0, height, 0, crest);
        // A bright band where it meets the ground, and a seething line a little above it.
        vertex(a0, -1, 240, crest); vertex(a1, -1, 240, crest); vertex(a1, 2.4F, 205, body); vertex(a0, 2.4F, 205, body);
        const float seethe = 7 + 1.5F * std::sin(mid * 30 + t * 2.4F);
        vertex(a0, seethe, 150, crest); vertex(a1, seethe, 150, crest); vertex(a1, seethe + .8F, 0, crest); vertex(a0, seethe + .8F, 0, crest);
    }
    rlEnd();
    rlDrawRenderBatchActive();
    EndBlendMode();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

void draw_next_circle(Vector2 center, float radius, Vector3 camera, float t) {
    if (!std::isfinite(radius) || radius <= 1) return;
    const int segments = zone_wall_segments(radius);
    constexpr float reach = 700;
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ALPHA);
    rlBegin(RL_QUADS);
    const float glow = .75F + .25F * std::sin(t * 2.5F);
    for (int i = 0; i < segments; ++i) {
        const float a0 = 2 * PI * i / segments, a1 = 2 * PI * (i + 1) / segments, mid = (a0 + a1) * .5F;
        if (std::hypot(center.x + std::cos(mid) * radius - camera.x, center.y + std::sin(mid) * radius - camera.z) > reach) continue;
        // Dashes, as a map line would be.
        if (i % 3 == 2) continue;
        const auto at = [&](float angle, float y, unsigned char alpha) {
            const float x = center.x + std::cos(angle) * radius, z = center.y + std::sin(angle) * radius;
            rlColor4ub(255, 255, 255, alpha);
            rlVertex3f(x, world::terrain::TerrainHeight::sample(x, z) + y, z);
        };
        const auto alpha = byte(210 * glow);
        at(a0, .1F, alpha); at(a1, .1F, alpha); at(a1, 3.0F, 0); at(a0, 3.0F, 0);
    }
    rlEnd();
    rlDrawRenderBatchActive();
    EndBlendMode();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

} // namespace outland::game
