#include "outland/game/vehicles/VehicleRenderer.hpp"
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <cmath>
#include <raymath.h>
#include <rlgl.h>
namespace outland::game::vehicles {
namespace {
// A repaint: the body's own paint is the commonest vertex colour (the generators build bodies
// in one paint and a darker shade of it); every vertex of that hue takes the new colour at its
// own brightness. Glass, chrome, tyres and lamps keep theirs. A textured body is tinted instead.
bool repaint(Model& model, Color paint) {
    bool coloured = false;
    for (int m = 0; m < model.meshCount; ++m) {
        auto& mesh = model.meshes[m];
        if (!mesh.colors || mesh.vertexCount <= 0) continue;
        coloured = true;
        std::unordered_map<std::uint32_t, int> counts;
        for (int i = 0; i < mesh.vertexCount; ++i) {
            const unsigned char* c = mesh.colors + i * 4;
            const int r = c[0], g = c[1], b = c[2];
            // Paint has some colour or some brightness; skip near-black trim and glass.
            if (r + g + b < 90 || c[3] < 250) continue;
            ++counts[static_cast<std::uint32_t>(r) << 16 | static_cast<std::uint32_t>(g) << 8 | static_cast<std::uint32_t>(b)];
        }
        std::uint32_t top = 0;
        int best = 0;
        for (const auto& [key, n] : counts) if (n > best) { best = n; top = key; }
        if (best == 0) continue;
        const float pr = static_cast<float>(top >> 16 & 255), pg = static_cast<float>(top >> 8 & 255), pb = static_cast<float>(top & 255);
        const float plen = std::sqrt(pr * pr + pg * pg + pb * pb);
        for (int i = 0; i < mesh.vertexCount; ++i) {
            unsigned char* c = mesh.colors + i * 4;
            const float r = c[0], g = c[1], b = c[2], len = std::sqrt(r * r + g * g + b * b);
            if (len < 1 || c[3] < 250) continue;
            const float along = (r * pr + g * pg + b * pb) / (len * plen), shade = len / plen;
            if (along < .995F || shade < .45F || shade > 1.25F) continue;
            c[0] = static_cast<unsigned char>(std::clamp(paint.r * shade, 0.0F, 255.0F));
            c[1] = static_cast<unsigned char>(std::clamp(paint.g * shade, 0.0F, 255.0F));
            c[2] = static_cast<unsigned char>(std::clamp(paint.b * shade, 0.0F, 255.0F));
        }
        if (mesh.vboId && mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR])
            UpdateMeshBuffer(mesh, RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, mesh.colors, mesh.vertexCount * 4, 0);
    }
    if (!coloured)
        // A textured body: the same, on the texture's pixels (the paint is its commonest colour).
        for (int k = 0; k < model.materialCount; ++k) {
            auto& map = model.materials[k].maps[MATERIAL_MAP_DIFFUSE];
            if (map.texture.id == 0 || map.texture.id == rlGetTextureIdDefault()) { map.color = paint; continue; }
            Image image = LoadImageFromTexture(map.texture);
            if (!image.data) { map.color = paint; continue; }
            ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            auto* px = static_cast<unsigned char*>(image.data);
            const int count = image.width * image.height;
            std::unordered_map<std::uint32_t, int> counts;
            for (int i = 0; i < count; ++i) {
                const unsigned char* c = px + i * 4;
                if (c[0] + c[1] + c[2] < 90) continue;
                ++counts[static_cast<std::uint32_t>(c[0] >> 3) << 10 | static_cast<std::uint32_t>(c[1] >> 3) << 5 | static_cast<std::uint32_t>(c[2] >> 3)];
            }
            std::uint32_t top = 0;
            int best = 0;
            for (const auto& [key, n] : counts) if (n > best) { best = n; top = key; }
            if (best > 0) {
                const float pr = static_cast<float>((top >> 10 & 31) * 8 + 4), pg = static_cast<float>((top >> 5 & 31) * 8 + 4), pb = static_cast<float>((top & 31) * 8 + 4);
                const float plen = std::sqrt(pr * pr + pg * pg + pb * pb);
                for (int i = 0; i < count; ++i) {
                    unsigned char* c = px + i * 4;
                    const float r = c[0], g = c[1], b = c[2], len = std::sqrt(r * r + g * g + b * b);
                    if (len < 1) continue;
                    const float along = (r * pr + g * pg + b * pb) / (len * plen), shade = len / plen;
                    if (along < .985F || shade < .4F || shade > 1.3F) continue;
                    c[0] = static_cast<unsigned char>(std::clamp(paint.r * shade, 0.0F, 255.0F));
                    c[1] = static_cast<unsigned char>(std::clamp(paint.g * shade, 0.0F, 255.0F));
                    c[2] = static_cast<unsigned char>(std::clamp(paint.b * shade, 0.0F, 255.0F));
                }
                const Texture2D fresh = LoadTextureFromImage(image);
                if (fresh.id != 0) { UnloadTexture(map.texture); map.texture = fresh; }
            }
            UnloadImage(image);
        }
    return true;
}
// A front bumper on the car's nose (local metres, before scale): a steel push bar, or a chrome
// bull bar up over the lamps.
void draw_bumper(int kind, const VehicleDefinition& d) {
    if (kind <= 0) return;
    const float z = d.front_z + .18F, w = d.width * .82F;
    const Color steel{34, 34, 36, 255}, chrome{196, 198, 202, 255};
    const Color c = kind == 1 ? steel : chrome;
    const float low = d.height * .2F, high = kind == 1 ? d.height * .38F : d.height * .55F;
    DrawCube({0, low, z}, w, .1F, .1F, c);
    DrawCube({0, high, z}, w * (kind == 1 ? .7F : .9F), .09F, .09F, c);
    for (const float x : {-w * .32F, w * .32F}) DrawCube({x, (low + high) * .5F, z}, .08F, high - low, .08F, c);
    if (kind == 2) {
        for (const float x : {-w * .5F, w * .5F}) DrawCube({x, (low + high) * .5F, z - .05F}, .07F, high - low, .07F, c);
        DrawCube({0, low - .06F, z + .05F}, w * .5F, .12F, .12F, c);
    }
}
}

void VehicleRenderer::draw(const VehicleSystem &system, const world::VerdaRegion &region,
                           Vector3 camera) {
    for (const auto &v : system.vehicles()) {
        const auto *a = system.asset(region, v.id);
        const auto *d = a ? system.definition(*a) : nullptr;
        if (!a || !d || !a->vehicle.enabled || Vector3DistanceSqr(a->position, camera) > 300 * 300)
            continue;
        const int paint = a->vehicle.painted ? a->vehicle.mods[0] : -1;
        if (paint >= 0 && paint < static_cast<int>(painted_.size()) && !painted_[static_cast<std::size_t>(paint)])
            painted_[static_cast<std::size_t>(paint)] = std::make_unique<assets::ModelCache>(16);
        auto *body = paint >= 0 && paint < static_cast<int>(painted_.size())
            ? painted_[static_cast<std::size_t>(paint)]->load(d->body, [&](Model &model) {
                  return VehicleRegistry::normalize(model, d->length) && repaint(model, a->vehicle.paint);
              })
            : models_.load(d->body, [&](Model &model) { return VehicleRegistry::normalize(model, d->length); });
        if (!body)
            continue;
        const float scale = a->size.x / d->width;
        const bool donor = parts_donor(a->vehicle.marker);
        const Color tint = a->vehicle.destroyed ? Color{75, 65, 57, 255} : donor ? Color{176, 150, 128, 255} : WHITE;
        rlPushMatrix();
        rlTranslatef(a->position.x, a->position.y, a->position.z);
        rlRotatef(a->rotation_y, 0, 1, 0);
        rlRotatef(-v.pitch, 1, 0, 0);
        rlRotatef(v.roll, 0, 0, 1);
        // Bigger (or smaller) tyres lift (or drop) the body with them.
        const float lift = (a->vehicle.wheel - 1) * d->wheel_radius * scale;
        DrawModelEx(*body, {0, d->body_offset * scale + lift, 0}, {0, 1, 0}, 0, {scale, scale, scale},
                    tint);
        if (a->vehicle.mods[3] > 0) {
            rlPushMatrix();
            rlTranslatef(0, lift, 0);
            rlScalef(scale, scale, scale);
            draw_bumper(a->vehicle.mods[3], *d);
            rlPopMatrix();
        }
        if (!d->wheel.empty()) {
            // Cached at a 1 m diameter (vehicles share wheel models) and sized per vehicle below.
            auto *wheel = models_.load(d->wheel, [&](Model &model) {
                const auto b = assets::transformed_model_bounds(model);
                const float diameter = b.max.y - b.min.y;
                if (diameter <= .0001F)
                    return false;
                model.transform = MatrixMultiply(model.transform, MatrixScale(1 / diameter, 1 / diameter, 1 / diameter));
                return true;
            });
            if (wheel) {
                Model centered = *wheel;
                centered.transform = MatrixMultiply(centered.transform, MatrixTranslate(0, -.5F, 0));
                const float wheel_scale = scale * d->wheel_radius * 2 * a->vehicle.wheel;
                const auto anchors = d->anchors();
                for (std::size_t i = 0; i < 4; ++i) {
                    auto p = Vector3Scale(anchors[i], scale);
                    p.y += lift;
                    if (donor && a->vehicle.tires[i] <= 0) {
                        // Wheel gone: the axle rests on stacked cinder blocks.
                        const float h = d->wheel_radius * scale * 1.6F;
                        DrawCube({p.x, h * .5F, p.z}, .38F, h, .3F, Color{128, 126, 120, 255});
                        DrawCubeWires({p.x, h * .5F, p.z}, .38F, h, .3F, Color{90, 88, 84, 255});
                        continue;
                    }
                    rlPushMatrix();
                    rlTranslatef(p.x, p.y, p.z);
                    if (i < 2)
                        rlRotatef(v.steering, 0, 1, 0);
                    rlRotatef(v.wheel_angle, 1, 0, 0);
                    DrawModelEx(centered, {0, 0, 0}, {0, 1, 0}, 0, {wheel_scale, wheel_scale, wheel_scale},
                                a->vehicle.tires[i] > 0 ? tint : Color{65, 65, 65, 255});
                    rlPopMatrix();
                }
            }
        }
        rlPopMatrix();
    }
}
void VehicleRenderer::camera(Camera3D &camera, const VehicleSystem &system,
                             const world::VerdaRegion &region, float dt, float orbit, float pitch,
                             const combat::CombatWorld &collision, std::string_view follow) {
    const auto *v = system.driver();
    if (!v && !follow.empty())
        if (const int i = system.find(follow); i >= 0) v = &system.vehicles()[static_cast<std::size_t>(i)];
    const auto *a = v ? system.asset(region, v->id) : nullptr;
    const auto *d = a ? system.definition(*a) : nullptr;
    if (!v || !a || !d) {
        following_ = false;
        return;
    }
    const float scale = a->size.x / d->width, speed = std::abs(v->speed) / d->top_speed;
    // A big vehicle (a rig, a bus) is looked at from over its cab, a little ahead of its centre,
    // so the road in front stays in view; a trailer pushes the camera back and up over it.
    const float big = std::clamp((d->length - 6) / 4, 0.0F, 1.0F);
    const float heading = v->yaw * DEG2RAD;
    const auto target = Vector3Add(a->position, {std::sin(heading) * d->length * .3F * big * scale, (1.1F + big * 1.6F) * scale,
                                                 std::cos(heading) * d->length * .3F * big * scale});
    const float yaw = (v->yaw + orbit) * DEG2RAD;
    const float back = d->camera_distance + trailer_ * .8F + speed * 2, up = d->camera_height + trailer_ * .3F + pitch * 2;
    Vector3 desired{target.x - std::sin(yaw) * back * scale, target.y + up * scale, target.z - std::cos(yaw) * back * scale};
    const auto obstruction = collision.trace_segment(target, desired, false, false, false);
    if (obstruction.hit() && obstruction.kind != combat::HitKind::Vehicle &&
        obstruction.kind != combat::HitKind::VehicleWindow)
        desired = Vector3Lerp(target, desired, std::max(.15F, obstruction.fraction - .05F));
    const float t = 1 - std::exp(-dt * (7 + speed * 3));
    follow_position_ = following_ ? Vector3Lerp(follow_position_, desired, t) : desired;
    follow_target_ = following_ ? Vector3Lerp(follow_target_, target, t) : target;
    camera.position = follow_position_;
    camera.target = follow_target_;
    camera.up = {0, 1, 0};
    camera.fovy = 70 + speed * 10;
    following_ = true;
}
} // namespace outland::game::vehicles
