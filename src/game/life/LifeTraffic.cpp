// Island life's cars: households drive their own vehicles on longer trips. Off-screen a car moves
// along its road route on the island clock; near the player the real vehicle is driven by an
// autopilot through VehicleSystem's physics, keeping right, slowing for corners and stopping for
// people and cars ahead. The driver walks out to the car, drives, parks near the destination and
// walks in.
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::life {
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
Vector3 rotate(Vector3 local, float yaw_degrees) {
    const float a = yaw_degrees * DEG2RAD;
    return {local.x * std::cos(a) + local.z * std::sin(a), 0, -local.x * std::sin(a) + local.z * std::cos(a)};
}
constexpr float walk_speed = 1.4F; // m/s, island time
}

void LifeSimulation::bind_vehicles(vehicles::VehicleSystem* vehicles, world::VerdaRegion* region) {
    if (vehicles_ && vehicles_ != vehicles)
        for (const auto& car : island_.cars) vehicles_->clear_autopilot(car.vehicle);
    vehicles_ = vehicles && region ? vehicles : nullptr;
    region_ = vehicles_ ? region : nullptr;
    cars_.assign(island_.cars.size(), {});
    for (std::size_t i = 0; i < cars_.size(); ++i) {cars_[i].position = island_.cars[i].spot; cars_[i].yaw = island_.cars[i].yaw;}
}

void LifeSimulation::reset_cars() {
    cars_.assign(island_.cars.size(), {});
    for (std::size_t i = 0; i < cars_.size(); ++i) {
        const auto& info = island_.cars[i];
        auto& car = cars_[i];
        car.position = info.spot; car.yaw = info.yaw;
        if (!vehicles_) continue;
        vehicles_->clear_autopilot(info.vehicle);
        // Every session starts with the cars parked at home.
        car.lost = vehicles_->find(info.vehicle) < 0 || !vehicles_->place(*region_, info.vehicle, info.spot, info.yaw);
    }
}

Vector3 LifeSimulation::car_door(int index) const {
    const auto& car = cars_[static_cast<std::size_t>(index)];
    const Vector3 side = rotate({-1.5F, 0, .3F}, car.yaw); // driver's side, on the left
    return {car.position.x + side.x, car.position.y, car.position.z + side.z};
}

int LifeSimulation::car_of(std::string_view vehicle) const {
    for (std::size_t i = 0; i < island_.cars.size(); ++i) if (island_.cars[i].vehicle == vehicle) return static_cast<int>(i);
    return -1;
}

bool LifeSimulation::car_moving(std::string_view vehicle) const {
    const int i = car_of(vehicle);
    return i >= 0 && i < static_cast<int>(cars_.size()) && !cars_[static_cast<std::size_t>(i)].lost && cars_[static_cast<std::size_t>(i)].driving;
}

bool LifeSimulation::wants_car(const Resident& r, Vector3 from) const {
    if (!vehicles_ || r.car < 0 || static_cast<std::size_t>(r.car) >= cars_.size() || r.place < 0) return false;
    const auto& car = cars_[static_cast<std::size_t>(r.car)];
    if (car.lost || car.driving) return false;
    const Vector3 door = island_.places[static_cast<std::size_t>(r.place)].door;
    return flat(from, door) > config_.drive_distance && flat(from, car.position) < config_.car_reach &&
           flat(car.position, door) > config_.drive_distance * .5F;
}

void LifeSimulation::parking(int index, int place, Vector3& spot, float& yaw) const {
    const auto& info = island_.cars[static_cast<std::size_t>(index)];
    if (place == info.home) {spot = info.spot; yaw = info.yaw; return;}
    const Vector3 door = island_.places[static_cast<std::size_t>(place)].door;
    const auto* asset = vehicles_->asset(*region_, info.vehicle);
    const auto* definition = asset ? vehicles_->definition(*asset) : nullptr;
    const float scale = asset && definition ? asset->size.x / definition->width : 1;
    const auto free = [&](Vector3 p, float facing) {
        for (std::size_t j = 0; j < cars_.size(); ++j) {
            if (static_cast<int>(j) == index || cars_[j].lost) continue;
            if (flat(cars_[j].position, p) < 5.5F || (cars_[j].driving && flat(cars_[j].park, p) < 5.5F)) return false;
        }
        return !definition || !vehicles_->blocked(*region_, p, facing, *definition, scale, info.vehicle);
    };
    const auto road = roads_.nearest(door);
    if (road.edge >= 0 && road.distance < 150) {
        // Kerbside on the door's side, facing the way that side's traffic runs, and walk the rest
        // (a yard or a field is no place to drive). Wide streets have a parking lane; narrower
        // roads park on the verge.
        const Vector3 right{-road.direction.z, 0, road.direction.x};
        const float side = (door.x - road.point.x) * right.x + (door.z - road.point.z) * right.z >= 0 ? 1.0F : -1.0F;
        const float offset = road.width >= 10 ? road.width * .5F - 2.2F : road.width * .5F + 1.4F;
        const Vector3 base{road.point.x + right.x * side * offset, 0, road.point.z + right.z * side * offset};
        const Vector3 facing = side > 0 ? road.direction : Vector3Scale(road.direction, -1);
        const float facing_yaw = std::atan2(facing.x, facing.z) * RAD2DEG;
        for (const int k : {0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5}) {
            const Vector3 p{base.x + road.direction.x * 6.5F * k, 0, base.z + road.direction.z * 6.5F * k};
            if (free(p, facing_yaw)) {spot = p; yaw = facing_yaw; return;}
        }
    }
    // No road near, or the kerb is full: a clear patch near the door.
    for (const float radius : {6.0F, 9.0F, 12.0F, 16.0F})
        for (int a = 0; a < 12; ++a) {
            const float angle = a * 2 * PI / 12;
            const Vector3 p{door.x + std::cos(angle) * radius, 0, door.z + std::sin(angle) * radius};
            const float facing_yaw = std::atan2(door.x - p.x, door.z - p.z) * RAD2DEG + 90;
            if (free(p, facing_yaw)) {spot = p; yaw = facing_yaw; return;}
        }
    spot = road.edge >= 0 ? road.point : door;
    yaw = road.edge >= 0 ? std::atan2(road.direction.x, road.direction.z) * RAD2DEG : 0;
}

void LifeSimulation::start_drive(Resident& r) {
    auto& car = cars_[static_cast<std::size_t>(r.car)];
    if (car.lost) {leave_car(r, false); return;}
    r.trip = Trip::Driving;
    car.driving = true;
    route_car(r);
}

void LifeSimulation::route_car(Resident& r) {
    auto& car = cars_[static_cast<std::size_t>(r.car)];
    car.destination = r.place;
    parking(r.car, r.place, car.park, car.park_yaw);
    // Pull straight out of the space before turning for the road, and come into the next one
    // straight: turning on the spot clips whatever the car is parked beside.
    const Vector3 ahead = rotate({0, 0, 7}, car.yaw), into = rotate({0, 0, 7}, car.park_yaw);
    const Vector3 out{car.position.x + ahead.x, 0, car.position.z + ahead.z};
    const Vector3 in{car.park.x - into.x, 0, car.park.z - into.z};
    const auto middle = roads_.route(out, in);
    car.route = {};
    car.route.points.push_back({car.position.x, 0, car.position.z});
    for (std::size_t i = 0; i < middle.points.size(); ++i) {
        car.route.lane.push_back(i == 0 ? 0.0F : middle.lane[i - 1]);
        car.route.points.push_back(middle.points[i]);
    }
    car.route.lane.push_back(0);
    car.route.points.push_back({car.park.x, 0, car.park.z});
    for (std::size_t i = 0; i + 1 < car.route.points.size(); ++i) car.route.length += flat(car.route.points[i], car.route.points[i + 1]);
    car.route.complete = middle.complete;
    car.progress = 0;
    car.stuck = car.reverse = car.waiting = 0;
    car.attempts = 0;
    // Somewhere this close is no drive at all, and no road goes there: get out and walk.
    if (car.route.length < 25 || !car.route.complete) {park_car(r.car, true); leave_car(r, false);}
}

void LifeSimulation::park_car(int index, bool where_it_stands) {
    auto& car = cars_[static_cast<std::size_t>(index)];
    const auto& id = island_.cars[static_cast<std::size_t>(index)].vehicle;
    car.driving = false;
    if (!vehicles_) return;
    if (car.physical) {
        vehicles_->clear_autopilot(id);
        car.physical = false;
        if (const int v = vehicles_->find(id); v >= 0) {
            car.position = vehicles_->vehicles()[static_cast<std::size_t>(v)].position;
            car.yaw = vehicles_->vehicles()[static_cast<std::size_t>(v)].yaw;
        }
        return;
    }
    if (!where_it_stands) {car.position = car.park; car.yaw = car.park_yaw;}
    vehicles_->place(*region_, id, car.position, car.yaw);
}

void LifeSimulation::leave_car(Resident& r, bool frightened) {
    const Vector3 out = r.car >= 0 ? car_door(r.car) : position(r);
    r.trip = Trip::Walk;
    r.from = out; r.body = out;
    r.depart = clock_.minutes;
    const Vector3 door = r.place >= 0 ? island_.places[static_cast<std::size_t>(r.place)].door : out;
    r.arrive = clock_.minutes + flat(out, door) / walk_speed / 60;
    if (frightened) {
        r.fear = 1;
        r.shelter_until = std::max(r.shelter_until, clock_.minutes + config_.shelter_minutes);
    }
}

LifeSimulation::Taken LifeSimulation::take_car(std::string_view vehicle) {
    const int index = car_of(vehicle);
    if (index < 0 || static_cast<std::size_t>(index) >= cars_.size() || cars_[static_cast<std::size_t>(index)].lost) return {};
    auto& car = cars_[static_cast<std::size_t>(index)];
    const int owner = island_.cars[static_cast<std::size_t>(index)].owner;
    Taken taken{owner, false, car.position};
    if (vehicles_) vehicles_->clear_autopilot(vehicle);
    if (car.driving) {
        taken.occupied = true;
        car.driving = car.physical = false;
        auto& r = island_.residents[static_cast<std::size_t>(owner)];
        leave_car(r, true);
        replan(r);
    }
    car.lost = true;
    return taken;
}

void LifeSimulation::drive(int index, float dt, Vector3 player, const characters::NpcSystem& npcs, int& driven) {
    auto& car = cars_[static_cast<std::size_t>(index)];
    const auto& info = island_.cars[static_cast<std::size_t>(index)];
    auto& r = island_.residents[static_cast<std::size_t>(info.owner)];
    if (!car.physical) {
        const float near = flat(car.position, player);
        if (near < config_.car_radius && driven < config_.max_driven) {
            car.physical = true; ++driven;
            vehicles_->place(*region_, info.vehicle, car.position, car.yaw);
            car.stuck = car.reverse = car.waiting = 0; car.attempts = 0;
        } else {
            // Within sight it keeps to real speeds; far off it runs on the island clock.
            const float speed = near < 450 ? 12.0F : config_.car_speed * config_.time_scale;
            car.progress += dt * speed;
            if (car.progress >= car.route.length) {park_car(index, false); leave_car(r, false); return;}
            float yaw = car.yaw;
            car.position = world::roads::route_point(car.route, car.progress, &yaw);
            car.yaw = yaw;
            if (near < 450) vehicles_->place(*region_, info.vehicle, car.position, car.yaw);
            return;
        }
    }
    const int v = vehicles_->find(info.vehicle);
    if (v < 0) return;
    const auto& runtime = vehicles_->vehicles()[static_cast<std::size_t>(v)];
    car.position = runtime.position; car.yaw = runtime.yaw;
    if (flat(runtime.position, player) > config_.car_release) {
        // Out of sight again: back to the island clock from where it got to.
        vehicles_->clear_autopilot(info.vehicle);
        car.physical = false; --driven;
        car.progress = world::roads::route_progress(car.route, runtime.position, car.progress);
        return;
    }
    car.progress = world::roads::route_progress(car.route, runtime.position, car.progress);
    const float remaining = car.route.length - car.progress, speed = runtime.speed;
    vehicles::VehicleInput input;
    if (remaining < 2.5F || (remaining < 6 && std::abs(speed) < .6F)) {
        input.brake = input.parking = true;
        vehicles_->set_autopilot(info.vehicle, input);
        if (std::abs(speed) < .3F) {park_car(index, true); leave_car(r, false);}
        return;
    }
    // Pure pursuit on the lane-offset route.
    const Vector3 target = world::roads::route_point(car.route, car.progress + 4.5F + std::abs(speed) * .7F);
    const float want = std::atan2(target.x - runtime.position.x, target.z - runtime.position.z) * RAD2DEG;
    const float error = std::remainder(want - runtime.yaw, 360.0F);
    input.steer = std::clamp(error / 25.0F, -1.0F, 1.0F);
    float limit = 11;
    float near_yaw = 0, far_yaw = 0;
    world::roads::route_point(car.route, car.progress + 4, &near_yaw);
    world::roads::route_point(car.route, car.progress + 22, &far_yaw);
    if (std::abs(std::remainder(far_yaw - near_yaw, 360.0F)) > 25) limit = 5;
    if (std::abs(error) > 45) limit = std::min(limit, 3.5F);
    // Pulling out of and into a parking space, gently.
    const float first_leg = car.route.points.size() > 1 ? flat(car.route.points[0], car.route.points[1]) : 0;
    const float last_leg = car.route.points.size() > 1 ? flat(car.route.points[car.route.points.size() - 2], car.route.points.back()) : 0;
    if (car.progress < first_leg + 3 || remaining < last_leg + 3) limit = std::min(limit, 4.0F);
    limit = std::min(limit, std::max(1.5F, std::sqrt(2 * 2.5F * remaining)));
    // Anyone or anything in the lane ahead: stop and wait.
    const Vector3 forward{std::sin(runtime.yaw * DEG2RAD), 0, std::cos(runtime.yaw * DEG2RAD)};
    const float reach = 4.0F + std::abs(speed) * 1.3F + 2.5F;
    const auto in_lane = [&](Vector3 p) {
        const float x = p.x - runtime.position.x, z = p.z - runtime.position.z;
        const float ahead = x * forward.x + z * forward.z, lateral = std::abs(x * forward.z - z * forward.x);
        return ahead > 1.5F && ahead < reach && lateral < 1.6F;
    };
    bool blocked = in_lane(player);
    for (const auto& actor : npcs.actors()) if (actor.active && actor.state != characters::NpcState::Dead) blocked = blocked || in_lane(actor.position);
    for (const auto& other : vehicles_->vehicles()) if (other.id != info.vehicle) blocked = blocked || in_lane(other.position);
    if (blocked) {limit = 0; car.waiting += dt;}
    else car.waiting = 0;
    // Wedged against something: back off and try again; after a few tries, park and walk.
    if (car.reverse > 0) {
        car.reverse -= dt;
        input.throttle = -.7F; input.steer = -input.steer;
        vehicles_->set_autopilot(info.vehicle, input);
        return;
    }
    if (!blocked && limit > 1 && std::abs(speed) < .25F) car.stuck += dt;
    else car.stuck = 0;
    if (car.stuck > 2) {
        car.stuck = 0; car.reverse = 1.4F;
        if (++car.attempts > 5) {park_car(index, true); leave_car(r, false); return;}
    }
    input.throttle = std::clamp((limit - speed) * .35F, -1.0F, 1.0F);
    if (limit <= .01F || speed > limit + 1.5F) {input.brake = true; input.throttle = 0;}
    vehicles_->set_autopilot(info.vehicle, input);
}

void LifeSimulation::update_traffic(float dt, Vector3 player, const characters::NpcSystem& npcs) {
    if (!vehicles_ || cars_.size() != island_.cars.size()) return;
    const auto* player_car = vehicles_->driver();
    int driven = 0;
    for (const auto& car : cars_) driven += car.physical && !car.lost;
    for (std::size_t i = 0; i < cars_.size(); ++i) {
        auto& car = cars_[i];
        if (car.lost) continue;
        const auto& id = island_.cars[i].vehicle;
        const auto* asset = vehicles_->asset(*region_, id);
        if (vehicles_->find(id) < 0 || !asset || asset->vehicle.destroyed || (player_car && player_car->id == id)) {
            // Wrecked, or the player has it: out of island life. A driver inside gets out.
            if (car.driving) {
                auto& r = island_.residents[static_cast<std::size_t>(island_.cars[i].owner)];
                vehicles_->clear_autopilot(id);
                car.driving = car.physical = false;
                leave_car(r, true);
            }
            car.lost = true;
            continue;
        }
        if (!car.driving) {car.position = asset->position; car.yaw = asset->rotation_y;} // pushed about
    }
    for (std::size_t i = 0; i < cars_.size(); ++i)
        if (!cars_[i].lost && cars_[i].driving) drive(static_cast<int>(i), dt, player, npcs, driven);
    for (auto& r : island_.residents) {
        if (!r.alive || r.trip != Trip::ToCar || r.car < 0) continue;
        if (cars_[static_cast<std::size_t>(r.car)].lost) {
            const Vector3 here = position(r);
            r.trip = Trip::Walk; r.from = here; r.depart = clock_.minutes;
            r.arrive = clock_.minutes + flat(here, trip_target(r)) / walk_speed / 60;
            continue;
        }
        // Off-screen walkers reach the car on the clock; bodies do it in sync_bodies.
        if (!r.physical && clock_.minutes >= r.arrive) start_drive(r);
    }
}
}
