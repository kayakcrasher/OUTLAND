#pragma once

#include <raylib.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace outland::world::assets {

// Stable integer mixing avoids signed overflow and platform-dependent sine hashes.
inline float variation(int x, int z, int salt) {
    std::uint32_t value = static_cast<std::uint32_t>(x) * 0x8da6b343U;
    value ^= static_cast<std::uint32_t>(z) * 0xd8163841U;
    value ^= static_cast<std::uint32_t>(salt) * 0xcb1ab31fU;
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return static_cast<float>(value >> 8) / 16777216.0F;
}

inline float distance_squared(Vector3 a, Vector3 b) {
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return x*x + y*y + z*z;
}

// Closed gable prism: bottom corners then front/rear ridge, all faces outward.
inline std::array<Vector3, 6> roof_vertices(Vector3 base, Vector3 size) {
    const float x = size.x * 0.5F + 0.35F;
    const float z = size.z * 0.5F + 0.35F;
    const float y = base.y + size.y;
    const float rise = size.x * 0.22F;
    return {{{base.x-x, y, base.z-z}, {base.x+x, y, base.z-z},
             {base.x+x, y, base.z+z}, {base.x-x, y, base.z+z},
             {base.x, y+rise, base.z-z}, {base.x, y+rise, base.z+z}}};
}

inline constexpr std::array<std::array<int, 3>, 8> roof_faces{{
    {{0,4,1}}, {{3,2,5}}, {{0,3,5}}, {{0,5,4}},
    {{1,4,5}}, {{1,5,2}}, {{0,1,2}}, {{0,2,3}}
}};

inline void draw_quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color) {
    DrawTriangle3D(a, b, c, color);
    DrawTriangle3D(a, c, d, color);
}

inline void draw_blade(Vector3 a, Vector3 b, Vector3 tip, Color color) {
    DrawTriangle3D(a, b, tip, color);
    DrawTriangle3D(tip, b, a, color);
}

} // namespace outland::world::assets
