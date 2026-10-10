#include "outland/game/culture/SignRenderer.hpp"
#include "outland/game/culture/Esperanto.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>

namespace outland::game::culture {
namespace {
constexpr float pad = .14F, depth = .08F, line_gap = 1.18F;
Color darker(Color c, float k) {
    return {static_cast<unsigned char>(c.r * k), static_cast<unsigned char>(c.g * k), static_cast<unsigned char>(c.b * k), 255};
}
float stack_height(const std::vector<SignLine>& lines) {
    float h = 0;
    for (std::size_t i = 0; i < lines.size(); ++i) h += lines[i].height * (i + 1 < lines.size() ? line_gap : 1.0F);
    return h;
}
}

SignRenderer::~SignRenderer() { unload(); }

void SignRenderer::load(const std::string& application_dir) {
    unload();
    const std::string local = "assets/fonts/DejaVuSans-Bold.ttf";
    const std::string packaged = application_dir + local;
    const std::string path = FileExists(packaged.c_str()) ? packaged : local;
    auto codepoints = sign_codepoints();
    if (FileExists(path.c_str())) {
        font_ = LoadFontEx(path.c_str(), 64, codepoints.data(), static_cast<int>(codepoints.size()));
        own_font_ = font_.texture.id != 0 && font_.texture.id != GetFontDefault().texture.id;
    }
    if (!own_font_) font_ = GetFontDefault();
    else {
        // Mipmapped so distant lettering softens instead of shimmering.
        GenTextureMipmaps(&font_.texture);
        SetTextureFilter(font_.texture, TEXTURE_FILTER_TRILINEAR);
    }
    Image cloth = flag_image(256);
    flag_ = LoadTextureFromImage(cloth);
    UnloadImage(cloth);
    GenTextureMipmaps(&flag_);
    SetTextureFilter(flag_, TEXTURE_FILTER_TRILINEAR);
    loaded_ = true;
    layout();
}

void SignRenderer::unload() {
    if (!loaded_) return;
    if (own_font_) UnloadFont(font_);
    if (flag_.id) UnloadTexture(flag_);
    font_ = {}; flag_ = {}; own_font_ = false; loaded_ = false;
}

void SignRenderer::set(Signage signage) {
    signage_ = std::move(signage);
    layout();
}

float SignRenderer::measure(const std::string& text, float height) const {
    if (!loaded_ || font_.baseSize <= 0) return static_cast<float>(utf8_codepoints(text).size()) * height * .66F;
    const Vector2 size = MeasureTextEx(font_, text.c_str(), static_cast<float>(font_.baseSize), 0);
    return size.x * height / static_cast<float>(font_.baseSize);
}

void SignRenderer::layout() {
    measured_.assign(signage_.signs.size(), {});
    for (std::size_t i = 0; i < signage_.signs.size(); ++i) {
        const auto& sign = signage_.signs[i];
        auto& m = measured_[i];
        const float room = std::max(.4F, sign.max_width - 2 * pad);
        float widest = 0;
        const auto fit = [&](const std::vector<SignLine>& lines, std::vector<float>& scales) {
            for (const auto& line : lines) {
                const float w = measure(line.text, line.height);
                const float k = w > room ? room / w : 1.0F;
                scales.push_back(k);
                widest = std::max(widest, w * k);
            }
        };
        fit(sign.front, m.scale);
        fit(sign.back, m.back_scale);
        m.width = widest + 2 * pad;
        m.height = std::max(stack_height(sign.front), stack_height(sign.back)) + 2 * pad;
    }
}

void SignRenderer::text_line(const std::string& text, float height, float scale, float y_top, Color ink) const {
    if (font_.baseSize <= 0 || !font_.glyphs) return;
    const float s = height / static_cast<float>(font_.baseSize);
    const float width = measure(text, height) * scale;
    const float tw = static_cast<float>(font_.texture.width), th = static_cast<float>(font_.texture.height);
    const float p = static_cast<float>(font_.glyphPadding);
    // Squeezed horizontally when it must fit, never vertically.
    const float sx = s * scale;
    float x = -width * .5F;
    for (const int cp : utf8_codepoints(text)) {
        const int g = GetGlyphIndex(font_, cp);
        const auto& glyph = font_.glyphs[g];
        const Rectangle r = font_.recs[g];
        if (cp != ' ' && cp != '\t') {
            const float gx = x + (static_cast<float>(glyph.offsetX) - p) * sx;
            const float gy = y_top - (static_cast<float>(glyph.offsetY) - p) * s;
            const float w = (r.width + 2 * p) * sx, h = (r.height + 2 * p) * s;
            const float u0 = (r.x - p) / tw, v0 = (r.y - p) / th, u1 = (r.x + r.width + p) / tw, v1 = (r.y + r.height + p) / th;
            // One quad per begin/end, as raylib's own text drawing does: the batch is flushed
            // when full, and consecutive quads with the same texture still share a draw call.
            rlCheckRenderBatchLimit(4);
            rlBegin(RL_QUADS);
            rlColor4ub(ink.r, ink.g, ink.b, 255);
            rlNormal3f(0, 0, 1);
            rlTexCoord2f(u0, v0); rlVertex3f(gx, gy, 0);
            rlTexCoord2f(u0, v1); rlVertex3f(gx, gy - h, 0);
            rlTexCoord2f(u1, v1); rlVertex3f(gx + w, gy - h, 0);
            rlTexCoord2f(u1, v0); rlVertex3f(gx + w, gy, 0);
            rlEnd();
        }
        x += (glyph.advanceX ? static_cast<float>(glyph.advanceX) : r.width) * sx;
    }
}

void SignRenderer::draw(Vector3 camera, Vector3 camera_target, float time_seconds) const {
    if (!loaded_) return;
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera_target, camera));
    const auto visible = [&](Vector3 at, float range) {
        const Vector3 to = Vector3Subtract(at, camera);
        const float d2 = Vector3LengthSqr(to);
        if (d2 > range * range) return false;
        return d2 < 400 || Vector3DotProduct(to, forward) > -10;
    };
    std::vector<std::size_t> shown;
    for (std::size_t i = 0; i < signage_.signs.size(); ++i) if (visible(signage_.signs[i].position, sign_range)) shown.push_back(i);

    // 1. Boards, frames and posts.
    for (const auto i : shown) {
        const auto& sign = signage_.signs[i];
        const auto& m = measured_[i];
        rlPushMatrix();
        rlTranslatef(sign.position.x, sign.position.y, sign.position.z);
        rlRotatef(sign.yaw, 0, 1, 0);
        DrawCube({0, 0, -.05F}, m.width, m.height, depth, sign.board);
        DrawCube({0, 0, -.055F}, m.width + .07F, m.height + .07F, depth * .8F, darker(sign.board, .55F));
        if (sign.post > 0) {
            const float bottom = -m.height * .5F;
            if (m.width > 2.6F) for (const float x : {-m.width * .32F, m.width * .32F})
                DrawCube({x, bottom - sign.post * .5F, -.05F}, .08F, sign.post, .08F, Color{120, 124, 128, 255});
            else DrawCube({0, bottom - sign.post * .5F, -.05F}, .09F, sign.post, .09F, Color{120, 124, 128, 255});
        }
        rlPopMatrix();
    }
    std::vector<std::size_t> flags;
    for (std::size_t i = 0; i < signage_.flags.size(); ++i) if (visible(signage_.flags[i].tip, flag_range)) flags.push_back(i);
    for (const auto i : flags) {
        const auto& flag = signage_.flags[i];
        const float r = flag.width > 3 ? .09F : .05F;
        DrawCylinderEx(flag.base, flag.tip, r, r * .75F, 6, Color{205, 208, 212, 255});
        DrawCube(Vector3Add(flag.tip, {0, .06F, 0}), r * 3, r * 2.4F, r * 3, Color{214, 180, 70, 255});
    }

    // 2. Lettering, one texture for every sign in range.
    rlSetTexture(font_.texture.id);
    for (const auto i : shown) {
        const auto& sign = signage_.signs[i];
        if (Vector3DistanceSqr(sign.position, camera) > text_range * text_range) continue;
        const auto& m = measured_[i];
        const auto face = [&](const std::vector<SignLine>& lines, const std::vector<float>& scales, bool back) {
            if (lines.empty()) return;
            rlPushMatrix();
            rlTranslatef(sign.position.x, sign.position.y, sign.position.z);
            rlRotatef(sign.yaw + (back ? 180.0F : 0.0F), 0, 1, 0);
            rlTranslatef(0, 0, back ? .105F : .006F);
            float y = stack_height(lines) * .5F;
            for (std::size_t k = 0; k < lines.size(); ++k) {
                text_line(lines[k].text, lines[k].height, scales[k], y, sign.ink);
                y -= lines[k].height * line_gap;
            }
            rlPopMatrix();
        };
        face(sign.front, m.scale, false);
        face(sign.back, m.back_scale, true);
    }
    rlSetTexture(0);

    // 3. Flags: the cloth ripples away from the staff, more toward the free end.
    if (flags.empty()) return;
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlSetTexture(flag_.id);
    constexpr int columns = 12, rows = 4;
    for (const auto i : flags) {
        const auto& flag = signage_.flags[i];
        const float W = flag.width, H = flag.width * .5F;
        const Vector3 across{-flag.fly.z, 0, flag.fly.x};
        const float phase = static_cast<float>(i) * 1.7F;
        const auto at = [&](int c, int r, float& shade) {
            const float u = static_cast<float>(c) / columns, v = static_cast<float>(r) / rows;
            const float arg = u * 7.0F - time_seconds * 5.5F + phase + v * .8F;
            const float wave = std::sin(arg) * .07F * W * std::pow(u, .8F);
            shade = .8F + .2F * std::cos(arg);
            return Vector3{flag.tip.x + flag.fly.x * u * W * .97F + across.x * wave, flag.tip.y - .08F - v * H,
                           flag.tip.z + flag.fly.z * u * W * .97F + across.z * wave};
        };
        for (int c = 0; c < columns; ++c) for (int r = 0; r < rows; ++r) {
            const int corners[4][2]{{c, r}, {c, r + 1}, {c + 1, r + 1}, {c + 1, r}};
            rlCheckRenderBatchLimit(4);
            rlBegin(RL_QUADS);
            for (const auto& k : corners) {
                float shade = 1;
                const Vector3 p = at(k[0], k[1], shade);
                const auto b = static_cast<unsigned char>(255 * shade);
                rlColor4ub(b, b, b, 255);
                rlTexCoord2f(static_cast<float>(k[0]) / columns, static_cast<float>(k[1]) / rows);
                rlVertex3f(p.x, p.y, p.z);
            }
            rlEnd();
        }
    }
    rlSetTexture(0);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}
}
