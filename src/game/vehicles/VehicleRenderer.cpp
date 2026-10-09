#include "outland/game/vehicles/VehicleRenderer.hpp"
#include <algorithm>
#include <cmath>
#include <raymath.h>
#include <rlgl.h>
namespace outland::game::vehicles {
void VehicleRenderer::draw(const VehicleSystem &system, const world::VerdaRegion &region,
                           Vector3 camera) {
    for (const auto &v : system.vehicles()) {
        const auto *a = system.asset(region, v.id);
        const auto *d = a ? system.definition(*a) : nullptr;
        if (!a || !d || !a->vehicle.enabled || Vector3DistanceSqr(a->position, camera) > 300 * 300)
            continue;
        auto *body = models_.load(
            d->body, [&](Model &model) { return VehicleRegistry::normalize(model, d->length); });
        if (!body)
            continue;
        const float scale = a->size.x / d->width;
        const Color tint = a->vehicle.destroyed ? Color{75, 65, 57, 255} : WHITE;
        rlPushMatrix();
        rlTranslatef(a->position.x, a->position.y, a->position.z);
        rlRotatef(a->rotation_y, 0, 1, 0);
        rlRotatef(-v.pitch, 1, 0, 0);
        rlRotatef(v.roll, 0, 0, 1);
        DrawModelEx(*body, {0, d->body_offset * scale, 0}, {0, 1, 0}, 0, {scale, scale, scale},
                    tint);
        if (!d->wheel.empty()) {
            auto *wheel = models_.load(d->wheel, [&](Model &model) {
                const auto b = assets::transformed_model_bounds(model);
                const float diameter = b.max.y - b.min.y;
                if (diameter <= .0001F)
                    return false;
                model.transform =
                    MatrixMultiply(model.transform, MatrixScale(d->wheel_radius * 2 / diameter,
                                                                d->wheel_radius * 2 / diameter,
                                                                d->wheel_radius * 2 / diameter));
                return true;
            });
            if (wheel) {
                Model centered = *wheel;
                centered.transform =
                    MatrixMultiply(centered.transform, MatrixTranslate(0, -d->wheel_radius, 0));
                const auto anchors = d->anchors();
                for (std::size_t i = 0; i < 4; ++i) {
                    const auto p = Vector3Scale(anchors[i], scale);
                    rlPushMatrix();
                    rlTranslatef(p.x, p.y, p.z);
                    if (i < 2)
                        rlRotatef(v.steering, 0, 1, 0);
                    rlRotatef(v.wheel_angle, 1, 0, 0);
                    DrawModelEx(centered, {0, 0, 0}, {0, 1, 0}, 0, {scale, scale, scale},
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
                             const combat::CombatWorld &collision) {
    const auto *v = system.driver();
    const auto *a = v ? system.asset(region, v->id) : nullptr;
    const auto *d = a ? system.definition(*a) : nullptr;
    if (!v || !a || !d) {
        following_ = false;
        return;
    }
    const float scale = a->size.x / d->width, speed = std::abs(v->speed) / d->top_speed;
    const auto target = Vector3Add(a->position, {0, 1.1F * scale, 0});
    const float yaw = (v->yaw + orbit) * DEG2RAD;
    Vector3 desired{target.x - std::sin(yaw) * (d->camera_distance + speed * 2) * scale,
                    target.y + (d->camera_height + pitch * 2) * scale,
                    target.z - std::cos(yaw) * (d->camera_distance + speed * 2) * scale};
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
