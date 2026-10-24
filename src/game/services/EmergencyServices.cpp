#include "outland/game/services/EmergencyServices.hpp"
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/game/ai/BotDrivers.hpp"
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::services {

namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
Vector3 lift(Vector3 p, float y) { return {p.x, p.y + y, p.z}; }
bool busy(const Unit& u) { return u.state == UnitState::Responding || u.state == UnitState::Pursuit || u.state == UnitState::OnScene; }
}

const char* unit_state_name(UnitState state) {
    switch (state) {
        case UnitState::Parked: return "parked";
        case UnitState::Responding: return "responding";
        case UnitState::Pursuit: return "in pursuit";
        case UnitState::OnScene: return "on scene";
        case UnitState::Returning: return "returning";
        case UnitState::Out: return "out of service";
        case UnitState::Patrol: return "on patrol";
    }
    return "?";
}

float EmergencyServices::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 7; rng_ ^= rng_ << 17;
    return static_cast<float>(rng_ >> 40) / static_cast<float>(1ULL << 24);
}

int EmergencyServices::start(vehicles::VehicleSystem& cars, world::VerdaRegion& region, const characters::CharacterRegistry* registry) {
    units_.clear();
    claimed_.clear();
    events_ = {};
    registry_ = registry;
    next_crew_ = 0;
    roads_.build(region);
    for (const auto& car : cars.vehicles()) {
        const bool police = car.id.find("_police_") != std::string::npos, ambulance = car.id.find("_ambulance_") != std::string::npos;
        if (!police && !ambulance) continue;
        Unit u;
        u.kind = police ? UnitKind::Police : UnitKind::Ambulance;
        u.vehicle = car.id;
        u.home = car.position;
        u.home_yaw = car.yaw;
        units_.push_back(std::move(u));
    }
    // The hospital's door: its purpose marker (just inside the entrance, facing out), a few
    // metres out into the street.
    hospital_found_ = false;
    for (const auto& town : region.settlements())
        for (const auto& asset : town.assets) {
            if (!asset.model_path.ends_with("civic/hospital.glb")) continue;
            for (const auto& owner : region.settlements())
                for (const auto& marker : owner.gameplay_markers) {
                    if (marker.type != world::GameplayMarkerType::BuildingPurpose || marker.purpose != world::BuildingPurpose::Clinic) continue;
                    if (flat(marker.position, asset.position) > 30) continue;
                    const float a = marker.rotation_y * DEG2RAD;
                    hospital_door_ = {marker.position.x - std::sin(a) * 4.5F, marker.position.y, marker.position.z - std::cos(a) * 4.5F};
                    hospital_found_ = true;
                }
        }
    return static_cast<int>(units_.size());
}

void EmergencyServices::clear(vehicles::VehicleSystem& cars, characters::NpcSystem& npcs) {
    for (auto& u : units_) {
        cars.clear_autopilot(u.vehicle);
        if (!u.suspect.empty()) cars.hold(u.suspect, false);
        for (const int id : u.crew) npcs.despawn_resident(id);
    }
    for (const auto& t : troopers_) npcs.despawn_resident(t.resident);
    troopers_.clear();
    beat_.clear();
    units_.clear();
    claimed_.clear();
}

const Unit* EmergencyServices::unit(std::string_view vehicle) const {
    for (const auto& u : units_) if (u.vehicle == vehicle) return &u;
    return nullptr;
}

bool EmergencyServices::siren(std::string_view vehicle) const {
    const auto* u = unit(vehicle);
    return u && u->siren;
}

int EmergencyServices::count(UnitKind kind, UnitState state) const {
    return static_cast<int>(std::count_if(units_.begin(), units_.end(), [&](const Unit& u) { return u.kind == kind && u.state == state; }));
}

std::vector<Vector3> EmergencyServices::officers_on_foot(const characters::NpcSystem& npcs) const {
    std::vector<Vector3> out;
    for (const auto& t : troopers_) {
        const int index = npcs.find_resident(t.resident);
        if (index >= 0 && npcs.actors()[static_cast<std::size_t>(index)].health > 0) out.push_back(npcs.actors()[static_cast<std::size_t>(index)].position);
    }
    for (const auto& u : units_) {
        if (u.kind == UnitKind::Ambulance) continue;
        for (const int id : u.crew) {
            const int index = npcs.find_resident(id);
            if (index < 0) continue;
            const auto& actor = npcs.actors()[static_cast<std::size_t>(index)];
            if (actor.state != characters::NpcState::Dead && actor.health > 0) out.push_back(actor.position);
        }
    }
    return out;
}

void EmergencyServices::add_highway(const std::string& vehicle, Vector3 home, float yaw) {
    if (unit(vehicle)) return;
    Unit u;
    u.kind = UnitKind::Highway;
    u.vehicle = vehicle;
    u.home = home;
    u.home_yaw = yaw;
    units_.push_back(std::move(u));
}

void EmergencyServices::post_troopers(Vector3 at, float yaw, characters::NpcSystem& npcs) {
    const float a = yaw * DEG2RAD;
    const Vector3 ahead{std::sin(a), 0, std::cos(a)}, side{std::cos(a), 0, -std::sin(a)};
    for (int k = 0; k < 2; ++k) {
        characters::ResidentSpawn spawn;
        spawn.resident = crew_base + next_crew_++;
        spawn.character_id = trooper_characters[k % 2];
        spawn.pool = characters::CharacterPool::Emergency;
        spawn.position = Vector3Add(Vector3Add(at, Vector3Scale(ahead, 4.4F)), Vector3Scale(side, k == 0 ? -1.4F : 1.2F));
        spawn.anchor = spawn.position;
        spawn.anchor_radius = 1.2F;
        spawn.yaw_degrees = yaw;
        spawn.police = true;
        npcs.spawn_resident(spawn);
        troopers_.push_back({spawn.resident, spawn.position, yaw});
    }
}

void EmergencyServices::guard(Vector3 where, vehicles::VehicleSystem& cars) {
    Unit* best = nullptr;
    float nearest = 1e9F;
    for (auto& u : units_) {
        if (u.kind != UnitKind::Highway || u.state == UnitState::Out || busy(u)) continue;
        const int v = cars.find(u.vehicle);
        if (v < 0) continue;
        const float d = flat(cars.vehicles()[static_cast<std::size_t>(v)].position, where);
        if (d < nearest) { nearest = d; best = &u; }
    }
    if (best) { dispatch(*best, where, true, cars); events_.message = "The Ŝosea Patrolo is riding out to the ranch"; }
}

bool EmergencyServices::trooper(int resident) const {
    for (const auto& t : troopers_) if (t.resident == resident) return true;
    for (const auto& u : units_)
        if (u.kind == UnitKind::Highway && std::find(u.crew.begin(), u.crew.end(), resident) != u.crew.end()) return true;
    return false;
}

void EmergencyServices::highway(Unit& u, const ServicesFrame& frame, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                                characters::NpcSystem& npcs) {
    const int v = cars.find(u.vehicle);
    const auto& car = cars.vehicles()[static_cast<std::size_t>(v)];
    const float distance = flat(car.position, frame.player);
    const bool visible = frame.player_alive && distance < 200 &&
        (!frame.sight || frame.sight(lift(car.position, 1.4F), lift(frame.player, 1.5F)));
    if (frame.wanted > 0 && visible && frame.now - backup_at_ > 30) {   // "send backup"
        backup_at_ = frame.now;
        events_.backup = true;
    }
    if (frame.wanted > 0 && !busy(u) && visible && distance < 220) dispatch(u, frame.last_known, true, cars);
    if (busy(u)) { police(u, frame, cars, region, npcs); return; }
    // Back on the beat: on round the ring road, one point after another.
    // No road there: wait a moment before trying the next point (not a new route every frame).
    if (u.state == UnitState::Patrol && !u.route.complete && (u.replan -= frame.dt) > 0) return;
    const bool arrived = u.state == UnitState::Patrol && drive(u, cars, frame.dt);
    if (beat_.empty() || (u.state == UnitState::Patrol && !arrived)) return;
    const int n = static_cast<int>(beat_.size());
    if (u.waypoint < 0 || u.state != UnitState::Patrol) {
        int best = 0;
        for (int i = 1; i < n; ++i)
            if (flat(beat_[static_cast<std::size_t>(i)], car.position) < flat(beat_[static_cast<std::size_t>(best)], car.position)) best = i;
        u.waypoint = (best + 1) % n;
    } else {
        u.waypoint = (u.waypoint + 1) % n;
    }
    dispatch(u, beat_[static_cast<std::size_t>(u.waypoint)], false, cars);
    u.state = UnitState::Patrol;
}

bool EmergencyServices::pursued(std::string_view suspect) const {
    return std::any_of(units_.begin(), units_.end(), [&](const Unit& u) { return u.suspect == suspect; });
}

bool EmergencyServices::pursue(const std::string& suspect, Vector3 where, vehicles::VehicleSystem& cars) {
    if (suspect.empty() || pursued(suspect) || unit(suspect) || cars.find(suspect) < 0) return false;
    Unit* best = nullptr;
    float nearest = suspect_reach;
    for (auto& u : units_) {
        if (u.kind == UnitKind::Ambulance || u.vehicle == player_car_) continue;
        if (u.state != UnitState::Parked && u.state != UnitState::Returning && u.state != UnitState::Patrol) continue;
        const int v = cars.find(u.vehicle);
        if (v < 0) continue;
        const float d = flat(cars.vehicles()[static_cast<std::size_t>(v)].position, where);
        if (d < nearest) { nearest = d; best = &u; }
    }
    if (!best) return false;
    dispatch(*best, where, true, cars);
    best->suspect = suspect;
    events_.message = best->kind == UnitKind::Highway ? "The Ŝosea Patrolo is after the hit-and-run driver" : "Police are after the hit-and-run driver";
    return true;
}

void EmergencyServices::drop_suspect(Unit& u, vehicles::VehicleSystem& cars) {
    cars.hold(u.suspect, false);
    u.suspect.clear();
    go_home(u, cars);
}

void EmergencyServices::after_suspect(Unit& u, const ServicesFrame& frame, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                                      characters::NpcSystem& npcs) {
    const int v = cars.find(u.vehicle), s = cars.find(u.suspect);
    const auto* suspect_asset = s >= 0 ? cars.asset(region, u.suspect) : nullptr;
    const bool taken = cars.driver() && cars.driver()->id == u.suspect;   // the player took the car: nobody left to arrest
    if (v < 0 || s < 0 || !suspect_asset || suspect_asset->vehicle.destroyed || taken) {
        if (crew_back(u, cars, npcs, frame.dt, true)) drop_suspect(u, cars);
        else halt(u, cars);
        return;
    }
    const auto& car = cars.vehicles()[static_cast<std::size_t>(v)];
    const auto& them = cars.vehicles()[static_cast<std::size_t>(s)];
    const float distance = flat(car.position, them.position);
    const float a = them.yaw * DEG2RAD;
    const Vector3 velocity{std::sin(a) * them.speed, 0, std::cos(a) * them.speed};
    switch (u.state) {
        case UnitState::Responding: {
            u.replan -= frame.dt;
            if (u.replan <= 0 && flat(them.position, u.target) > 25) dispatch(u, them.position, true, cars);
            // By road until close; then straight at them.
            const bool done = drive(u, cars, frame.dt);
            if (distance < 45) { u.state = UnitState::Pursuit; u.scene = u.unseen = u.stalled = 0; u.gap = distance; break; }
            if (done) dispatch(u, them.position, true, cars);
            break;
        }
        case UnitState::Pursuit:
            // Not closing in (a wall between, or they got away): back to the roads.
            if (distance < u.gap - 1) { u.gap = distance; u.stalled = 0; }
            else u.stalled += frame.dt;
            if (distance > 80 || (u.stalled > 4 && distance > 16)) { dispatch(u, them.position, true, cars); break; }
            // On their bumper for a couple of seconds and they pull over.
            u.scene = distance < 14 ? u.scene + frame.dt : std::max(0.0F, u.scene - frame.dt);
            if (u.scene > 2.0F || (distance < 16 && std::abs(them.speed) < 1)) {
                cars.hold(u.suspect, true);
                halt(u, cars);
                u.state = UnitState::OnScene;
                u.scene = 0;
                crew_out(u, 2, true, false, cars, region, npcs);
                break;
            }
            chase(u, cars, them.position, velocity, frame.dt);
            break;
        case UnitState::OnScene: {
            cars.hold(u.suspect, true);
            halt(u, cars);
            u.scene += frame.dt;
            if (u.scene < 8) {
                std::erase_if(u.crew, [&](int id) { return npcs.find_resident(id) < 0; });
                for (const int id : u.crew) npcs.direct_resident(id, them.position, 1.0F, false);
                break;
            }
            // The driver is cuffed and taken in; the car goes to the pound.
            if (u.scene < 8.5F && std::find(events_.arrested.begin(), events_.arrested.end(), u.suspect) == events_.arrested.end()) {
                events_.arrested.push_back(u.suspect);
                events_.message = "HIT AND RUN: the police caught the driver";
            }
            if (crew_back(u, cars, npcs, frame.dt, false)) drop_suspect(u, cars);
            break;
        }
        default:
            dispatch(u, them.position, true, cars);
            break;
    }
}

std::string EmergencyServices::character(bool police) {
    if (!registry_) return {};
    const auto seed = static_cast<std::uint64_t>(random() * 1e9F) * 2654435761ULL + (police ? 7 : 13);
    const auto* definition = registry_->choose(characters::CharacterPool::Emergency, seed);
    if (!definition) definition = registry_->choose(characters::CharacterPool::Civilian, seed);
    return definition ? definition->id : std::string();
}

void EmergencyServices::dispatch(Unit& u, Vector3 target, bool siren, vehicles::VehicleSystem& cars) {
    const int v = cars.find(u.vehicle);
    if (v < 0) { u.state = UnitState::Out; return; }
    u.route = roads_.route(cars.vehicles()[static_cast<std::size_t>(v)].position, target);
    u.progress = u.stuck = u.reverse = u.scene = u.unseen = 0;
    u.attempts = 0;
    u.replan = 3;
    u.target = target;
    u.siren = siren;
    u.state = UnitState::Responding;
}

void EmergencyServices::go_home(Unit& u, vehicles::VehicleSystem& cars) {
    dispatch(u, u.home, false, cars);
    u.state = UnitState::Returning;
}

bool EmergencyServices::drive(Unit& u, vehicles::VehicleSystem& cars, float dt) {
    if (!u.route.complete || u.route.points.size() < 2) return true;
    ai::steer_along(u.route, u.progress, u.stuck, u.reverse, u.attempts, u.vehicle, cars, dt,
                    u.siren ? 21.0F : 14.0F, u.siren ? 9.0F : 7.0F);
    return u.route.length - u.progress < 6 || u.attempts > 6;
}

void EmergencyServices::chase(Unit& u, vehicles::VehicleSystem& cars, Vector3 target, Vector3 velocity, float dt) {
    const int v = cars.find(u.vehicle);
    if (v < 0) return;
    const auto& car = cars.vehicles()[static_cast<std::size_t>(v)];
    vehicles::VehicleInput input;
    if (u.reverse > 0) {
        u.reverse -= dt;
        input.throttle = -.8F;
        cars.set_autopilot(u.vehicle, input);
        return;
    }
    // Straight at the suspect, leading them a little; hard on the gas, easing off up close.
    const Vector3 aim = Vector3Add(target, Vector3Scale(velocity, .6F));
    const float want = std::atan2(aim.x - car.position.x, aim.z - car.position.z) * RAD2DEG;
    const float error = std::remainder(want - car.yaw, 360.0F);
    input.steer = std::clamp(error / 20.0F, -1.0F, 1.0F);
    const float distance = flat(car.position, target);
    float limit = std::clamp(distance * .9F, 5.0F, 28.0F);
    if (std::abs(error) > 60) limit = std::min(limit, 7.0F);
    if (limit > 1 && std::abs(car.speed) < .3F) u.stuck += dt;
    else u.stuck = 0;
    if (u.stuck > 1.5F) { u.stuck = 0; u.reverse = 1.0F; }
    input.throttle = std::clamp((limit - car.speed) * .4F, -1.0F, 1.0F);
    cars.set_autopilot(u.vehicle, input);
}

void EmergencyServices::halt(Unit& u, vehicles::VehicleSystem& cars) {
    const int v = cars.find(u.vehicle);
    if (v < 0) return;
    const float speed = cars.vehicles()[static_cast<std::size_t>(v)].speed;
    vehicles::VehicleInput input;
    if (std::abs(speed) > .4F) input.throttle = speed > 0 ? -1.0F : 1.0F;
    else input.parking = true;
    cars.set_autopilot(u.vehicle, input);
}

void EmergencyServices::crew_out(Unit& u, int count, bool police, bool armed, vehicles::VehicleSystem& cars,
                                 world::VerdaRegion& region, characters::NpcSystem& npcs) {
    (void)region;
    const int v = cars.find(u.vehicle);
    if (v < 0) return;
    const auto& car = cars.vehicles()[static_cast<std::size_t>(v)];
    const float a = car.yaw * DEG2RAD;
    const Vector3 right{std::cos(a), 0, -std::sin(a)}, back{-std::sin(a), 0, -std::cos(a)};
    for (int k = 0; k < count; ++k) {
        characters::ResidentSpawn spawn;
        spawn.resident = crew_base + next_crew_++;
        spawn.character_id = u.kind == UnitKind::Highway ? std::string(trooper_characters[k % 2]) : character(police);
        spawn.pool = characters::CharacterPool::Emergency;
        // Officers from the front doors; paramedics round the back.
        const float side = k % 2 == 0 ? 1.7F : -1.7F;
        spawn.position = Vector3Add(Vector3Add(car.position, Vector3Scale(right, side)), Vector3Scale(back, police ? 0.0F : 2.4F));
        spawn.anchor = spawn.position;
        spawn.anchor_radius = 1.5F;
        spawn.yaw_degrees = car.yaw;
        spawn.police = police;
        npcs.spawn_resident(spawn);
        if (police && armed) npcs.arm_resident(spawn.resident, true);
        u.crew.push_back(spawn.resident);
    }
}

bool EmergencyServices::crew_back(Unit& u, vehicles::VehicleSystem& cars, characters::NpcSystem& npcs, float dt, bool hurry_home) {
    const int v = cars.find(u.vehicle);
    u.scene += dt;
    std::erase_if(u.crew, [&](int id) {
        const int index = npcs.find_resident(id);
        if (index < 0) return true;
        const auto& actor = npcs.actors()[static_cast<std::size_t>(index)];
        if (actor.state == characters::NpcState::Dead || actor.health <= 0) return true; // left for the ambulance
        npcs.arm_resident(id, false);
        const Vector3 door = v >= 0 ? cars.vehicles()[static_cast<std::size_t>(v)].position : actor.position;
        // At the car (its body is in the way of the very centre), or they have taken long enough.
        if (v < 0 || flat(actor.position, door) < 4.5F || u.scene > (hurry_home ? 8.0F : 14.0F)) {
            npcs.despawn_resident(id);
            return true;
        }
        npcs.direct_resident(id, door, 1.0F, true);
        return false;
    });
    return u.crew.empty();
}

void EmergencyServices::police(Unit& u, const ServicesFrame& frame, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                               characters::NpcSystem& npcs) {
    const int v = cars.find(u.vehicle);
    const auto& car = cars.vehicles()[static_cast<std::size_t>(v)];
    const float distance = flat(car.position, frame.player);
    const bool visible = frame.player_alive && distance < 160 &&
        (!frame.sight || frame.sight(lift(car.position, 1.4F), lift(frame.player, 1.5F)));
    if (busy(u) && frame.wanted <= 0) {
        // The search is off: everyone back in and home.
        if (crew_back(u, cars, npcs, frame.dt, true)) go_home(u, cars);
        else halt(u, cars);
        return;
    }
    switch (u.state) {
        case UnitState::Responding: {
            u.unseen = visible ? 0 : u.unseen + frame.dt;
            const Vector3 target = visible ? frame.player : frame.last_known;
            if (frame.player_driving && visible && distance < 90) { u.state = UnitState::Pursuit; return; }
            u.replan -= frame.dt;
            if (u.replan <= 0 && flat(target, u.target) > 25) dispatch(u, target, true, cars);
            const bool done = drive(u, cars, frame.dt);
            if ((!frame.player_driving && visible && distance < 40) || done || flat(car.position, target) < 18) {
                halt(u, cars);
                u.state = UnitState::OnScene;
                u.scene = 0;
                crew_out(u, 2, true, frame.wanted >= 2, cars, region, npcs);
            }
            break;
        }
        case UnitState::Pursuit:
            u.unseen = visible ? 0 : u.unseen + frame.dt;
            if (u.unseen > 5) { dispatch(u, frame.last_known, true, cars); break; }
            if (!frame.player_driving && distance < 40) {
                halt(u, cars);
                u.state = UnitState::OnScene;
                u.scene = 0;
                crew_out(u, 2, true, frame.wanted >= 2, cars, region, npcs);
                break;
            }
            chase(u, cars, frame.player, frame.player_velocity, frame.dt);
            break;
        case UnitState::OnScene: {
            halt(u, cars);
            u.scene += frame.dt;
            const bool gone = frame.player_driving && distance > 20;
            const bool lost = !visible && u.scene > 30;
            if (gone || lost) {
                if (crew_back(u, cars, npcs, frame.dt, false)) {
                    if (gone && visible) u.state = UnitState::Pursuit;
                    else dispatch(u, frame.last_known, true, cars);
                }
                break;
            }
            // On foot after the suspect: armed from two stars, otherwise to make the arrest.
            std::erase_if(u.crew, [&](int id) { return npcs.find_resident(id) < 0; });
            for (const int id : u.crew) {
                npcs.arm_resident(id, frame.wanted >= 2);
                npcs.direct_resident(id, visible ? frame.player : frame.last_known, 1.2F, true);
            }
            if (u.crew.empty() && u.scene > 2) dispatch(u, frame.last_known, true, cars); // both down: the car drives on
            break;
        }
        case UnitState::Returning:
            if (drive(u, cars, frame.dt) || flat(car.position, u.home) < 8 || distance > 400) {
                cars.clear_autopilot(u.vehicle);
                if (distance > 90) cars.place(region, u.vehicle, u.home, u.home_yaw);
                u.state = UnitState::Parked;
                u.siren = false;
            }
            break;
        default: break;
    }
}

void EmergencyServices::ambulance(Unit& u, const ServicesFrame& frame, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                                  characters::NpcSystem& npcs, life::LifeSimulation* life) {
    const int v = cars.find(u.vehicle);
    const auto& car = cars.vehicles()[static_cast<std::size_t>(v)];
    const int index = u.patient >= 0 ? npcs.find_resident(u.patient) : -1;
    const auto unclaim = [&]() { std::erase(claimed_, u.patient); u.patient = -1; };
    switch (u.state) {
        case UnitState::Responding: {
            if (index < 0) { unclaim(); go_home(u, cars); break; } // they are gone (or someone else took them)
            const Vector3 where = npcs.actors()[static_cast<std::size_t>(index)].position;
            u.replan -= frame.dt;
            if (u.replan <= 0 && flat(where, u.target) > 12) dispatch(u, where, true, cars);
            const bool done = drive(u, cars, frame.dt);
            if (done || flat(car.position, where) < 16) {
                halt(u, cars);
                u.state = UnitState::OnScene;
                u.scene = 0;
                crew_out(u, 2, false, false, cars, region, npcs);
            }
            break;
        }
        case UnitState::OnScene: {
            halt(u, cars);
            u.scene += frame.dt;
            if (!u.loaded) {
                if (index < 0) { unclaim(); u.loaded = true; u.patient_alive = false; break; }
                const auto& patient = npcs.actors()[static_cast<std::size_t>(index)];
                bool reached = u.scene > 16;
                for (const int id : u.crew) {
                    const int p = npcs.find_resident(id);
                    if (p < 0) continue;
                    if (flat(npcs.actors()[static_cast<std::size_t>(p)].position, patient.position) < 2.2F) reached = true;
                    npcs.direct_resident(id, patient.position, .8F, true);
                }
                if (reached && u.scene > 4) {
                    // On the stretcher and into the back.
                    u.patient_alive = patient.state != characters::NpcState::Dead && patient.health > 0;
                    npcs.despawn_resident(u.patient);
                    if (!crew_member(u.patient) && life) {
                        if (u.patient_alive && life->hospitalise(u.patient, hospital_door_, treatment_minutes)) events_.hospitalised.push_back(u.patient);
                        else events_.morgue.push_back(u.patient);
                    }
                    for (auto& other : units_) std::erase(other.crew, u.patient); // a fallen officer
                    std::erase(claimed_, u.patient);
                    u.patient = -1;
                    u.loaded = true;
                    u.scene = 0;
                }
                break;
            }
            if (crew_back(u, cars, npcs, frame.dt, false)) {
                dispatch(u, u.home, u.patient_alive, cars);
                u.state = UnitState::Returning;
            }
            break;
        }
        case UnitState::Returning: {
            const float distance = flat(car.position, frame.player);
            if (drive(u, cars, frame.dt) || flat(car.position, u.home) < 8 || distance > 450) {
                cars.clear_autopilot(u.vehicle);
                if (distance > 90) cars.place(region, u.vehicle, u.home, u.home_yaw);
                u.state = UnitState::Parked;
                u.siren = false;
                u.loaded = false;
            }
            break;
        }
        default: break;
    }
}

void EmergencyServices::update(const ServicesFrame& frame, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                               characters::NpcSystem& npcs, life::LifeSimulation* life) {
    events_ = {};
    if (units_.empty() || !std::isfinite(frame.dt) || frame.dt <= 0) return;
    // A unit whose car is gone, wrecked or taken by the player is out of service.
    for (auto& u : units_) {
        // The player's own patrol car (an officer): its lights are theirs to work.
        if (!player_car_.empty() && u.vehicle == player_car_) {
            u.siren = player_siren_ && cars.driver() && cars.driver()->id == u.vehicle;
            if (u.state != UnitState::Out) {
                cars.clear_autopilot(u.vehicle);
                for (const int id : u.crew) npcs.arm_resident(id, false);
                u.crew.clear();
                u.state = UnitState::Out;
            }
            continue;
        }
        if (u.state == UnitState::Out) continue;
        const int v = cars.find(u.vehicle);
        const auto* asset = v >= 0 ? cars.asset(region, u.vehicle) : nullptr;
        const bool stolen = cars.driver() && cars.driver()->id == u.vehicle;
        if (v < 0 || !asset || asset->vehicle.destroyed || asset->vehicle.health <= 0 || stolen) {
            cars.clear_autopilot(u.vehicle);
            for (const int id : u.crew) npcs.arm_resident(id, false);
            u.crew.clear();
            if (!u.suspect.empty()) cars.hold(u.suspect, false);
            u.suspect.clear();
            if (u.patient >= 0) std::erase(claimed_, u.patient);
            u.patient = -1;
            u.siren = false;
            u.state = UnitState::Out;
            if (stolen) events_.message = u.kind == UnitKind::Ambulance ? "You took an ambulance" :
                                          u.kind == UnitKind::Highway ? "You took a Ŝosea Patrolo cruiser" : "You took a police car";
        }
    }

    // Police: one car per star (three at most), the nearest free ones.
    if (frame.wanted > 0) {
        int out = 0;
        for (const auto& u : units_) if (u.kind == UnitKind::Police && busy(u)) ++out;
        const int want = std::min(3, frame.wanted);
        while (out < want) {
            Unit* best = nullptr;
            float nearest = 2200;
            for (auto& u : units_) {
                if (u.kind != UnitKind::Police || (u.state != UnitState::Parked && u.state != UnitState::Returning)) continue;
                const int v = cars.find(u.vehicle);
                const float d = flat(cars.vehicles()[static_cast<std::size_t>(v)].position, frame.last_known);
                if (d < nearest) { nearest = d; best = &u; }
            }
            if (!best) break;
            dispatch(*best, frame.last_known, true, cars);
            if (out == 0) events_.message = "Police on the way";
            ++out;
        }
    }
    // Ambulances: the nearest free one to each person down in the street.
    for (const auto& actor : npcs.actors()) {
        if (actor.resident < 0 || actor.bot >= 0 || actor.hidden) continue;
        const bool dead = actor.state == characters::NpcState::Dead || actor.health <= 0;
        if (!dead && actor.health >= 60) continue;
        if (std::find(claimed_.begin(), claimed_.end(), actor.resident) != claimed_.end()) continue;
        bool crew = false; // officers still on their feet stay on the job
        for (const auto& u : units_) crew |= !dead && std::find(u.crew.begin(), u.crew.end(), actor.resident) != u.crew.end();
        if (crew) continue;
        Unit* best = nullptr;
        float nearest = 3000;
        for (auto& u : units_) {
            if (u.kind != UnitKind::Ambulance || u.loaded || (u.state != UnitState::Parked && u.state != UnitState::Returning)) continue;
            const int v = cars.find(u.vehicle);
            const float d = flat(cars.vehicles()[static_cast<std::size_t>(v)].position, actor.position);
            if (d < nearest) { nearest = d; best = &u; }
        }
        if (!best) break;
        best->patient = actor.resident;
        claimed_.push_back(actor.resident);
        dispatch(*best, actor.position, true, cars);
        if (events_.message.empty()) events_.message = "An ambulance is on its way";
    }
    // The troopers at the outposts: they stand their post, and draw on a dangerous man close by.
    for (const auto& t : troopers_) {
        const int index = npcs.find_resident(t.resident);
        if (index < 0) continue;
        const auto& actor = npcs.actors()[static_cast<std::size_t>(index)];
        if (actor.state == characters::NpcState::Dead || actor.health <= 0) continue;
        const bool danger = frame.wanted >= 2 && frame.player_alive && flat(actor.position, frame.player) < 70;
        npcs.arm_resident(t.resident, danger);
        npcs.direct_resident(t.resident, danger ? frame.player : t.post, danger ? 1.2F : .8F, danger);
    }
    for (auto& u : units_) {
        if (u.state == UnitState::Out || cars.find(u.vehicle) < 0) continue;
        if (!u.suspect.empty()) { after_suspect(u, frame, cars, region, npcs); continue; }
        if (u.kind == UnitKind::Highway) { highway(u, frame, cars, region, npcs); continue; }
        if (u.state == UnitState::Parked) continue;
        if (u.kind == UnitKind::Police) police(u, frame, cars, region, npcs);
        else ambulance(u, frame, cars, region, npcs, life);
    }
}

const Unit* EmergencyServices::station_near(Vector3 feet, float radius) const {
    const Unit* best = nullptr;
    for (const auto& u : units_)
        if (u.kind == UnitKind::Police && flat(u.home, feet) < radius && (!best || flat(u.home, feet) < flat(best->home, feet))) best = &u;
    return best;
}

void draw_light_bar(Vector3 position, float yaw, float y, float z, float width, int seed, Vector3 camera, float time) {
    if (flat(position, camera) > 350) return;
    const Color left = RED, right{40, 110, 255, 255};
    // A faster double-flash than the AI cars: two pulses a side.
    const float cycle = std::fmod(time * 2.5F + static_cast<float>(seed) * .37F, 1.0F);
    const bool phase = cycle < .5F;
    const bool pulse = std::fmod(cycle * 4, 1.0F) < .6F;
    for (const float side : {1.0F, -1.0F}) {
        const bool on = (side > 0) == phase && pulse;
        const Color c = side > 0 ? left : right;
        const Vector3 p = vehicles::VehicleSystem::world_point({side * width, y, z}, position, yaw);
        DrawSphere(p, on ? .12F : .06F, on ? c : Fade(c, .5F));
        if (on) DrawSphere(p, .55F, Fade(c, .3F));
    }
    // The flashers in the grille, alternating with the bar.
    for (const float side : {1.0F, -1.0F}) {
        const bool on = (side > 0) != phase && pulse;
        if (on) DrawSphere(vehicles::VehicleSystem::world_point({side * .2F, .74F, 2.75F}, position, yaw), .18F, Fade(side > 0 ? left : right, .55F));
    }
}

void draw_light_bars(const EmergencyServices& services, const vehicles::VehicleSystem& cars, Vector3 camera, float time) {
    for (const auto& u : services.units()) {
        if (!u.siren) continue;
        const int v = cars.find(u.vehicle);
        if (v < 0) continue;
        const auto& car = cars.vehicles()[static_cast<std::size_t>(v)];
        if (flat(car.position, camera) > 350) continue;
        const bool police = u.kind != UnitKind::Ambulance;
        const Color left = RED, right = police ? Color{40, 110, 255, 255} : Color{255, 170, 30, 255};
        const float y = police ? 1.6F : 1.98F, z = police ? -.1F : 1.8F;
        const bool phase = std::fmod(time * 3.0F + static_cast<float>(v) * .37F, 1.0F) < .5F;
        for (const float side : {1.0F, -1.0F}) {
            const bool on = (side > 0) == phase;
            const Color c = side > 0 ? left : right;
            const Vector3 p = vehicles::VehicleSystem::world_point({side * .42F, y, z}, car.position, car.yaw);
            DrawSphere(p, on ? .13F : .07F, on ? c : Fade(c, .5F));
            if (on) DrawSphere(p, .45F, Fade(c, .28F));
        }
        if (!police) // the box's red corner lamps flash too
            for (const float side : {1.0F, -1.0F}) {
                const Vector3 p = vehicles::VehicleSystem::world_point({side * .9F, 2.6F, 1.12F}, car.position, car.yaw);
                if (phase == (side < 0)) DrawSphere(p, .3F, Fade(RED, .35F));
            }
    }
}

} // namespace outland::game::services
