#include "outland/game/mafia/GangLogos.hpp"
#include "outland/game/mafia/Mafio.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <cmath>
#include <tuple>

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
    if (mural_ready_) UnloadRenderTexture(mural_);
    mural_ready_ = false;
    if (!ready_) return;
    for (auto& t : textures_) UnloadRenderTexture(t);
    ready_ = false;
}

namespace {
// Text centred on `x`, in `font` at `size`, with a dark drop shadow.
void mural_text(const Font& font, const char* text, float x, float y, float size, float spacing, Color colour, Color shadow) {
    const Vector2 m = MeasureTextEx(font, text, size, spacing);
    DrawTextEx(font, text, {x - m.x / 2 + 3, y + 3}, size, spacing, shadow);
    DrawTextEx(font, text, {x - m.x / 2, y}, size, spacing, colour);
}
// A scroll: a ribbon with folded tails, edged in gold.
void scroll(Rectangle r, Color ribbon, Color edge, Color fold) {
    const float tail = r.height * .9F;
    for (const float s : {-1.0F, 1.0F}) {
        const float x = s < 0 ? r.x : r.x + r.width;
        const float out = x + s * tail;
        const Vector2 a{x, r.y + r.height * .25F}, b{out, r.y + r.height * .25F}, c{out - s * tail * .4F, r.y + r.height * .75F},
                      d{out, r.y + r.height * 1.25F}, e{x, r.y + r.height * 1.25F};
        // Drawn as two triangles each, wound both ways so neither is culled.
        for (const auto& [p, q, t] : {std::tuple{a, b, c}, std::tuple{a, c, e}, std::tuple{c, d, e}}) {
            DrawTriangle(p, q, t, fold);
            DrawTriangle(p, t, q, fold);
        }
    }
    DrawRectangleRec({r.x - 4, r.y - 4, r.width + 8, r.height + 8}, edge);
    DrawRectangleRec(r, ribbon);
    DrawRectangleLinesEx({r.x + 5, r.y + 5, r.width - 10, r.height - 10}, 2, edge);
}
// A laurel branch: leaves in pairs along an arc round `c` (radius `r`) from angle `a0` to `a1`
// (degrees, screen space: 90 is straight down).
void laurel(Vector2 c, float r, float a0, float a1, Color leaf, Color dark) {
    const int n = 11;
    Vector2 previous{c.x + std::cos(a0 * DEG2RAD) * r, c.y + std::sin(a0 * DEG2RAD) * r};
    for (int k = 1; k <= n; ++k) {
        const float a = (a0 + (a1 - a0) * static_cast<float>(k) / n) * DEG2RAD;
        const Vector2 p{c.x + std::cos(a) * r, c.y + std::sin(a) * r};
        DrawLineEx(previous, p, 4, dark);
        Vector2 t{p.x - previous.x, p.y - previous.y};
        const float len = std::max(1e-3F, std::hypot(t.x, t.y));
        t = {t.x / len, t.y / len};
        const float size = 22 - 8 * static_cast<float>(k) / n;
        for (const float side : {-1.0F, 1.0F}) {
            const Vector2 nrm{-t.y * side, t.x * side};
            const Vector2 base{(p.x + previous.x) / 2, (p.y + previous.y) / 2};
            const Vector2 tip{base.x + (nrm.x * .8F + t.x * .6F) * size * 1.6F, base.y + (nrm.y * .8F + t.y * .6F) * size * 1.6F};
            const Vector2 mid{(base.x + tip.x) / 2, (base.y + tip.y) / 2};
            const Vector2 w{-(tip.y - base.y) * .22F, (tip.x - base.x) * .22F};
            const Color colour = side > 0 ? leaf : dark;
            for (const auto& [x, y, z] : {std::tuple{base, Vector2{mid.x + w.x, mid.y + w.y}, tip}, std::tuple{base, tip, Vector2{mid.x - w.x, mid.y - w.y}}}) {
                DrawTriangle(x, y, z, colour);
                DrawTriangle(x, z, y, colour);
            }
        }
        previous = p;
    }
}
// A roundel: the open hand in white on green, ringed in gold.
void roundel(Vector2 c, float r) {
    DrawCircleV(c, r + 7, Color{18, 18, 18, 255});
    DrawCircleV(c, r + 4, Color{232, 186, 52, 255});
    DrawCircleV(c, r, Color{0, 140, 60, 255});
    const Color hand{248, 246, 236, 255};
    const float u = r / 9;
    DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y + 2.5F * u), static_cast<int>(3.8F * u), static_cast<int>(3.6F * u), hand);
    for (int f = 0; f < 4; ++f) {
        const float x = c.x + (-2.7F + 1.8F * static_cast<float>(f)) * u;
        const float len = (f == 1 || f == 2 ? 5.6F : 4.6F) * u;
        DrawRectangleRounded({x - .8F * u, c.y - len + .5F * u, 1.6F * u, len + 2 * u}, .9F, 6, hand);
    }
    DrawLineEx({c.x - 2.8F * u, c.y + 3 * u}, {c.x - 5.6F * u, c.y - .2F * u}, 1.8F * u, hand);
}
}

void GangLogos::prepare_mural(const Texture2D& banner, const Font& font) {
    if (mural_ready_ || !IsWindowReady() || banner.id == 0) return;
    mural_ = LoadRenderTexture(mural_width, mural_height);
    SetTextureFilter(mural_.texture, TEXTURE_FILTER_BILINEAR);
    const float W = mural_width, H = mural_height;
    const Color wall{232, 226, 206, 255}, black{20, 20, 20, 255}, gold{232, 186, 52, 255}, gold_dark{150, 106, 22, 255},
                green{0, 140, 60, 255}, green_dark{0, 86, 40, 255}, cream{248, 244, 228, 255}, sky{196, 222, 214, 255};
    BeginTextureMode(mural_);
    ClearBackground(wall);
    // The painted frame: black, gold, a green inner line, gold corner blocks.
    DrawRectangleLinesEx({8, 8, W - 16, H - 16}, 22, black);
    DrawRectangleLinesEx({30, 30, W - 60, H - 60}, 7, gold);
    DrawRectangleLinesEx({40, 40, W - 80, H - 80}, 3, green);
    for (const float x : {8.0F, W - 56}) for (const float y : {8.0F, H - 56}) {
        DrawRectangleRec({x, y, 48, 48}, gold);
        DrawRectangleLinesEx({x + 6, y + 6, 36, 36}, 3, black);
        DrawPoly({x + 24, y + 24}, 5, 11, -90, green);
    }
    // A pale sky behind the colours.
    DrawRectangleGradientV(43, 43, static_cast<int>(W - 86), static_cast<int>(H * .6F), sky, wall);
    // The flags on their poles to either side, leaning out.
    const Rectangle source{0, 0, static_cast<float>(banner.width), static_cast<float>(banner.height)};
    for (const float s : {-1.0F, 1.0F}) {
        const Vector2 foot{W * .5F + s * 236, H * .74F}, top{W * .5F + s * 262, H * .2F};
        DrawLineEx(foot, top, 7, black);
        DrawLineEx(foot, top, 4, Color{205, 208, 212, 255});
        DrawCircleV(top, 8, gold);
        const Rectangle cloth{top.x + (s < 0 ? -164.0F : 4.0F), top.y + 4, 160, 80};
        DrawRectangleRec({cloth.x - 3, cloth.y - 3, cloth.width + 6, cloth.height + 6}, black);
        DrawTexturePro(banner, s < 0 ? source : Rectangle{source.x + source.width, 0, -source.width, source.height}, cloth, {0, 0}, 0, WHITE);
    }
    // The banner itself, large and outlined, in the middle, with laurels below it.
    const Rectangle big{W * .5F - 210, H * .26F, 420, 210};
    const Vector2 wreath{W * .5F, big.y + big.height * .42F};
    laurel(wreath, 236, 96, 176, green, green_dark);
    laurel(wreath, 236, 84, 4, green, green_dark);
    DrawRectangleRec({big.x - 9, big.y - 9, big.width + 18, big.height + 18}, black);
    DrawRectangleRec({big.x - 5, big.y - 5, big.width + 10, big.height + 10}, gold);
    DrawTexturePro(banner, source, big, {0, 0}, 0, WHITE);
    // The hand in roundels at the sides, over the district and the year.
    roundel({W * .5F - 400, H * .6F}, 46);
    roundel({W * .5F + 400, H * .6F}, 46);
    mural_text(font, "VERDKERNO", W * .5F - 400, H * .6F + 62, 26, 2, black, Fade(gold, .6F));
    mural_text(font, "FONDITA 1887", W * .5F + 400, H * .6F + 62, 26, 2, black, Fade(gold, .6F));
    // The scrolls.
    scroll({W * .5F - 300, 52, 600, 72}, black, gold, gold_dark);
    mural_text(font, "LA VERDA MANO", W * .5F, 62, 56, 6, gold, black);
    scroll({W * .5F - 250, H * .79F, 500, 64}, green_dark, gold, Color{0, 60, 28, 255});
    mural_text(font, "NENIAM CEDI", W * .5F, H * .79F + 9, 50, 8, cream, black);
    mural_text(font, "VERDA URBO  -  ĈI TIE REGAS LA VERDA MANO", W * .5F, H - 74, 22, 2, black, Fade(gold, .5F));
    EndTextureMode();
    mural_ready_ = true;
}

void GangLogos::draw_mural(Vector3 centre, Vector3 normal, float width, Vector3 camera) const {
    if (!mural_ready_ || Vector3Distance(centre, camera) > 260) return;
    Vector3 n = Vector3Normalize(Vector3{normal.x, 0, normal.z});
    if (Vector3Length(n) < .5F) return;
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(Vector3Negate(n), {0, 1, 0}));
    const Vector3 lift{0, 1, 0};
    const float height = width * mural_height / mural_width;
    const Vector3 c = Vector3Add(centre, Vector3Scale(n, .03F));
    const Vector3 hw = Vector3Scale(right, width * .5F), hh = Vector3Scale(lift, height * .5F);
    const Vector3 bl = Vector3Subtract(Vector3Subtract(c, hw), hh), br = Vector3Subtract(Vector3Add(c, hw), hh);
    const Vector3 tr = Vector3Add(Vector3Add(c, hw), hh), tl = Vector3Add(Vector3Subtract(c, hw), hh);
    rlDisableBackfaceCulling();
    rlSetTexture(mural_.texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    rlNormal3f(n.x, n.y, n.z);
    rlTexCoord2f(0, 0); rlVertex3f(bl.x, bl.y, bl.z);
    rlTexCoord2f(1, 0); rlVertex3f(br.x, br.y, br.z);
    rlTexCoord2f(1, 1); rlVertex3f(tr.x, tr.y, tr.z);
    rlTexCoord2f(0, 1); rlVertex3f(tl.x, tl.y, tl.z);
    rlEnd();
    rlSetTexture(0);
    rlEnableBackfaceCulling();
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

void draw_green_hand_livery(Vector3 position, float yaw, const Texture2D& banner, Vector3 camera) {
    if (banner.id == 0 || Vector3Distance(position, camera) > 120) return;
    const auto at = [&](float x, float y, float z) { return vehicles::VehicleSystem::world_point({x, y, z}, position, yaw); };
    // One textured quad from four car-local corners (texture u0..u1 across, v0..v1 down).
    const auto quad = [&](Vector3 a, Vector3 b, Vector3 c, Vector3 d, float u0, float u1) {
        rlTexCoord2f(u0, 0); rlVertex3f(a.x, a.y, a.z);
        rlTexCoord2f(u0, 1); rlVertex3f(b.x, b.y, b.z);
        rlTexCoord2f(u1, 1); rlVertex3f(c.x, c.y, c.z);
        rlTexCoord2f(u1, 0); rlVertex3f(d.x, d.y, d.z);
    };
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlSetTexture(banner.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    // The bonnet: the banner's middle (crown, star and hand on the green upright), its top toward
    // the nose so it reads from in front.
    const float y = .752F, z0 = 1.12F, z1 = 1.86F, half = .31F;
    quad(at(-half, y, z1), at(-half, y, z0), at(half, y, z0), at(half, y, z1), .29F, .71F);
    // The front wings: the whole banner, hoist toward the front, both sides.
    for (const float side : {-1.0F, 1.0F}) {
        const float x = side * .985F;
        quad(at(x, .54F, .86F), at(x, .27F, .86F), at(x, .27F, .32F), at(x, .54F, .32F), 0, 1);
    }
    rlEnd();
    rlSetTexture(0);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

void GangLogos::draw_flat(int gang, Rectangle where) const {
    if (!ready_ || gang < 0 || gang >= gang_count) return;
    const auto& t = texture(gang);
    DrawTexturePro(t, {0, 0, static_cast<float>(t.width), -static_cast<float>(t.height)}, where, {0, 0}, 0, WHITE);
}

} // namespace outland::game::mafia
