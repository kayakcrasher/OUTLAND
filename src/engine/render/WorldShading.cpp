#include "outland/engine/render/WorldShading.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <rlgl.h>

namespace outland::engine::render {
namespace {

// The light is worked out per vertex (cheap on a phone; the models are low-poly) and the haze
// from the depth (the clip-space w is the distance along the view).
constexpr const char* kLitVertex = R"(
IN vec3 vertexPosition;
IN vec2 vertexTexCoord;
IN vec3 vertexNormal;
IN vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matNormal;
uniform vec3 sunDir;
uniform vec3 sunColor;
uniform vec3 skyLight;
uniform vec3 groundLight;
uniform vec3 fogRange;   // start, 1/(end - start), most
OUT vec2 fragTexCoord;
OUT vec4 fragColor;
OUT vec3 fragLight;
OUT float fragFog;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    vec3 n = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    float up = n.y * 0.5 + 0.5;
    fragLight = mix(groundLight, skyLight, up) + sunColor * max(dot(n, sunDir), 0.0);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
    fragFog = clamp((gl_Position.w - fogRange.x) * fogRange.y, 0.0, 1.0) * fogRange.z;
}
)";

constexpr const char* kFlatVertex = R"(
IN vec3 vertexPosition;
IN vec2 vertexTexCoord;
IN vec4 vertexColor;
uniform mat4 mvp;
uniform vec3 fogRange;
OUT vec2 fragTexCoord;
OUT vec4 fragColor;
OUT vec3 fragLight;
OUT float fragFog;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragLight = vec3(1.0);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
    fragFog = clamp((gl_Position.w - fogRange.x) * fogRange.y, 0.0, 1.0) * fogRange.z;
}
)";

constexpr const char* kFragment = R"(
IN vec2 fragTexCoord;
IN vec4 fragColor;
IN vec3 fragLight;
IN float fragFog;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 fogColor;
void main() {
    vec4 texel = TEXTURE(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 lit = min(texel.rgb * fragLight, vec3(1.0));
    FRAG_COLOR = vec4(mix(lit, fogColor, fragFog), texel.a);
}
)";

struct State {
    bool ready{false};
    Shader lit{}, flat{};
    int lit_sun{-1}, lit_sun_color{-1}, lit_sky{-1}, lit_ground{-1}, lit_fog{-1}, lit_fog_color{-1};
    int flat_fog{-1}, flat_fog_color{-1};
    std::unordered_set<unsigned int> mipmapped;
};
State& state() {
    static State s;
    return s;
}

std::string source(const char* body, bool vertex) {
    const int version = rlGetVersion();
    const bool desktop = version == RL_OPENGL_33 || version == RL_OPENGL_43;
    std::string head;
    if (desktop) {
        head = "#version 330\n#define TEXTURE texture\n";
        head += vertex ? "#define IN in\n#define OUT out\n" : "#define IN in\nout vec4 finalColor;\n#define FRAG_COLOR finalColor\n";
    } else {
        // (Vertex positions in high precision: the island is kilometres across.)
        head = vertex ? "#version 100\nprecision highp float;\n#define TEXTURE texture2D\n" : "#version 100\nprecision mediump float;\n#define TEXTURE texture2D\n";
        head += vertex ? "#define IN attribute\n#define OUT varying\n" : "#define IN varying\n#define FRAG_COLOR gl_FragColor\n";
    }
    return head + body;
}

bool usable(const Shader& shader) { return shader.id != 0 && shader.id != rlGetShaderIdDefault(); }

void texture_quality(Texture2D& texture) {
    auto& s = state();
    if (texture.id == 0 || texture.id == rlGetTextureIdDefault() || !s.mipmapped.insert(texture.id).second) return;
    if (texture.mipmaps <= 1) GenTextureMipmaps(&texture);
    SetTextureFilter(texture, texture.mipmaps > 1 ? TEXTURE_FILTER_TRILINEAR : TEXTURE_FILTER_BILINEAR);
    // Anisotropic filtering keeps roads and walls sharp at a glancing angle (desktop GL has it;
    // phones and browsers may not).
    if (texture.mipmaps > 1 && (rlGetVersion() == RL_OPENGL_33 || rlGetVersion() == RL_OPENGL_43)) SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_4X);
}

} // namespace

void shading_initialize() {
    auto& s = state();
    if (s.ready) return;
    if (const char* off = std::getenv("OUTLAND_SHADING"); off && *off == '0') return;
    const auto fragment = source(kFragment, false);
    s.lit = LoadShaderFromMemory(source(kLitVertex, true).c_str(), fragment.c_str());
    s.flat = LoadShaderFromMemory(source(kFlatVertex, true).c_str(), fragment.c_str());
    if (!usable(s.lit) || !usable(s.flat)) {
        TraceLog(LOG_WARNING, "OUTLAND shading: shaders did not compile; drawing unlit");
        if (usable(s.lit)) UnloadShader(s.lit);
        if (usable(s.flat)) UnloadShader(s.flat);
        s.lit = s.flat = Shader{};
        return;
    }
    s.lit_sun = GetShaderLocation(s.lit, "sunDir");
    s.lit_sun_color = GetShaderLocation(s.lit, "sunColor");
    s.lit_sky = GetShaderLocation(s.lit, "skyLight");
    s.lit_ground = GetShaderLocation(s.lit, "groundLight");
    s.lit_fog = GetShaderLocation(s.lit, "fogRange");
    s.lit_fog_color = GetShaderLocation(s.lit, "fogColor");
    s.flat_fog = GetShaderLocation(s.flat, "fogRange");
    s.flat_fog_color = GetShaderLocation(s.flat, "fogColor");
    s.ready = true;
    shading_frame(world::sky::sky_at(12));
}

void shading_shutdown() {
    auto& s = state();
    if (!s.ready) return;
    UnloadShader(s.lit);
    UnloadShader(s.flat);
    s = State{};
}

bool shading_enabled() { return state().ready; }

void shade_model(Model& model) {
    auto& s = state();
    if (!s.ready) return;
    bool normals = true;
    for (int i = 0; i < model.meshCount; ++i) normals &= model.meshes[i].normals != nullptr;
    for (int i = 0; i < model.materialCount; ++i) {
        auto& material = model.materials[i];
        if (material.shader.id == rlGetShaderIdDefault()) material.shader = normals ? s.lit : s.flat;
        if (material.maps) texture_quality(material.maps[MATERIAL_MAP_DIFFUSE].texture);
    }
}

void shade_flat(Material& material) {
    auto& s = state();
    if (s.ready && material.shader.id == rlGetShaderIdDefault()) material.shader = s.flat;
}

void shading_frame(const world::sky::SkyState& sky) {
    auto& s = state();
    if (!s.ready) return;
    // The sun: warm and strong when it is high, fading as it sets; the sky light a little blue,
    // the ground's bounce a little green. At night the world is darkened afterwards (the ambient
    // multiply), so here the light just goes flat.
    const float day = std::clamp(sky.daylight, 0.0F, 1.0F);
    const float height = std::clamp(sky.sun.y * 3, 0.0F, 1.0F) * day;
    const float sun[3]{sky.sun.x, sky.sun.y, sky.sun.z};
    const float sun_color[3]{sky.sun_color.r / 255.0F * .44F * height, sky.sun_color.g / 255.0F * .41F * height,
                             sky.sun_color.b / 255.0F * .36F * height};
    const float flat = 1 - height;   // with no sun, everything gets the same light
    const float sky_light[3]{.71F + .27F * flat, .73F + .25F * flat, .79F + .19F * flat};
    const float ground_light[3]{.56F + .41F * flat, .58F + .39F * flat, .53F + .44F * flat};
    // The haze: from 90 m, thickest by 700 m, never hiding everything.
    const float fog[3]{90, 1.0F / 610, .72F};
    const Color horizon = world::sky::before_multiply(sky.horizon, sky.ambient);
    const float fog_color[3]{horizon.r / 255.0F, horizon.g / 255.0F, horizon.b / 255.0F};
    SetShaderValue(s.lit, s.lit_sun, sun, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.lit, s.lit_sun_color, sun_color, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.lit, s.lit_sky, sky_light, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.lit, s.lit_ground, ground_light, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.lit, s.lit_fog, fog, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.lit, s.lit_fog_color, fog_color, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.flat, s.flat_fog, fog, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.flat, s.flat_fog_color, fog_color, SHADER_UNIFORM_VEC3);
}

void shading_begin() {
    if (state().ready) BeginShaderMode(state().flat);
}

void shading_end() {
    if (state().ready) EndShaderMode();
}

} // namespace outland::engine::render
