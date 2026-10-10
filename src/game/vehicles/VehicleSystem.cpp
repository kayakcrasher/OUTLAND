#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <algorithm>
#include <cmath>
#include <raymath.h>
#include <set>
#include <unordered_map>
namespace outland::game::vehicles {
namespace {
float distance(Vector3 a, Vector3 b) { return Vector3DistanceSqr(a, b); }
float scale_of(const world::WorldAsset &a, const VehicleDefinition &d) {
    return a.size.x / d.width;
}
bool finite(Vector3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
Vector3 local_point(Vector3 point, Vector3 origin, float yaw, float scale) {
    return Vector3Scale(
        Vector3RotateByAxisAngle(Vector3Subtract(point, origin), {0, 1, 0}, -yaw * DEG2RAD),
        1 / scale);
}
bool box(Vector3 a, Vector3 b, Vector3 lo, Vector3 hi, float &result) {
    float near = 0, far = 1;
    const auto delta = Vector3Subtract(b, a);
    const float start[]{a.x, a.y, a.z}, dir[]{delta.x, delta.y, delta.z}, min[]{lo.x, lo.y, lo.z},
        max[]{hi.x, hi.y, hi.z};
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(dir[axis]) < .00001F) {
            if (start[axis] < min[axis] || start[axis] > max[axis])
                return false;
        } else {
            float x = (min[axis] - start[axis]) / dir[axis],
                  y = (max[axis] - start[axis]) / dir[axis];
            if (x > y)
                std::swap(x, y);
            near = std::max(near, x);
            far = std::min(far, y);
            if (near > far)
                return false;
        }
    }
    result = near;
    return true;
}
} // namespace
Vector3 VehicleSystem::world_point(Vector3 local, Vector3 origin, float yaw, float scale) {
    return Vector3Add(
        origin, Vector3RotateByAxisAngle(Vector3Scale(local, scale), {0, 1, 0}, yaw * DEG2RAD));
}
world::WorldAsset *VehicleSystem::asset(world::VerdaRegion &r, std::string_view id) const {
    if (owner_ == &r) {
        auto found = assets_.find(std::string(id));
        return found == assets_.end() ? nullptr : found->second;
    }
    for (auto &s : r.runtime_settlements())
        for (auto &a : s.assets)
            if (a.id == id)
                return &a;
    return nullptr;
}
const world::WorldAsset *VehicleSystem::asset(const world::VerdaRegion &r,
                                              std::string_view id) const {
    if (owner_ == &r) {
        auto found = assets_.find(std::string(id));
        return found == assets_.end() ? nullptr : found->second;
    }
    for (const auto &s : r.settlements())
        for (const auto &a : s.assets)
            if (a.id == id)
                return &a;
    return nullptr;
}
const VehicleRuntime *VehicleSystem::driver() const {
    for (const auto &v : vehicles_)
        if (v.id == driver_id_)
            return &v;
    return nullptr;
}
bool VehicleSystem::spawn(world::VerdaRegion &region, Vector3 near, float yaw,
                          std::string_view id) {
    const auto *d = registry_.find(id);
    if (!d || !finite(near) || !std::isfinite(yaw) || region.settlements().empty() ||
        vehicles_.size() >= 128)
        return false;
    for (float offset : {4.0F, 6.0F, 8.0F})
        for (float side : {1.0F, -1.0F}) {
            Vector3 p = world_point({side * offset, 0, 2}, near, yaw);
            p.y = world::terrain::TerrainHeight::sample(p.x, p.z);
            if (blocked(region, p, yaw, *d, 1))
                continue;
            world::WorldAsset a;
            do {
                a.id = "creator_vehicle_" + std::to_string(++spawn_counter_);
            } while (asset(region, a.id));
            a.model_path = d->body;
            a.position = p;
            a.rotation_y = yaw;
            a.size = {d->width, d->height, d->length};
            a.vehicle.definition = d->id;
            a.vehicle.home = p;
            a.vehicle.home_yaw = yaw;
            a.vehicle.fuel = d->fuel;
            auto &sites = region.runtime_settlements();
            auto closest =
                std::min_element(sites.begin(), sites.end(), [&](const auto &x, const auto &y) {
                    return distance(x.center, p) < distance(y.center, p);
                });
            closest->assets.push_back(a);
            reconcile(region);
            return true;
        }
    return false;
}
bool VehicleSystem::reconcile(world::VerdaRegion &region) {
    bool changed = false;
    std::size_t placed_count = 0;
    for (const auto &s : region.settlements())
        for (const auto &a : s.assets)
            if (!a.vehicle.definition.empty())
                ++placed_count;
    for (auto &s : region.runtime_settlements()) {
        std::set<std::string> markers;
        for (const auto &m : s.gameplay_markers)
            if (m.type == world::GameplayMarkerType::VehicleSpawn) {
                markers.insert(m.id);
                const std::string id = "vehicle_marker_" + s.id + "_" + m.id;
                auto found = std::find_if(s.assets.begin(), s.assets.end(),
                                          [&](const auto &a) { return a.id == id; });
                if (found == s.assets.end() && m.enabled && placed_count < 128) {
                    const VehicleDefinition *d = registry_.find("hatchback");
                    for (const auto &definition : registry_.definitions())
                        if (m.id.find("vehicle_spawn_" + definition.id + "_") != std::string::npos)
                            d = &definition;
                    if (!d)
                        continue;
                    world::WorldAsset a;
                    a.id = id;
                    a.model_path = d->body;
                    a.position = m.position;
                    a.position.y =
                        world::terrain::TerrainHeight::sample(m.position.x, m.position.z);
                    a.rotation_y = m.rotation_y;
                    const float scale = std::clamp(m.size.x / 3.0F, .25F, 4.0F);
                    a.size = {d->width * scale, d->height * scale, d->length * scale};
                    a.vehicle.definition = d->id;
                    a.vehicle.marker = m.id;
                    a.vehicle.home = m.position;
                    a.vehicle.home_yaw = m.rotation_y;
                    s.assets.push_back(a);
                    ++placed_count;
                    changed = true;
                } else if (found != s.assets.end()) {
                    if (distance(found->vehicle.home, m.position) > .001F ||
                        found->vehicle.home_yaw != m.rotation_y) {
                        found->position = m.position;
                        found->position.y =
                            world::terrain::TerrainHeight::sample(m.position.x, m.position.z);
                        found->rotation_y = m.rotation_y;
                        found->vehicle.home = m.position;
                        found->vehicle.home_yaw = m.rotation_y;
                        changed = true;
                    }
                    if (found->vehicle.enabled != m.enabled) {
                        found->vehicle.enabled = m.enabled;
                        changed = true;
                    }
                }
            }
        const auto before = s.assets.size();
        std::erase_if(s.assets, [&](const auto &a) {
            return !a.vehicle.marker.empty() && !markers.contains(a.vehicle.marker);
        });
        changed |= before != s.assets.size();
    }
    std::unordered_map<std::string, VehicleRuntime> previous;
    for (const auto &v : vehicles_)
        previous.emplace(v.id, v);
    owner_ = &region;
    assets_.clear();
    for (auto &s : region.runtime_settlements())
        for (auto &a : s.assets)
            if (!a.vehicle.definition.empty())
                assets_.emplace(a.id, &a);
    vehicles_.clear();
    for (const auto &s : region.settlements())
        for (const auto &a : s.assets)
            if (!a.vehicle.definition.empty() && registry_.find(a.vehicle.definition) &&
                vehicles_.size() < 128) {
                auto it = previous.find(a.id);
                VehicleRuntime v = it == previous.end() ? VehicleRuntime{} : it->second;
                v.id = a.id;
                if (distance(v.position, a.position) > .001F || v.yaw != a.rotation_y) {
                    v.speed = 0;
                    v.position = a.position;
                    v.yaw = a.rotation_y;
                }
                vehicles_.push_back(v);
            }
    if (!driver_id_.empty() && !driver()) {
        driver_id_.clear();
        events_.stopped = true;
    }
    return changed;
}
int VehicleSystem::nearest(const world::VerdaRegion &r, Vector3 p, float radius) const {
    int result = -1;
    float best = radius * radius;
    for (std::size_t i = 0; i < vehicles_.size(); ++i) {
        const auto *a = asset(r, vehicles_[i].id);
        if (!a || !a->vehicle.enabled || a->vehicle.destroyed)
            continue;
        // Distance to the car body: standing at a door or bumper counts, not only near its centre.
        float d = distance(p, a->position);
        if (const auto *def = definition(*a)) {
            const float scale = scale_of(*a, *def);
            const auto local = Vector3RotateByAxisAngle(Vector3Subtract(p, a->position), {0, 1, 0}, -a->rotation_y * DEG2RAD);
            const float dx = std::max(0.0F, std::abs(local.x) - def->width * scale * .5F),
                        dz = std::max(0.0F, std::abs(local.z) - def->length * scale * .5F);
            d = dx * dx + dz * dz;
        }
        if (d < best) {
            best = d;
            result = static_cast<int>(i);
        }
    }
    return result;
}
bool VehicleSystem::enter(world::VerdaRegion &r, int index, Vector3 p) {
    if (driver() || index < 0 || static_cast<std::size_t>(index) >= vehicles_.size())
        return false;
    auto *a = asset(r, vehicles_[index].id);
    if (!a || !a->vehicle.enabled || a->vehicle.destroyed || a->vehicle.engine <= 0 ||
        distance(p, a->position) > 49) // nearest() already measures to the body
        return false;
    // Test the approach against the same world collision used by pedestrians, up to the body:
    // whatever is under the car itself does not stand between the player and the door.
    const auto *def = definition(*a);
    const float half_w = def ? def->width * scale_of(*a, *def) * .5F : 0,
                half_l = def ? def->length * scale_of(*a, *def) * .5F : 0;
    for (int step = 1; step < 8; ++step) {
        auto point = Vector3Lerp(p, a->position, step / 8.0F);
        const auto local = Vector3RotateByAxisAngle(Vector3Subtract(point, a->position), {0, 1, 0}, -a->rotation_y * DEG2RAD);
        if (std::abs(local.x) < half_w && std::abs(local.z) < half_l)
            break;
        if (world::physics::WorldCollision::blocked(point, r, .15F, a->id))
            return false;
    }
    driver_id_ = a->id;
    events_.started = events_.door = true;
    return true;
}
Vector3 VehicleSystem::seat(const world::VerdaRegion &r) const {
    const auto *v = driver();
    if (!v)
        return {};
    const auto *a = asset(r, v->id);
    const auto *d = a ? definition(*a) : nullptr;
    return d ? world_point(d->seat, a->position, a->rotation_y, scale_of(*a, *d)) : v->position;
}
bool VehicleSystem::exit(world::VerdaRegion &r, Vector3 &destination, bool forced) {
    const auto *v = driver();
    if (!v)
        return false;
    const auto *a = asset(r, v->id);
    const auto *d = a ? definition(*a) : nullptr;
    if (!a || !d)
        return false;
    if (!forced && std::abs(v->speed) > 1.5F)
        return false;
    for (float radius : {1.0F, 1.5F, 2.0F})
        for (Vector3 local :
             {d->exit, {-d->exit.x, 0, d->exit.z}, {0, 0, -d->length * .5F - 1.2F}}) {
            auto p = world_point(Vector3Scale(local, radius), a->position, a->rotation_y,
                                 scale_of(*a, *d));
            p.y = world::terrain::TerrainHeight::sample(p.x, p.z);
            if (world::physics::WorldCollision::blocked(p, r, .45F))
                continue;
            destination = p;
            for (auto &runtime : vehicles_)
                if (runtime.id == driver_id_)
                    runtime.speed = 0;
            driver_id_.clear();
            events_.stopped = events_.door = true;
            return true;
        }
    return false;
}
bool VehicleSystem::blocked(const world::VerdaRegion &r, Vector3 p, float yaw,
                            const VehicleDefinition &d, float scale,
                            std::string_view ignore) const {
    // The existing central inspection structure is solid to a vehicle footprint.
    // Its conservative broad phase runs only near the capital's origin.
    if (training_structure_ && std::abs(p.x) < 2 + d.length * scale && std::abs(p.z) < 2 + d.length * scale) {
        const float yaw_r = -yaw * DEG2RAD;
        const auto origin =
            Vector3RotateByAxisAngle(Vector3Scale(Vector3Negate(p), 1 / scale), {0, 1, 0}, yaw_r);
        if (std::abs(origin.x) < d.width * .5F + 2 / scale &&
            std::abs(origin.z) < d.length * .5F + 2 / scale)
            return true;
    }
    float heights[3][3]{};
    int row = 0;
    for (float x : {-d.width * .5F, 0.0F, d.width * .5F}) {
        int column = 0;
        for (float z : {-d.length * .5F, 0.0F, d.length * .5F}) {
            const auto point = world_point({x, 0, z}, p, yaw, scale);
            heights[row][column++] = world::terrain::TerrainHeight::sample(point.x, point.z);
            if (world::physics::WorldCollision::blocked(point, r, .2F, ignore))
                return true;
        }
        ++row;
    }
    const float front = (heights[0][2] + heights[2][2]) * .5F,
                rear = (heights[0][0] + heights[2][0]) * .5F;
    const float pitch = (front - rear) / (d.length * scale),
                roll = (heights[2][1] - heights[0][1]) / (d.width * scale);
    return std::atan(std::hypot(pitch, roll)) * RAD2DEG > d.max_slope;
}

bool VehicleSystem::update(float dt, VehicleInput input, world::VerdaRegion &r, Vector3 player,
                           bool paused) {
    if (!std::isfinite(dt) || dt <= 0)
        return false;
    dt = std::min(dt, .1F);
    bool changed = false;
    events_.horn = input.horn;
    events_.parking = input.parking;
    for (auto &v : vehicles_) {
        auto *a = asset(r, v.id);
        if (!a)
            continue;
        const bool driving = v.id == driver_id_;
        v.sleeping = !driving && (paused || distance(a->position, player) > 180 * 180 ||
                                  std::abs(v.speed) < .02F);
        if (v.sleeping)
            continue;
        const auto *d = definition(*a);
        if (!d)
            continue;
        const float scale = scale_of(*a, *d);
        if (!std::isfinite(scale) || scale <= 0)
            continue;
        VehicleInput controls =
            driving && !paused && !a->vehicle.destroyed ? input : VehicleInput{};
        if (!std::isfinite(controls.throttle))
            controls.throttle = 0;
        if (!std::isfinite(controls.steer))
            controls.steer = 0;
        if (paused || a->vehicle.destroyed)
            controls.brake = true;
        const float tire =
            (*std::min_element(a->vehicle.tires.begin(), a->vehicle.tires.end())) / 100;
        const float performance =
            std::clamp(a->vehicle.engine / 100, .0F, 1.0F) * (.45F + .55F * tire);
        const float old_speed = v.speed;
        const float throttle = std::clamp(controls.throttle, -1.0F, 1.0F);
        const bool opposing = v.speed * throttle < -.2F;
        float acceleration = throttle * d->acceleration * performance;
        if (a->vehicle.fuel <= 0)
            acceleration = 0;
        if (opposing)
            acceleration = throttle * d->brake;
        v.speed += acceleration * dt;
        const float resistance = d->drag + std::abs(v.speed) * .035F +
                                 (controls.brake ? d->brake : 0) +
                                 (controls.parking ? d->brake * 1.8F : 0);
        v.speed = std::copysign(std::max(0.0F, std::abs(v.speed) - resistance * dt), v.speed);
        v.speed = std::clamp(v.speed, -d->reverse * performance, d->top_speed * performance);
        const float target =
            std::clamp(controls.steer, -1.0F, 1.0F) * d->steer / (1 + std::abs(v.speed) * .04F);
        v.steering += (target - v.steering) * std::min(1.0F, dt * 8);
        const float yaw_delta = v.speed / std::max(.5F, (d->front_z - d->rear_z) * scale) *
                                std::tan(v.steering * DEG2RAD) * RAD2DEG * dt;
        float yaw = std::remainder(v.yaw + yaw_delta, 360.0F);
        const int steps =
            std::clamp(static_cast<int>(std::ceil(std::abs(v.speed * dt) / .4F)), 1, 12);
        // A car that already overlaps something (placed on a pole, or a world edit landed on it)
        // gets one body length to drive clear of it; otherwise every move would be refused.
        if (v.escape <= 0 && std::abs(v.speed) > .001F && blocked(r, a->position, a->rotation_y, *d, scale, a->id))
            v.escape = d->length * scale + .5F;
        for (int step = 0; step < steps; ++step) {
            auto p = world_point({0, 0, v.speed * dt / steps}, a->position, yaw);
            p.y = world::terrain::TerrainHeight::sample(p.x, p.z);
            if (v.escape > 0) v.escape -= std::abs(v.speed * dt / steps);
            else if (blocked(r, p, yaw, *d, scale, a->id)) {
                a->vehicle.health = std::max(0.0F, a->vehicle.health - std::abs(v.speed) * .4F);
                v.speed = 0;
                events_.collision = true;
                changed = true;
                break;
            }
            a->position = p;
            a->rotation_y = yaw;
            changed |= std::abs(v.speed) > .001F;
        }
        v.position = a->position;
        v.yaw = a->rotation_y;
        v.wheel_angle = std::remainder(
            v.wheel_angle + v.speed * dt / (d->wheel_radius * scale) * RAD2DEG, 360.0F);
        const auto front = world_point({0, 0, d->front_z}, a->position, v.yaw, scale),
                   rear = world_point({0, 0, d->rear_z}, a->position, v.yaw, scale);
        const auto left = world_point({-d->track * .5F, 0, 0}, a->position, v.yaw, scale),
                   right = world_point({d->track * .5F, 0, 0}, a->position, v.yaw, scale);
        const auto height = [](Vector3 p) {
            return world::terrain::TerrainHeight::sample(p.x, p.z);
        };
        v.pitch =
            std::atan2(height(front) - height(rear), (d->front_z - d->rear_z) * scale) * RAD2DEG +
            std::clamp((v.speed - old_speed) / dt * .15F, -3.0F, 3.0F);
        v.roll = std::atan2(height(right) - height(left), d->track * scale) * RAD2DEG +
                 std::clamp(v.speed * yaw_delta * .025F, -4.0F, 4.0F) * (1 + .5F * (1 - tire));
        if (fuel_hook_ && driving) {
            const float used = fuel_hook_(a->id, std::abs(v.speed) * dt);
            if (std::isfinite(used) && used > 0) {
                a->vehicle.fuel = std::max(0.0F, a->vehicle.fuel - used);
                changed = true;
            }
        }
        if (a->vehicle.health <= 0) {
            a->vehicle.destroyed = true;
            a->vehicle.engine = 0;
            changed = true;
        }
    }
    return changed;
}
combat::BulletHit VehicleSystem::trace(const world::VerdaRegion &r, Vector3 start, Vector3 end,
                                       bool occupants) const {
    combat::BulletHit best;
    const auto delta = Vector3Subtract(end, start);
    for (std::size_t i = 0; i < vehicles_.size(); ++i) {
        const auto *a = asset(r, vehicles_[i].id);
        const auto *d = a ? definition(*a) : nullptr;
        if (!a || !d || !a->vehicle.enabled)
            continue;
        if (distance(a->position, start) > 400 * 400 && distance(a->position, end) > 400 * 400)
            continue;
        const float scale = scale_of(*a, *d);
        const auto s = local_point(start, a->position, a->rotation_y, scale),
                   e = local_point(end, a->position, a->rotation_y, scale);
        auto record = [&](Vector3 lo, Vector3 hi, VehicleZone zone, float penetration = 0) {
            float t = 0;
            if (box(s, e, lo, hi, t) && t < best.fraction &&
                !(zone == VehicleZone::Window && t < .00001F)) {
                best = {zone == VehicleZone::Window     ? combat::HitKind::VehicleWindow
                        : zone == VehicleZone::Occupant ? combat::HitKind::VehicleOccupant
                                                        : combat::HitKind::Vehicle,
                        t,
                        Vector3Add(start, Vector3Scale(delta, t)),
                        {0, 1, 0},
                        static_cast<int>(i),
                        false,
                        static_cast<int>(zone),
                        penetration};
            }
        };
        const float w = d->width * .5F, l = d->length * .5F;
        record({-w, .3F, -l}, {w, .88F, l * .55F}, VehicleZone::Body);
        record({-w, .88F, -l}, {w, .9F, -l * .5F}, VehicleZone::Body);
        record({-w, .3F, l * .55F}, {w, .88F, l}, VehicleZone::Engine);
        record({-w, d->height - .15F, -l * .5F}, {w, d->height, l * .55F}, VehicleZone::Body);
        // Logical window shell, independent of opaque/fused visual window materials.
        for (float side : {-1.0F, 1.0F})
            record({side < 0 ? -w : w - .08F, .88F, -l * .5F},
                   {side < 0 ? -w + .08F : w, d->height - .15F, l * .55F}, VehicleZone::Window,
                   .7F);
        record({-w, .88F, l * .55F - .09F}, {w, d->height - .15F, l * .55F}, VehicleZone::Window,
               .7F);
        record({-w, .88F, -l * .5F}, {w, d->height - .15F, -l * .5F + .09F}, VehicleZone::Window,
               .7F);
        const auto anchors = d->anchors();
        for (std::size_t tire = 0; tire < 4; ++tire) {
            const auto c = anchors[tire];
            record(Vector3Subtract(c, {.15F, d->wheel_radius, d->wheel_radius}),
                   Vector3Add(c, {.15F, d->wheel_radius, d->wheel_radius}),
                   static_cast<VehicleZone>(static_cast<int>(VehicleZone::Tire0) + tire));
        }
        if (occupants && a->id == driver_id_)
            record(Vector3Subtract(d->seat, {.24F, .25F, .22F}),
                   Vector3Add(d->seat, {.24F, .4F, .22F}), VehicleZone::Occupant);
    }
    return best;
}
void VehicleSystem::damage(world::VerdaRegion &r, const combat::BulletHit &hit, float amount) {
    if (!std::isfinite(amount) || amount <= 0 || hit.target < 0 ||
        static_cast<std::size_t>(hit.target) >= vehicles_.size())
        return;
    auto *a = asset(r, vehicles_[hit.target].id);
    if (!a)
        return;
    const auto *d = definition(*a);
    if (!d)
        return;
    const auto zone = static_cast<VehicleZone>(hit.zone);
    if (zone == VehicleZone::Occupant) {
        if (occupant_damage_)
            occupant_damage_(amount);
        return;
    }
    if (zone == VehicleZone::Window)
        return;
    if (zone >= VehicleZone::Tire0 && zone <= VehicleZone::Tire3) {
        auto &tire =
            a->vehicle.tires[static_cast<int>(zone) - static_cast<int>(VehicleZone::Tire0)];
        tire = std::max(0.0F, tire - amount * 100 / d->tire_health);
    } else {
        a->vehicle.health = std::max(0.0F, a->vehicle.health - amount * 100 / d->health);
        if (zone == VehicleZone::Engine)
            a->vehicle.engine = std::max(0.0F, a->vehicle.engine - amount * 150 / d->engine_health);
    }
    if (a->vehicle.health <= 0) {
        a->vehicle.destroyed = true;
        a->vehicle.engine = 0;
    }
}
} // namespace outland::game::vehicles
