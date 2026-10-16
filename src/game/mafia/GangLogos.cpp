#include "outland/game/mafia/GangLogos.hpp"
#include "outland/game/mafia/Mafio.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <cmath>

namespace outland::game::mafia {
namespace {
constexpr const char* initials[gang_count]{"LVM", "ML", "OA", "MR", "FH"};

// Each emblem twice: once fat in black (the outline), once in colour.
void emblem(int gang, Vector2 c, float grow, Color ink) {
    const float g = grow;
    switch (gang) {
        case 0: {   // an open hand
            DrawCircleV({c.x, c.y + 10}, 24 + g, ink);
            for (int f = 0; f < 4; ++f)
                DrawRectangleRounded({c.x - 22 + f * 12.0F - g, c.y - 34 - g - (f == 1 || f == 2 ? 6.0F : 0.0F), 9 + 2 * g, 40 + 2 * g}, .5F, 6, ink);
            DrawRectangleRounded({c.x + 18 - g, c.y - 2 - g, 22 + 2 * g, 10 + 2 * g}, .5F, 6, ink);   // thumb
            break;
        }
        case 1: {   // a wolf's head: ears, brow, muzzle
            DrawTriangle({c.x - 30 - g, c.y - 30 - g}, {c.x - 26 - g, c.y + 4}, {c.x - 8, c.y - 12 - g}, ink);
            DrawTriangle({c.x + 30 + g, c.y - 30 - g}, {c.x + 8, c.y - 12 - g}, {c.x + 26 + g, c.y + 4}, ink);
            DrawTriangle({c.x - 30 - g, c.y - 12 - g}, {c.x, c.y + 38 + g}, {c.x + 30 + g, c.y - 12 - g}, ink);
            break;
        }
        case 2: {   // an anchor
            DrawRing({c.x, c.y - 28}, 6 - g * .5F, 11 + g, 0, 360, 24, ink);
            DrawRectangleV({c.x - 4 - g, c.y - 18 - g}, {8 + 2 * g, 50 + 2 * g}, ink);
            DrawRectangleV({c.x - 20 - g, c.y - 14 - g}, {40 + 2 * g, 7 + 2 * g}, ink);
            DrawRing({c.x, c.y + 4}, 24 - g * .5F, 31 + g, 0, 180, 24, ink);
            DrawTriangle({c.x - 36 - g, c.y + 2}, {c.x - 22, c.y + 12 + g}, {c.x - 24, c.y - 6 - g}, ink);
            DrawTriangle({c.x + 36 + g, c.y + 2}, {c.x + 24, c.y - 6 - g}, {c.x + 22, c.y + 12 + g}, ink);
            break;
        }
        case 3: {   // a frog: body, two eyes on top
            DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y + 10), 34 + g, 22 + g, ink);
            DrawCircleV({c.x - 18, c.y - 14}, 11 + g, ink);
            DrawCircleV({c.x + 18, c.y - 14}, 11 + g, ink);
            break;
        }
        default: {  // a spiked collar
            DrawRing(c, 18 - g * .5F, 28 + g, 0, 360, 32, ink);
            for (int k = 0; k < 8; ++k) {
                const float a = static_cast<float>(k) * PI / 4;
                const Vector2 out{c.x + std::cos(a) * (40 + g), c.y + std::sin(a) * (40 + g)};
                const Vector2 l{c.x + std::cos(a - .22F) * 26, c.y + std::sin(a - .22F) * 26};
                const Vector2 r{c.x + std::cos(a + .22F) * 26, c.y + std::sin(a + .22F) * 26};
                DrawTriangle(out, l, r, ink);
                DrawTriangle(out, r, l, ink);
            }
            break;
        }
    }
}
}

void GangLogos::prepare() {
    if (ready_ || !IsWindowReady()) return;
    for (int gang = 0; gang < gang_count; ++gang) {
        auto& target = textures_[static_cast<std::size_t>(gang)];
        target = LoadRenderTexture(width, height);
        SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
        const Color colour = gangs[static_cast<std::size_t>(gang)].color;
        BeginTextureMode(target);
        ClearBackground(BLANK);
        // Overspray: a haze of the colour behind it all.
        for (int k = 0; k < 14; ++k)
            DrawCircleV({30.0F + static_cast<float>((k * 37) % 200), 40.0F + static_cast<float>((k * 53) % 50)}, 18.0F + static_cast<float>(k % 4) * 6,
                        Fade(colour, .12F));
        const Vector2 centre{58, 62};
        emblem(gang, centre, 5, Color{12, 12, 12, 255});
        emblem(gang, centre, 0, colour);
        // The letters: black outline, colour, a white highlight; drips under them.
        const char* text = initials[gang];
        const int size = gang == 0 ? 46 : 58;
        const int x = 112, y = 34;
        for (int dx = -3; dx <= 3; dx += 3)
            for (int dy = -3; dy <= 3; dy += 3) DrawText(text, x + dx, y + dy, size, Color{12, 12, 12, 255});
        DrawText(text, x, y, size, colour);
        DrawText(text, x - 1, y - 1, size, Fade(WHITE, .25F));
        for (int d = 0; d < 5; ++d) {
            const float dx = static_cast<float>(x) + 10 + static_cast<float>(d) * 26 + static_cast<float>((d * 7) % 9);
            const float length = 10.0F + static_cast<float>((d * 13 + gang * 5) % 22);
            DrawRectangleV({dx, static_cast<float>(y + size - 6)}, {3, length}, colour);
            DrawCircleV({dx + 1.5F, static_cast<float>(y + size - 6) + length}, 2.6F, colour);
        }
        EndTextureMode();
    }
    ready_ = true;
}

void GangLogos::unload() {
    if (!ready_) return;
    for (auto& t : textures_) UnloadRenderTexture(t);
    ready_ = false;
}

void GangLogos::draw(const Tag& tag, Vector3 camera) const {
    if (!ready_ || tag.gang < 0 || tag.gang >= gang_count || Vector3Distance(tag.position, camera) > 140) return;
    Vector3 n = Vector3Normalize(tag.normal);
    if (Vector3Length(n) < .5F) return;
    const Vector3 up{0, 1, 0};
    Vector3 right = Vector3CrossProduct(Vector3Negate(n), up);
    if (Vector3Length(right) < 1e-3F) return;
    right = Vector3Normalize(right);
    const Vector3 lift = Vector3CrossProduct(right, Vector3Negate(n));   // up along the wall
    const Vector3 c = Vector3Add(tag.position, Vector3Scale(n, .025F));
    const Vector3 hw = Vector3Scale(right, wall_width * .5F), hh = Vector3Scale(lift, wall_height * .5F);
    const Vector3 bl = Vector3Subtract(Vector3Subtract(c, hw), hh), br = Vector3Subtract(Vector3Add(c, hw), hh);
    const Vector3 tr = Vector3Add(Vector3Add(c, hw), hh), tl = Vector3Add(Vector3Subtract(c, hw), hh);
    rlDisableBackfaceCulling();
    rlSetTexture(texture(tag.gang).id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 235);
    rlNormal3f(n.x, n.y, n.z);
    rlTexCoord2f(0, 0); rlVertex3f(bl.x, bl.y, bl.z);
    rlTexCoord2f(1, 0); rlVertex3f(br.x, br.y, br.z);
    rlTexCoord2f(1, 1); rlVertex3f(tr.x, tr.y, tr.z);
    rlTexCoord2f(0, 1); rlVertex3f(tl.x, tl.y, tl.z);
    rlEnd();
    rlSetTexture(0);
    rlEnableBackfaceCulling();
}

void GangLogos::draw_flat(int gang, Rectangle where) const {
    if (!ready_ || gang < 0 || gang >= gang_count) return;
    const auto& t = texture(gang);
    DrawTexturePro(t, {0, 0, static_cast<float>(t.width), -static_cast<float>(t.height)}, where, {0, 0}, 0, WHITE);
}

} // namespace outland::game::mafia
