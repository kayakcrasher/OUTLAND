#include "outland/game/career/RoutineCalls.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/ai/BotDrivers.hpp"
#include "outland/game/autos/Autos.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::career {
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
std::uint64_t mix(std::uint64_t z) {
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
float unit(std::uint64_t& h) { h = mix(h); return static_cast<float>(h % 10000) / 10000.0F; }
Vector3 ground(Vector3 p, const world::VerdaRegion& region) {
    const float terrain = world::terrain::TerrainHeight::sample(p.x, p.z);
    const float probe = std::max(terrain, p.y) + 1.2F;
    return {p.x, world::physics::WorldCollision::ground_height({p.x, probe, p.z}, probe, region), p.z};
}
struct Ride { const char* definition; const char* name; };
constexpr Ride rides[]{{"sedan", "Sedano"}, {"hatchback", "hatchback"}, {"truck", "pickup"}, {"wagon", "Familio estate"}, {"muscle", "Tempesto muscle car"}};
std::string lower(std::string text) {
    for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
}

const char* routine_name(RoutineKind kind) {
    switch (kind) {
        case RoutineKind::Domestic: return "DOMESTIC DISPUTE";
        case RoutineKind::DrunkDriver: return "DRUNK DRIVER";
        case RoutineKind::Pharmacy: return "PHARMACY ROBBERY";
    }
    return "";
}

void RoutineCalls::start(ai::BattleRoyaleBots& bots, Vector3 park, std::uint64_t seed) {
    *this = RoutineCalls{};
    seed_ = seed;
    auto& bot = bots.add_bot(park, combat::WeaponId::Pistol, ai::BotLevel::Medium, seed + 4441);
    bot_ = bot.id();
    bots.retire(bot_);
    bots.set_hidden(bot_, true);
}

bool RoutineCalls::hostile(int a, int b) const {
    if (stage_ != Stage::Fighting) return false;
    return (a == bot_ && b == ai::BattleRoyaleBots::player_id) || (b == bot_ && a == ai::BattleRoyaleBots::player_id);
}

bool RoutineCalls::dispatch(const Frame& frame, const std::vector<CallScene>& scenes, const std::vector<CallScene>& pharmacies, Career& career,
                            ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                            std::uint64_t seed) {
    if (bot_ < 0 || stage_ != Stage::Idle || frame.now < next_call_ || !frame.alive) return false;
    next_call_ = frame.now + 8;
    auto h = mix(seed ^ static_cast<std::uint64_t>(frame.now * 1000));
    const auto in_reach = [&](const CallScene& s) { const float d = flat(s.door, frame.officer); return d >= scene_near && d <= scene_far; };
    for (int attempt = 0; attempt < 6; ++attempt) {
        h = mix(h);
        const auto kind = static_cast<RoutineKind>(h % 3);
        // Where: a house (a domestic), a pub's kerb (a drunk driver), a clinic's pharmacy.
        std::vector<const CallScene*> pool;
        for (const auto& s : kind == RoutineKind::Pharmacy ? pharmacies : scenes) {
            if (!in_reach(s)) continue;
            if (kind == RoutineKind::Domestic && s.shop) continue;
            if (kind == RoutineKind::DrunkDriver && s.place != "the pub") continue;
            pool.push_back(&s);
        }
        if (pool.empty()) continue;
        const CallScene& scene = *pool[mix(h) % pool.size()];
        Job job;
        job.title = routine_name(kind);
        job.target = scene.door;
        job.needed = 1;
        job.deadline = frame.now + 900;
        job.respect = 6;
        std::string car;
        if (kind == RoutineKind::DrunkDriver) {
            // His car at the kerb, and home for him: a town a long drive off.
            const CallScene* home = nullptr;
            for (const auto& s : scenes)
                if (flat(s.door, scene.door) > 600 && (!home || flat(s.door, scene.door) < flat(home->door, scene.door))) home = &s;
            if (!home) continue;
            const auto& ride = rides[(h >> 8) % std::size(rides)];
            const Vector3 kerb = Vector3Add(scene.door, Vector3Scale(scene.out, 6));
            const float along = std::atan2(scene.out.x, scene.out.z) * RAD2DEG + 90;
            if (!cars.spawn(region, kerb, along, ride.definition)) continue;
            car = cars.last_spawned();
            const std::size_t colour = 1 + (h >> 16) % (autos::street_paints - 1);
            if (auto* a = cars.asset(region, car)) {
                autos::apply({static_cast<int>(colour), 0, 1, 0}, a->vehicle);
                a->vehicle.marker = "routine_" + car;   // nobody else's to drive off in
            }
            bots.respawn(bot_, ground(kerb, region), along, seed_ + h % 1000);
            bots.set_hidden(bot_, false);
            bots.set_asleep(bot_, false);
            if (!drivers.dispatch(bots, cars, region, {bot_}, kerb, home->door, 40, car)) {
                cars.remove(region, car);
                bots.retire(bot_);
                bots.set_hidden(bot_, true);
                continue;
            }
            plate_ = Callouts::make_plate(h);
            job.detail = "Drunk driver reported leaving " + scene.place + " in " + scene.town + ": a " + lower(autos::paints[colour].name) + " " +
                         ride.name + ", plate " + plate_ + ", all over the road. Lights on, get on his bumper, pull him over";
            job.pay = 360;
        } else {
            const Vector3 at = ground(Vector3Add(scene.door, Vector3Scale(scene.out, kind == RoutineKind::Domestic ? 2.0F : 3.0F)), region);
            bots.respawn(bot_, at, std::atan2(scene.out.x, scene.out.z) * RAD2DEG, seed_ + h % 1000);
            bots.set_hidden(bot_, false);
            bots.set_asleep(bot_, false);
            if (auto* b = bots.find(bot_)) b->set_roam_center(at, 2.5F);
            if (kind == RoutineKind::Domestic) {
                job.detail = "Domestic dispute at a house in " + scene.town + ": the neighbours hear shouting and breaking glass. He's out front - bring him in";
                job.pay = 300;
            } else {
                job.detail = "Pharmacy robbery at " + scene.place + " in " + scene.town + ": one man, armed, after the medicine cabinet. Take him";
                job.pay = 450;
                job.respect = 8;
            }
        }
        if (!career.routine_call(job)) {
            if (!car.empty()) { drivers.cancel(bot_, bots, cars); cars.remove(region, car); }
            bots.retire(bot_);
            bots.set_hidden(bot_, true);
            return false;
        }
        kind_ = kind;
        scene_ = scene.door;
        car_ = car;
        tailed_ = 0;
        roll_ = mix(h ^ 0x51ed);
        told_lethal_ = false;
        stage_ = kind == RoutineKind::DrunkDriver ? Stage::Driving : Stage::Waiting;
        next_call_ = frame.now + 30;
        return true;
    }
    return false;
}

void RoutineCalls::decide(ai::BattleRoyaleBots& bots, std::string& said) {
    // Face to face with the officer: a drunk nearly always gives up; a husband mostly does; a
    // robber with a gun is as likely to run or fight.
    const float r = unit(roll_);
    const float hands = kind_ == RoutineKind::DrunkDriver ? .85F : kind_ == RoutineKind::Domestic ? .65F : .4F;
    const float runs = kind_ == RoutineKind::DrunkDriver ? 1.0F : kind_ == RoutineKind::Domestic ? .85F : .7F;
    if (r < hands) {
        stage_ = Stage::HandsUp;
        bots.set_asleep(bot_, true);
        said = "Hands up! He gives up - walk up and USE to cuff him";
    } else if (r < runs) {
        stage_ = Stage::Running;
        said = "He's running for it - catch him, or stop him";
    } else {
        stage_ = Stage::Fighting;
        said = "He's pulled a gun! You may use lethal force";
        told_lethal_ = true;
    }
}

void RoutineCalls::update(const Frame& frame, Career& career, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                          vehicles::VehicleSystem& cars, world::VerdaRegion& region, std::string& said) {
    walking_ = false;
    // An old car is towed once nobody is looking.
    if (!stale_car_.empty()) {
        const int v = cars.find(stale_car_);
        if (v < 0) stale_car_.clear();
        else if (flat(cars.vehicles()[static_cast<std::size_t>(v)].position, frame.officer) > 150 && !drivers.ride_of(bot_)) {
            cars.remove(region, stale_car_);
            stale_car_.clear();
        }
    }
    if (stage_ == Stage::Idle) return;
    const bool on_call = career.job().kind == JobKind::Callout && career.job().routine;
    if (!career.on_duty() || !frame.alive || (!on_call && stage_ != Stage::Cuffed)) { end(bots, drivers, cars); return; }
    auto* bot = bots.find(bot_);
    if (!bot || !bot->alive()) {
        // Down: stopped (an officer's lawful use of force).
        if (on_call) career.suspect_down(false);
        if (drivers.ride_of(bot_)) drivers.cancel(bot_, bots, cars);
        bots.set_asleep(bot_, false);
        if (!car_.empty()) stale_car_ = car_;
        car_.clear();
        stage_ = Stage::Idle;   // (he lies where he fell until the next call)
        return;
    }
    const float near = flat(bot->position(), frame.officer);
    switch (stage_) {
        case Stage::Driving: {
            const auto* ride = drivers.ride_of(bot_);
            const int v = cars.find(car_);
            if (!ride) {
                // Home, stuck or crashed: out of the car, and he faces whoever comes.
                stage_ = Stage::Waiting;
                if (auto* b = bots.find(bot_)) b->set_roam_center(b->position(), 2);
                break;
            }
            if (v >= 0) career.track(cars.vehicles()[static_cast<std::size_t>(v)].position);
            if (v >= 0 && !frame.police_car.empty() && frame.lights &&
                flat(frame.car, cars.vehicles()[static_cast<std::size_t>(v)].position) < tail_distance) tailed_ += frame.dt;
            if (tailed_ >= tail_seconds) {
                drivers.halt(bot_, bots, cars, region);
                stage_ = Stage::Waiting;
                if (auto* b = bots.find(bot_)) b->set_roam_center(b->position(), 1.5F);
                said = "The drunk driver pulled over";
            }
            break;
        }
        case Stage::Waiting:
            career.track(bot->position());
            if (frame.alive && near < face_off) decide(bots, said);
            break;
        case Stage::Running: {
            career.track(bot->position());
            if (near < caught || bot->health() < give_up_health) {
                stage_ = Stage::HandsUp;
                bots.set_asleep(bot_, true);
                said = "He gives up - USE to cuff him";
                break;
            }
            Vector3 away{bot->position().x - frame.officer.x, 0, bot->position().z - frame.officer.z};
            away = Vector3Length(away) > .1F ? Vector3Normalize(away) : Vector3{1, 0, 0};
            bot->set_roam_center(Vector3Add(bot->position(), Vector3Scale(away, 30)), 3);
            break;
        }
        case Stage::Fighting:
            career.track(bot->position());
            if (bot->health() < give_up_health) {
                stage_ = Stage::HandsUp;
                bots.set_asleep(bot_, true);
                said = "He drops the gun - hands up. USE to cuff him";
            }
            break;
        case Stage::HandsUp:
            career.track(bot->position());
            break;
        case Stage::Cuffed: {
            // He walks behind the officer; with the officer at the wheel of a police car close by,
            // he climbs in the back.
            if (!frame.police_car.empty() && near < Callouts::board_reach) {
                if (career.arrested(-1)) {
                    career.suspect_down(true);
                    said = "In the back of the car: " + std::to_string(career.custody()) + " in custody - drive to the state prison";
                    bots.retire(bot_);
                    bots.set_hidden(bot_, true);
                    stage_ = Stage::Idle;
                    if (!car_.empty()) stale_car_ = car_;
                    car_.clear();
                    return;
                }
                said = career.message();
            }
            if (near > Callouts::escort_lost) { said = "Your prisoner slipped away"; end(bots, drivers, cars); return; }
            if (frame.police_car.empty() && near > 1.2F) {
                const Vector3 behind{-std::sin(frame.yaw), 0, -std::cos(frame.yaw)};
                const Vector3 goal = Vector3Add(frame.officer, Vector3Scale(behind, Callouts::escort_gap));
                Vector3 to{goal.x - bot->position().x, 0, goal.z - bot->position().z};
                const float gap = Vector3Length(to);
                if (gap > .3F) {
                    const float step = std::min(gap, (gap > 6 ? 4.2F : 2.4F) * frame.dt);
                    Vector3 p = Vector3Add(bot->position(), Vector3Scale(Vector3Normalize(to), step));
                    if (gap > 40) p = goal;
                    bot->set_position(ground({p.x, frame.officer.y, p.z}, region));
                    bot->set_yaw(std::atan2(to.x, to.z) * RAD2DEG);
                    walking_ = true;
                }
            }
            break;
        }
        case Stage::Idle: break;
    }
}

int RoutineCalls::to_cuff(Vector3 feet, ai::BattleRoyaleBots& bots) const {
    if (stage_ != Stage::HandsUp) return -1;
    const auto* bot = bots.find(bot_);
    if (!bot || !bot->alive() || std::abs(bot->position().y - feet.y) > 1.8F || flat(bot->position(), feet) > Callouts::cuff_reach) return -1;
    return bot_;
}

bool RoutineCalls::cuff(ai::BattleRoyaleBots& bots) {
    if (stage_ != Stage::HandsUp) return false;
    stage_ = Stage::Cuffed;
    bots.set_asleep(bot_, true);
    return true;
}

void RoutineCalls::end(ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers, vehicles::VehicleSystem& cars) {
    if (drivers.ride_of(bot_)) drivers.cancel(bot_, bots, cars);
    bots.set_asleep(bot_, false);
    bots.retire(bot_);
    bots.set_hidden(bot_, true);
    if (!car_.empty()) stale_car_ = car_;
    car_.clear();
    stage_ = Stage::Idle;
}

} // namespace outland::game::career
