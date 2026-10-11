#include "outland/game/ai/BotDrivers.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::ai {

namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
bool usable(const vehicles::VehicleSystem& cars, const world::VerdaRegion& region, const vehicles::VehicleRuntime& car) {
    if (car.id.find("_donor_") != std::string::npos || car.id.find("bus") != std::string::npos || cars.kinematic(car.id)) return false;
    const auto* asset = cars.asset(region, car.id);
    return asset && asset->vehicle.enabled && !asset->vehicle.destroyed && asset->vehicle.fuel > .15F && asset->vehicle.health > 30;
}
}

Vector3 BotDrivers::zone_goal(Vector3 from, float zone_radius) {
    // Well inside the circle along the line to its centre: the circle shrinks on the capital.
    const float distance = std::hypot(from.x, from.z), inside = std::max(60.0F, zone_radius * .35F);
    if (distance <= inside) return {0, 0, 0};
    const float keep = inside / distance;
    return {from.x * keep, 0, from.z * keep};
}

int BotDrivers::start(BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                      std::uint64_t seed, float zone_radius, int max_drivers) {
    rides_.clear();
    roads_.build(region);
    std::vector<std::string> taken;
    std::uint64_t state = seed | 1;
    // Every other bot in drop order is a candidate, from a seeded starting point.
    const auto& all = bots.bots();
    if (all.empty()) return 0;
    state ^= state << 13; state ^= state >> 7; state ^= state << 17;
    const std::size_t first = static_cast<std::size_t>(state % all.size());
    for (std::size_t k = 0; k < all.size() && static_cast<int>(rides_.size()) < max_drivers; ++k) {
        const auto& bot = all[(first + k) % all.size()];
        if (!bot.alive()) continue;
        int best = -1;
        float nearest = 140;
        for (std::size_t i = 0; i < cars.vehicles().size(); ++i) {
            const auto& car = cars.vehicles()[i];
            if (!usable(cars, region, car) || std::find(taken.begin(), taken.end(), car.id) != taken.end()) continue;
            const float d = flat(car.position, bot.position());
            if (d < nearest) { nearest = d; best = static_cast<int>(i); }
        }
        if (best < 0) continue;
        const auto& car = cars.vehicles()[static_cast<std::size_t>(best)];
        BotRide ride;
        ride.bot = bot.id();
        ride.vehicle = car.id;
        ride.route = roads_.route(car.position, zone_goal(car.position, zone_radius));
        if (!ride.route.complete || ride.route.length < 80) continue;
        ride.active = true;
        taken.push_back(car.id);
        bots.set_mounted(bot.id(), true);
        if (auto* driver = bots.find(bot.id())) { driver->set_position(car.position); driver->set_yaw(car.yaw); }
        rides_.push_back(std::move(ride));
    }
    return static_cast<int>(rides_.size());
}

void BotDrivers::clear(BattleRoyaleBots& bots, vehicles::VehicleSystem& cars) {
    for (auto& ride : rides_) {
        cars.clear_autopilot(ride.vehicle);
        bots.set_mounted(ride.bot, false);
    }
    rides_.clear();
}

int BotDrivers::driver_of(std::string_view vehicle) const {
    for (const auto& ride : rides_) if (ride.active && ride.vehicle == vehicle) return ride.bot;
    return -1;
}

int BotDrivers::driving() const {
    return static_cast<int>(std::count_if(rides_.begin(), rides_.end(), [](const BotRide& r) { return r.active; }));
}

void BotDrivers::dismount(BotRide& ride, BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region) {
    ride.active = false;
    cars.clear_autopilot(ride.vehicle);
    bots.set_mounted(ride.bot, false);
    auto* bot = bots.find(ride.bot);
    const int v = cars.find(ride.vehicle);
    if (!bot || v < 0) return;
    const auto& car = cars.vehicles()[static_cast<std::size_t>(v)];
    // Out of the driver's door, or the other side if that is blocked.
    const float a = car.yaw * DEG2RAD;
    const Vector3 side{std::cos(a), 0, -std::sin(a)};
    for (const float s : {2.4F, -2.4F, 3.4F, -3.4F}) {
        Vector3 p{car.position.x + side.x * s, 0, car.position.z + side.z * s};
        const float terrain = world::terrain::TerrainHeight::sample(p.x, p.z);
        p.y = world::physics::WorldCollision::ground_height({p.x, terrain + 1, p.z}, terrain + 1, region);
        if (world::physics::WorldCollision::body_blocked(p, p.y, region, .35F)) continue;
        bot->set_position(p);
        break;
    }
    bot->set_roam_center(bot->position(), 60);
}

void BotDrivers::steer(BotRide& ride, vehicles::VehicleSystem& cars, float dt) {
    const int v = cars.find(ride.vehicle);
    if (v < 0) return;
    const auto& car = cars.vehicles()[static_cast<std::size_t>(v)];
    ride.progress = world::roads::route_progress(ride.route, car.position, ride.progress);
    const float remaining = ride.route.length - ride.progress, speed = car.speed;
    vehicles::VehicleInput input;
    if (ride.reverse > 0) {
        ride.reverse -= dt;
        input.throttle = -.8F;
        cars.set_autopilot(ride.vehicle, input);
        return;
    }
    // Pure pursuit along the lane, faster than a resident and less patient.
    const Vector3 target = world::roads::route_point(ride.route, ride.progress + 5.0F + std::abs(speed) * .8F);
    const float want = std::atan2(target.x - car.position.x, target.z - car.position.z) * RAD2DEG;
    const float error = std::remainder(want - car.yaw, 360.0F);
    input.steer = std::clamp(error / 22.0F, -1.0F, 1.0F);
    float near_yaw = 0, far_yaw = 0;
    world::roads::route_point(ride.route, ride.progress + 4, &near_yaw);
    world::roads::route_point(ride.route, ride.progress + 26, &far_yaw);
    float limit = std::abs(std::remainder(far_yaw - near_yaw, 360.0F)) > 25 ? 7.0F : 17.0F;
    if (std::abs(error) > 45) limit = std::min(limit, 4.0F);
    limit = std::min(limit, std::max(2.0F, std::sqrt(2 * 3.0F * std::max(0.0F, remaining))));
    // Cars in the lane ahead: slow behind them.
    const Vector3 forward{std::sin(car.yaw * DEG2RAD), 0, std::cos(car.yaw * DEG2RAD)};
    for (const auto& other : cars.vehicles()) {
        if (other.id == car.id) continue;
        const float x = other.position.x - car.position.x, z = other.position.z - car.position.z;
        const float ahead = x * forward.x + z * forward.z, lateral = std::abs(x * forward.z - z * forward.x);
        if (ahead > 1.5F && ahead < 6 + std::abs(speed) * 1.3F && lateral < 1.8F) limit = std::min(limit, 1.0F);
    }
    if (limit > 1 && std::abs(speed) < .3F) ride.stuck += dt;
    else ride.stuck = 0;
    if (ride.stuck > 1.8F) { ride.stuck = 0; ride.reverse = 1.2F; ++ride.attempts; }
    input.throttle = std::clamp((limit - speed) * .4F, -1.0F, 1.0F);
    if (speed > limit + 2) input.throttle = -1;
    cars.set_autopilot(ride.vehicle, input);
}

void BotDrivers::update(float dt, BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                        Vector3 player, bool player_alive, float zone_radius, const Sight& sight) {
    if (!std::isfinite(dt) || dt <= 0) return;
    for (auto& ride : rides_) {
        if (!ride.active) continue;
        auto* bot = bots.find(ride.bot);
        const int v = cars.find(ride.vehicle);
        const auto* asset = v >= 0 ? cars.asset(region, ride.vehicle) : nullptr;
        if (!bot || !bot->alive() || v < 0 || !asset) {
            ride.active = false;
            cars.clear_autopilot(ride.vehicle);
            bots.set_mounted(ride.bot, false);
            continue;
        }
        const auto& car = cars.vehicles()[static_cast<std::size_t>(v)];
        bot->set_position(car.position);
        bot->set_yaw(car.yaw);
        ride.driven += dt;
        // A wrecked car throws its driver out, hurt.
        if (asset->vehicle.destroyed || asset->vehicle.health <= 0) {
            bot->set_health(std::max(0.0F, bot->health() - 45));
            dismount(ride, bots, cars, region);
            continue;
        }
        const Vector3 eye{car.position.x, car.position.y + 1.4F, car.position.z};
        const bool ambush = player_alive && flat(car.position, player) < 55 && (!sight || sight(eye, {player.x, player.y + 1.5F, player.z}));
        const bool arrived = ride.route.length - ride.progress < 12 || std::hypot(car.position.x, car.position.z) < zone_radius * .3F;
        if (ambush || arrived || ride.attempts > 4 || ride.driven > 240) {
            dismount(ride, bots, cars, region);
            continue;
        }
        steer(ride, cars, dt);
    }
}

} // namespace outland::game::ai
