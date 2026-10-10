#include "outland/world/sky/DayNight.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace outland::world::sky {
namespace {
struct Key { float hour; Color zenith, horizon, ambient; float daylight; };
// A day on Verda: deep blue nights, warm short dawns and dusks, pale blue noon.
constexpr std::array<Key, 11> keys{{
    {0.0F,  {6, 9, 22, 255},     {16, 22, 44, 255},    {52, 62, 105, 255},   0.0F},
    {4.5F,  {6, 9, 22, 255},     {18, 24, 48, 255},    {52, 62, 105, 255},   0.0F},
    {5.5F,  {30, 40, 85, 255},   {225, 140, 112, 255}, {120, 106, 132, 255}, .35F},
    {6.5F,  {88, 138, 200, 255}, {248, 200, 160, 255}, {222, 202, 188, 255}, .75F},
    {8.0F,  {100, 160, 225, 255},{175, 210, 235, 255}, {255, 255, 255, 255}, 1.0F},
    {15.0F, {92, 152, 222, 255}, {178, 208, 233, 255}, {255, 255, 255, 255}, 1.0F}, // midday: no multiply pass
    {17.0F, {95, 150, 215, 255}, {190, 205, 220, 255}, {255, 250, 240, 255}, 1.0F},
    {18.5F, {80, 110, 180, 255}, {250, 170, 110, 255}, {240, 202, 172, 255}, .7F},
    {19.5F, {35, 40, 90, 255},   {200, 100, 90, 255},  {122, 102, 136, 255}, .3F},
    {20.5F, {12, 16, 40, 255},   {32, 32, 62, 255},    {60, 68, 110, 255},   .05F},
    {24.0F, {6, 9, 22, 255},     {16, 22, 44, 255},    {52, 62, 105, 255},   0.0F},
}};
Color mix(Color a, Color b, float t) {
    const auto lerp = [&](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(std::clamp(std::lround(x + (y - x) * t), 0L, 255L));
    };
    return {lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b), 255};
}
Color scale(Color c, float k) {
    k = std::clamp(k, 0.0F, 1.0F);
    return {static_cast<unsigned char>(c.r * k), static_cast<unsigned char>(c.g * k), static_cast<unsigned char>(c.b * k), 255};
}
std::uint32_t hash(std::uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
float unit(std::uint32_t x) { return static_cast<float>(hash(x) & 0xFFFFFF) / static_cast<float>(0x1000000); }

// A soft disc that fades from `centre` to black at the rim; additive black adds nothing.
void glow_disc(Vector3 centre, Vector3 axis_a, Vector3 axis_b, Color colour, int segments = 18) {
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < segments; ++i) {
        const float a0 = 2 * PI * i / segments, a1 = 2 * PI * (i + 1) / segments;
        const auto rim = [&](float a) { return Vector3Add(centre, Vector3Add(Vector3Scale(axis_a, std::cos(a)), Vector3Scale(axis_b, std::sin(a)))); };
        const auto p0 = rim(a0), p1 = rim(a1);
        rlColor4ub(colour.r, colour.g, colour.b, 255); rlVertex3f(centre.x, centre.y, centre.z);
        rlColor4ub(0, 0, 0, 255); rlVertex3f(p1.x, p1.y, p1.z);
        rlColor4ub(0, 0, 0, 255); rlVertex3f(p0.x, p0.y, p0.z);
    }
    rlEnd();
}
// Camera-facing glow sprite.
void glow_sprite(Vector3 at, float radius, Color colour, Vector3 camera) {
    auto view = Vector3Subtract(at, camera);
    if (Vector3LengthSqr(view) < 1e-4F) return;
    view = Vector3Normalize(view);
    Vector3 right = Vector3CrossProduct(view, {0, 1, 0});
    if (Vector3LengthSqr(right) < 1e-4F) right = {1, 0, 0};
    right = Vector3Normalize(right);
    const Vector3 up = Vector3CrossProduct(right, view);
    glow_disc(at, Vector3Scale(right, radius), Vector3Scale(up, radius), colour, 14);
}
void begin_additive() { rlDrawRenderBatchActive(); rlDisableDepthMask(); rlDisableBackfaceCulling(); BeginBlendMode(BLEND_ADDITIVE); }
void end_additive() { EndBlendMode(); rlDrawRenderBatchActive(); rlEnableBackfaceCulling(); rlEnableDepthMask(); }
Vector3 local(Vector3 origin, float yaw_degrees, float x, float y, float z) {
    const float a = yaw_degrees * DEG2RAD;
    return {origin.x + x * std::cos(a) + z * std::sin(a), origin.y + y, origin.z - x * std::sin(a) + z * std::cos(a)};
}
}

SkyState sky_at(float hour) {
    if (!std::isfinite(hour)) hour = 12;
    hour = std::fmod(hour, 24.0F);
    if (hour < 0) hour += 24;
    SkyState s;
    s.hour = hour;
    std::size_t k = 0;
    while (k + 2 < keys.size() && hour >= keys[k + 1].hour) ++k;
    const auto& a = keys[k];
    const auto& b = keys[k + 1];
    // Smoothstep between keys so colours ease rather than kink.
    float t = std::clamp((hour - a.hour) / (b.hour - a.hour), 0.0F, 1.0F);
    t = t * t * (3 - 2 * t);
    s.zenith = mix(a.zenith, b.zenith, t);
    s.horizon = mix(a.horizon, b.horizon, t);
    s.ambient = mix(a.ambient, b.ambient, t);
    s.daylight = a.daylight + (b.daylight - a.daylight) * t;
    s.lamps = std::clamp((.55F - s.daylight) / .4F, 0.0F, 1.0F);
    s.stars = std::clamp((.25F - s.daylight) / .25F, 0.0F, 1.0F);
    // The sun rises in the east (+x) at 06:00, is highest at noon and sets in the west at 18:00,
    // leaning a little south; the moon is opposite.
    const float phase = 2 * PI * (hour - 6) / 24;
    s.sun = Vector3Normalize({std::cos(phase), std::sin(phase), .35F});
    s.moon = Vector3Normalize({-std::cos(phase), -std::sin(phase), -.2F});
    s.sun_color = mix({255, 150, 80, 255}, {255, 250, 225, 255}, std::clamp(s.sun.y * 3, 0.0F, 1.0F));
    return s;
}

Color before_multiply(Color wanted, Color ambient) {
    const auto channel = [](unsigned char w, unsigned char a) {
        if (a == 0) return static_cast<unsigned char>(255);
        return static_cast<unsigned char>(std::clamp(w * 255 / static_cast<int>(a), 0, 255));
    };
    return {channel(wanted.r, ambient.r), channel(wanted.g, ambient.g), channel(wanted.b, ambient.b), 255};
}

std::vector<LightSource> collect_lights(const VerdaRegion& region) {
    std::vector<LightSource> lights;
    for (const auto& settlement : region.settlements()) for (const auto& asset : settlement.assets) {
        const auto& path = asset.model_path;
        if (path.find("Traffic lights/street_light") != std::string::npos) {
            // Head on the model's +x arm, 6.2 m up; the pool falls a little further out.
            LightSource light;
            light.bulb = local(asset.position, asset.rotation_y, .9F, 6.15F, 0);
            light.ground = local(asset.position, asset.rotation_y, 1.8F, 0, 0);
            light.radius = 11;
            lights.push_back(light);
        } else if (path.find("bonfire") != std::string::npos) {
            LightSource light;
            light.kind = LightKind::Fire;
            light.bulb = Vector3Add(asset.position, {0, .6F, 0});
            light.ground = asset.position;
            light.radius = 6;
            lights.push_back(light);
        }
    }
    return lights;
}

void draw_sky_objects(const SkyState& sky, Vector3 camera) {
    begin_additive();
    constexpr float distance = 700;
    if (sky.sun.y > -.08F) glow_sprite(Vector3Add(camera, Vector3Scale(sky.sun, distance)), 40, sky.sun_color, camera);
    if (sky.moon.y > -.05F && sky.daylight < .6F)
        glow_sprite(Vector3Add(camera, Vector3Scale(sky.moon, distance)), 18, scale({215, 222, 240, 255}, 1 - sky.daylight), camera);
    if (sky.stars > .01F) {
        for (std::uint32_t i = 0; i < 260; ++i) {
            // Upper hemisphere, fixed pattern.
            const float azimuth = unit(i * 3 + 1) * 2 * PI, elevation = std::asin(.08F + .92F * unit(i * 3 + 2));
            const Vector3 dir{std::cos(elevation) * std::cos(azimuth), std::sin(elevation), std::cos(elevation) * std::sin(azimuth)};
            const float twinkle = .55F + .45F * unit(i * 3 + 3);
            glow_sprite(Vector3Add(camera, Vector3Scale(dir, distance * .95F)), 1.6F + 1.6F * unit(i + 977), scale(WHITE, sky.stars * twinkle), camera);
        }
    }
    end_additive();
}

void draw_lights(const std::vector<LightSource>& lights, const SkyState& sky, Vector3 camera, float time_seconds) {
    begin_additive();
    for (std::size_t i = 0; i < lights.size(); ++i) {
        const auto& light = lights[i];
        if (Vector3DistanceSqr(light.ground, camera) > 240.0F * 240.0F) continue;
        if (light.kind == LightKind::StreetLamp) {
            if (sky.lamps < .01F) continue;
            const Vector3 pool{light.ground.x, light.ground.y + .12F, light.ground.z};
            glow_disc(pool, {light.radius, 0, 0}, {0, 0, light.radius}, scale({255, 196, 120, 255}, .85F * sky.lamps));
            glow_sprite(light.bulb, .9F, scale({255, 228, 170, 255}, sky.lamps), camera);
            glow_sprite(light.bulb, 2.6F, scale({255, 190, 110, 255}, .35F * sky.lamps), camera);
        } else {
            // Fires burn day and night; at night their light carries.
            const float flicker = .82F + .18F * std::sin(time_seconds * 11 + static_cast<float>(i)) * std::sin(time_seconds * 7.3F);
            const float strength = flicker * (.35F + .65F * (1 - sky.daylight));
            const Vector3 pool{light.ground.x, light.ground.y + .1F, light.ground.z};
            glow_disc(pool, {light.radius, 0, 0}, {0, 0, light.radius}, scale({255, 130, 50, 255}, .6F * strength));
            glow_sprite(light.bulb, 1.4F, scale({255, 150, 60, 255}, strength), camera);
        }
    }
    end_additive();
}

void draw_headlights(Vector3 car, float yaw_degrees, const SkyState& sky) {
    if (sky.lamps < .05F) return;
    begin_additive();
    const float a = yaw_degrees * DEG2RAD;
    const Vector3 forward{std::sin(a), 0, std::cos(a)}, side{std::cos(a), 0, -std::sin(a)};
    const Vector3 pool = Vector3Add(car, Vector3Add(Vector3Scale(forward, 10), {0, .12F, 0}));
    glow_disc(pool, Vector3Scale(forward, 9), Vector3Scale(side, 4.5F), scale({255, 245, 215, 255}, .5F * sky.lamps));
    end_additive();
}
}
