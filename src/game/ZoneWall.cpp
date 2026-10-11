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

void draw_zone_wall(float radius, Vector3 camera, float t) {
    if (!std::isfinite(radius) || radius <= 1) return;
    const int segments = zone_wall_segments(radius);
    constexpr float reach = 950, height = 120;
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ALPHA);
    rlBegin(RL_QUADS);
    const auto vertex = [&](float angle, float y_off, float alpha) {
        const float x = std::cos(angle) * radius, z = std::sin(angle) * radius;
        const float ground = world::terrain::TerrainHeight::sample(x, z);
        // Streaks drift along the wall and up it.
        const float streak = .65F + .35F * std::sin(angle * radius * .18F + t * 1.7F) * std::sin(y_off * .05F - t * 1.3F);
        rlColor4ub(55, 125, 255, static_cast<unsigned char>(std::clamp(alpha * streak, 0.0F, 255.0F)));
        rlVertex3f(x, ground + y_off, z);
    };
    for (int i = 0; i < segments; ++i) {
        const float a0 = 2 * PI * i / segments, a1 = 2 * PI * (i + 1) / segments, mid = (a0 + a1) * .5F;
        if (std::hypot(std::cos(mid) * radius - camera.x, std::sin(mid) * radius - camera.z) > reach) continue;
        // The curtain: strong at the foot, gone at the top; then a bright band along the ground.
        for (const auto& [y0, y1, alpha0, alpha1] : {std::tuple{-4.0F, 18.0F, 225.0F, 165.0F}, std::tuple{18.0F, height, 165.0F, 25.0F}}) {
            vertex(a0, y0, alpha0); vertex(a1, y0, alpha0); vertex(a1, y1, alpha1); vertex(a0, y1, alpha1);
        }
        vertex(a0, -1, 235); vertex(a1, -1, 235); vertex(a1, 2.2F, 200); vertex(a0, 2.2F, 200);
    }
    rlEnd();
    rlDrawRenderBatchActive();
    EndBlendMode();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

} // namespace outland::game
