#include "outland/game/mafia/MafioRenderer.hpp"
#include <raymath.h>
#include <cmath>

namespace outland::game::mafia {

namespace {
// Both faces: cloth is seen from either side.
void quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color) {
    DrawTriangle3D(a, b, c, color);
    DrawTriangle3D(a, c, d, color);
    DrawTriangle3D(a, c, b, color);
    DrawTriangle3D(a, d, c, color);
}
void banner(const Crew& crew, float time) {
    Vector3 out = Vector3Subtract(crew.door, crew.interior);
    out.y = 0;
    out = Vector3Length(out) > .1F ? Vector3Normalize(out) : Vector3{0, 0, 1};
    const Vector3 side{-out.z, 0, out.x};
    const Vector3 foot = Vector3Add(Vector3Add(crew.door, Vector3Scale(out, 2.8F)), Vector3Scale(side, 3.4F));
    constexpr float height = 5.2F;
    DrawCylinder(foot, .07F, .06F, height, 6, Color{60, 60, 64, 255});
    DrawSphere({foot.x, foot.y + height + .05F, foot.z}, .11F, Color{210, 180, 90, 255});
    // The cloth: strips that ripple away from the pole.
    const Color color = gangs[static_cast<std::size_t>(crew.gang)].color;
    const Color dark{static_cast<unsigned char>(color.r * .6F), static_cast<unsigned char>(color.g * .6F), static_cast<unsigned char>(color.b * .6F), 255};
    constexpr int strips = 8;
    constexpr float length = 2.0F, drop = 1.2F;
    const float top = foot.y + height - .1F;
    for (int i = 0; i < strips; ++i) {
        const float a = static_cast<float>(i) / strips, b = static_cast<float>(i + 1) / strips;
        const auto wave = [&](float u) { return std::sin(time * 3.1F - u * 5.5F) * .18F * u; };
        const Vector3 p0 = Vector3Add(Vector3Add(foot, Vector3Scale(side, a * length)), Vector3Scale(out, wave(a)));
        const Vector3 p1 = Vector3Add(Vector3Add(foot, Vector3Scale(side, b * length)), Vector3Scale(out, wave(b)));
        const Vector3 t0{p0.x, top, p0.z}, t1{p1.x, top, p1.z}, b0{p0.x, top - drop, p0.z}, b1{p1.x, top - drop, p1.z};
        const Color c = i % 2 == 0 ? color : Color{static_cast<unsigned char>((color.r + dark.r) / 2), static_cast<unsigned char>((color.g + dark.g) / 2), static_cast<unsigned char>((color.b + dark.b) / 2), 255};
        quad(t0, b0, b1, t1, c);
        // A dark band through the middle: the gang's stripe.
        const Vector3 m0{p0.x, top - drop * .42F, p0.z}, m1{p1.x, top - drop * .42F, p1.z};
        const Vector3 n0{p0.x, top - drop * .58F, p0.z}, n1{p1.x, top - drop * .58F, p1.z};
        quad(Vector3Add(m0, Vector3Scale(out, .01F)), Vector3Add(n0, Vector3Scale(out, .01F)), Vector3Add(n1, Vector3Scale(out, .01F)), Vector3Add(m1, Vector3Scale(out, .01F)), dark);
        quad(Vector3Add(m0, Vector3Scale(out, -.01F)), Vector3Add(n0, Vector3Scale(out, -.01F)), Vector3Add(n1, Vector3Scale(out, -.01F)), Vector3Add(m1, Vector3Scale(out, -.01F)), dark);
    }
}

void table(const Crew& crew) {
    const Vector3 t = crew.table;
    DrawCylinder(t, .32F, .22F, .08F, 16, Color{50, 34, 22, 255});                 // foot
    DrawCylinder({t.x, t.y + .08F, t.z}, .09F, .09F, .66F, 10, Color{60, 40, 26, 255}); // pedestal
    DrawCylinder({t.x, t.y + .74F, t.z}, 1.15F, 1.15F, .06F, 32, Color{92, 52, 30, 255}); // rail
    DrawCylinder({t.x, t.y + .80F, t.z}, 1.02F, 1.02F, .012F, 32, Color{24, 112, 58, 255}); // felt
    // Chip stacks and a spread of cards.
    const Color chips[]{{200, 40, 40, 255}, {30, 60, 160, 255}, {235, 235, 235, 255}, {30, 30, 30, 255}};
    for (int i = 0; i < 4; ++i) {
        const float a = static_cast<float>(i) * 1.57F + .4F;
        const Vector3 c{t.x + std::cos(a) * .7F, t.y + .812F, t.z + std::sin(a) * .7F};
        DrawCylinder(c, .045F, .045F, .03F + .02F * static_cast<float>(i % 3), 10, chips[i]);
    }
    for (int i = 0; i < 5; ++i)
        DrawCube({t.x - .28F + static_cast<float>(i) * .14F, t.y + .816F, t.z}, .1F, .004F, .14F, Color{248, 246, 238, 255});
}
}

void draw_turf(const Mafio& mafio, Vector3 camera, float time) {
    for (const auto& crew : mafio.crews()) {
        const float d = std::hypot(crew.door.x - camera.x, crew.door.z - camera.z);
        if (d > 260) continue;
        banner(crew, time);
        if (crew.casino && d < 90) table(crew);
    }
}

} // namespace outland::game::mafia
