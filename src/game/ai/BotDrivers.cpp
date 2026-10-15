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
    if (car.id.find("_police_") != std::string::npos || car.id.find("_ambulance_") != std::string::npos) return false; // the services' own
    if (cars.autopiloted(car.id) || (cars.driver() && cars.driver()->id == car.id)) return false; // taken already
    const auto* asset = cars.asset(region, car.id);
    return asset && asset->vehicle.enabled && !asset->vehicle.destroyed && asset->vehicle.fuel > .15F && asset->vehicle.health > 30;
}
}

Vector3 BotDrivers::zone_goal(Vector3 from, float zone_radius, Vector2 center) {
    // Well inside the circle along the line to its centre.
    const float dx = from.x - center.x, dz = from.z - center.y;
    const float distance = std::hypot(dx, dz), inside = std::max(60.0F, zone_radius * .35F);
    if (distance <= inside) return {center.x, 0, center.y};
    const float keep = inside / distance;
    return {center.x + dx * keep, 0, center.y + dz * keep};
}

int BotDrivers::start(BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                      std::uint64_t seed, float zone_radius, int max_drivers, Vector2 zone_center) {
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
        ride.route = roads_.route(car.position, zone_goal(car.position, zone_radius, zone_center));
        if (!ride.route.complete || ride.route.length < 80) continue;
        ride.active = true;
        taken.push_back(car.id);
        bots.set_mounted(bot.id(), true);
        if (auto* driver = bots.find(bot.id())) { driver->set_position(car.position); driver->set_yaw(car.yaw); }
        rides_.push_back(std::move(ride));
    }
    return static_cast<int>(rides_.size());
}

bool BotDrivers::reserved(std::string_view vehicle) const {
    return std::any_of(reserved_.begin(), reserved_.end(), [&](const std::string& marker) { return vehicle.find(marker) != std::string_view::npos; });
}

bool BotDrivers::dispatch(BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                          const std::vector<int>& crew, Vector3 from, Vector3 goal, float reach, std::string_view vehicle) {
    if (crew.empty()) return false;
    if (roads_.nodes().empty()) roads_.build(region);
    int best = -1;
    float nearest = reach;
    for (std::size_t i = 0; i < cars.vehicles().size(); ++i) {
        const auto& car = cars.vehicles()[i];
        if (!usable(cars, region, car) || driver_of(car.id) >= 0) continue;
        if (vehicle.empty() ? reserved(car.id) : car.id != vehicle) continue;
        const float d = flat(car.position, from);
        if (d < nearest) { nearest = d; best = static_cast<int>(i); }
    }
    if (best < 0) return false;
    const auto& car = cars.vehicles()[static_cast<std::size_t>(best)];
    BotRide ride;
    ride.bot = crew.front();
    ride.vehicle = car.id;
    ride.route = roads_.route(car.position, goal);
    if (!ride.route.complete || ride.route.length < 60) return false;
    ride.active = ride.targeted = true;
    ride.ambush = false;
    ride.goal = goal;
    ride.passengers.assign(crew.begin() + 1, crew.end());
    for (const int member : crew) {
        bots.set_mounted(member, true);
        if (auto* bot = bots.find(member)) { bot->set_position(car.position); bot->set_yaw(car.yaw); }
    }
    std::erase_if(rides_, [](const BotRide& r) { return !r.active; });
    rides_.push_back(std::move(ride));
    return true;
}

const BotRide* BotDrivers::ride_of(int bot) const {
    for (const auto& ride : rides_)
        if (ride.active && (ride.bot == bot || std::find(ride.passengers.begin(), ride.passengers.end(), bot) != ride.passengers.end())) return &ride;
    return nullptr;
}

void BotDrivers::clear(BattleRoyaleBots& bots, vehicles::VehicleSystem& cars) {
    for (auto& ride : rides_) {
        cars.clear_autopilot(ride.vehicle);
        bots.set_mounted(ride.bot, false);
        for (const int p : ride.passengers) bots.set_mounted(p, false);
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
    const int v = cars.find(ride.vehicle);
    std::vector<int> occupants{ride.bot};
    occupants.insert(occupants.end(), ride.passengers.begin(), ride.passengers.end());
    for (std::size_t k = 0; k < occupants.size(); ++k) {
        bots.set_mounted(occupants[k], false);
        auto* bot = bots.find(occupants[k]);
        if (!bot || v < 0) continue;
        const auto& car = cars.vehicles()[static_cast<std::size_t>(v)];
        // Out of the driver's door (passengers the other side, then behind), or wherever is clear.
        const float a = car.yaw * DEG2RAD;
        const Vector3 side{std::cos(a), 0, -std::sin(a)}, back{-std::sin(a), 0, -std::cos(a)};
        const float along = static_cast<float>(k / 2) * 1.6F, first = k % 2 == 0 ? 2.4F : -2.4F;
        for (const float s : {first, -first, first * 1.4F, -first * 1.4F}) {
            Vector3 p{car.position.x + side.x * s + back.x * along, 0, car.position.z + side.z * s + back.z * along};
            const float terrain = world::terrain::TerrainHeight::sample(p.x, p.z);
            p.y = world::physics::WorldCollision::ground_height({p.x, terrain + 1, p.z}, terrain + 1, region);
            if (world::physics::WorldCollision::body_blocked(p, p.y, region, .35F)) continue;
            bot->set_position(p);
            break;
        }
        bot->set_roam_center(ride.targeted ? ride.goal : bot->position(), ride.targeted ? 25 : 60);
    }
}

void steer_along(const world::roads::Route& route, float& progress, float& stuck, float& reverse, int& attempts,
                 std::string_view vehicle, vehicles::VehicleSystem& cars, float dt, float straight, float bends) {
    const int v = cars.find(vehicle);
    if (v < 0) return;
    const auto& car = cars.vehicles()[static_cast<std::size_t>(v)];
    progress = world::roads::route_progress(route, car.position, progress);
    const float remaining = route.length - progress, speed = car.speed;
    vehicles::VehicleInput input;
    if (reverse > 0) {
        reverse -= dt;
        input.throttle = -.8F;
        cars.set_autopilot(vehicle, input);
        return;
    }
    // Pure pursuit along the lane, faster than a resident and less patient.
    const Vector3 target = world::roads::route_point(route, progress + 5.0F + std::abs(speed) * .8F);
    const float want = std::atan2(target.x - car.position.x, target.z - car.position.z) * RAD2DEG;
    const float error = std::remainder(want - car.yaw, 360.0F);
    input.steer = std::clamp(error / 22.0F, -1.0F, 1.0F);
    float near_yaw = 0, far_yaw = 0;
    world::roads::route_point(route, progress + 4, &near_yaw);
    world::roads::route_point(route, progress + 26, &far_yaw);
    float limit = std::abs(std::remainder(far_yaw - near_yaw, 360.0F)) > 25 ? bends : straight;
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
    if (limit > 1 && std::abs(speed) < .3F) stuck += dt;
    else stuck = 0;
    if (stuck > 1.8F) { stuck = 0; reverse = 1.2F; ++attempts; }
    input.throttle = std::clamp((limit - speed) * .4F, -1.0F, 1.0F);
    if (speed > limit + 2) input.throttle = -1;
    cars.set_autopilot(vehicle, input);
}

void BotDrivers::steer(BotRide& ride, vehicles::VehicleSystem& cars, float dt) {
    steer_along(ride.route, ride.progress, ride.stuck, ride.reverse, ride.attempts, ride.vehicle, cars, dt);
}

void BotDrivers::update(float dt, BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                        Vector3 player, bool player_alive, float zone_radius, const Sight& sight, Vector2 zone_center) {
    if (!std::isfinite(dt) || dt <= 0) return;
    for (auto& ride : rides_) {
        if (!ride.active) continue;
        auto* bot = bots.find(ride.bot);
        const int v = cars.find(ride.vehicle);
        const auto* asset = v >= 0 ? cars.asset(region, ride.vehicle) : nullptr;
        if (!bot || !bot->alive() || v < 0 || !asset) {
            if (v >= 0 && asset) { dismount(ride, bots, cars, region); continue; } // the driver died: all out
            ride.active = false;
            cars.clear_autopilot(ride.vehicle);
            bots.set_mounted(ride.bot, false);
            for (const int p : ride.passengers) bots.set_mounted(p, false);
            continue;
        }
        const auto& car = cars.vehicles()[static_cast<std::size_t>(v)];
        bot->set_position(car.position);
        bot->set_yaw(car.yaw);
        for (const int p : ride.passengers)
            if (auto* passenger = bots.find(p)) { passenger->set_position(car.position); passenger->set_yaw(car.yaw); }
        ride.driven += dt;
        // A wrecked car throws its driver out, hurt.
        if (asset->vehicle.destroyed || asset->vehicle.health <= 0) {
            bot->set_health(std::max(0.0F, bot->health() - 45));
            for (const int p : ride.passengers)
                if (auto* passenger = bots.find(p)) passenger->set_health(std::max(0.0F, passenger->health() - 45));
            dismount(ride, bots, cars, region);
            continue;
        }
        const Vector3 eye{car.position.x, car.position.y + 1.4F, car.position.z};
        const bool ambush = ride.ambush && player_alive && flat(car.position, player) < 55 && (!sight || sight(eye, {player.x, player.y + 1.5F, player.z}));
        const bool arrived = ride.route.length - ride.progress < 12 || (ride.targeted && flat(car.position, ride.goal) < 22) ||
            (zone_radius > 0 && std::hypot(car.position.x - zone_center.x, car.position.z - zone_center.y) < zone_radius * .3F);
        if (ambush || arrived || ride.attempts > 4 || ride.driven > 240) {
            dismount(ride, bots, cars, region);
            continue;
        }
        steer(ride, cars, dt);
    }
}

} // namespace outland::game::ai
