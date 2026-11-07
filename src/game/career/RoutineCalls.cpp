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
        case RoutineKind::TrafficStop: return "TRAFFIC STOP";
        case RoutineKind::Shoplifter: return "SHOPLIFTER";
        case RoutineKind::BarFight: return "BAR FIGHT";
        case RoutineKind::StolenCar: return "STOLEN CAR";
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
    if (bot_ < 0 || frame.now < next_call_ || !frame.alive) return false;
    if (stage_ == Stage::Leaving) end(bots, drivers, cars);   // last call's ticketed driver is long gone
    if (stage_ != Stage::Idle) return false;
    next_call_ = frame.now + 8;
    auto h = mix(seed ^ static_cast<std::uint64_t>(frame.now * 1000));
    const auto in_reach = [&](const CallScene& s) { const float d = flat(s.door, frame.officer); return d >= scene_near && d <= scene_far; };
    // How often each comes up: traffic stops most, then the rest.
    constexpr RoutineKind deck[]{RoutineKind::TrafficStop, RoutineKind::TrafficStop, RoutineKind::Domestic, RoutineKind::DrunkDriver,
                                 RoutineKind::Pharmacy, RoutineKind::Shoplifter, RoutineKind::BarFight, RoutineKind::StolenCar};
    for (int attempt = 0; attempt < 8; ++attempt) {
        h = mix(h);
        const auto kind = deck[h % std::size(deck)];
        // Where: a house (a domestic), a pub (a drunk driver, a bar fight), a shop (a shoplifter),
        // a clinic's pharmacy, anywhere on a street (a speeder, a stolen car).
        std::vector<const CallScene*> pool;
        for (const auto& s : kind == RoutineKind::Pharmacy ? pharmacies : scenes) {
            if (!in_reach(s)) continue;
            if (kind == RoutineKind::Domestic && s.shop) continue;
            if ((kind == RoutineKind::DrunkDriver || kind == RoutineKind::BarFight) && s.place != "the pub") continue;
            if (kind == RoutineKind::Shoplifter && (!s.shop || s.place == "the pub")) continue;
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
        if (driving_call(kind)) {
            // His car at the kerb, and where he's going: a town a long drive off.
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
            home_ = home->door;
            plate_ = Callouts::make_plate(h);
            const std::string look = "a " + lower(autos::paints[colour].name) + " " + ride.name + ", plate " + plate_;
            if (kind == RoutineKind::DrunkDriver) {
                job.detail = "Drunk driver reported leaving " + scene.place + " in " + scene.town + ": " + look +
                             ", all over the road. Lights on, get on his bumper, pull him over";
                job.pay = 360;
            } else if (kind == RoutineKind::TrafficStop) {
                job.detail = "Speeder through " + scene.town + ": " + look + ", doing twice the limit past the school. Pull him over and write him a ticket";
                job.pay = 200;
                job.respect = 4;
            } else {
                job.detail = "Stolen car spotted in " + scene.town + ": " + look + ". Pull it over and take the driver";
                job.pay = 420;
                job.respect = 7;
            }
        } else {
            const float out = kind == RoutineKind::Domestic ? 2.0F : 3.0F;
            const Vector3 at = ground(Vector3Add(scene.door, Vector3Scale(scene.out, out)), region);
            bots.respawn(bot_, at, std::atan2(scene.out.x, scene.out.z) * RAD2DEG, seed_ + h % 1000);
            bots.set_hidden(bot_, false);
            bots.set_asleep(bot_, false);
            if (auto* b = bots.find(bot_)) b->set_roam_center(at, 2.5F);
            switch (kind) {
                case RoutineKind::Domestic:
                    job.detail = "Domestic dispute at a house in " + scene.town + ": the neighbours hear shouting and breaking glass. He's out front - bring him in";
                    job.pay = 300;
                    break;
                case RoutineKind::Shoplifter:
                    job.detail = "Shoplifter at " + scene.place + " in " + scene.town + ": he's out of the door with the goods. Catch him";
                    job.pay = 220;
                    job.respect = 4;
                    break;
                case RoutineKind::BarFight:
                    job.detail = "Bar fight at " + scene.place + " in " + scene.town + ": a drunk throwing chairs out front. Calm it down - bring him in";
                    job.pay = 280;
                    break;
                default:
                    job.detail = "Pharmacy robbery at " + scene.place + " in " + scene.town + ": one man, armed, after the medicine cabinet. Take him";
                    job.pay = 450;
                    job.respect = 8;
                    break;
            }
        }
        first_ = career.routine_done() == 0 && !career.task_force();
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
        stage_ = driving_call(kind) ? Stage::Driving : Stage::Waiting;
        next_call_ = frame.now + 30;
        lesson_.clear();
        line_time_ = 0;
        if (first_) {
            // The first routine call: Officer Novak at the officer's side, talking it through.
            partner_wanted_ = true;
            const std::string novak = "Officer Novak";
            lesson_.push_back({novak, "First call, partner - " + std::string(routine_name(kind)) + ". Routine work: every officer starts here."});
            if (driving_call(kind)) {
                lesson_.push_back({novak, "Look for the car on the radio - colour, model, plate. It's on the compass."});
                lesson_.push_back({novak, "Get in behind him, lights on (L), and stay on his bumper. He'll pull over."});
            } else {
                lesson_.push_back({novak, "It's on the compass. Walk up to him - he'll make up his mind when he sees us."});
            }
            if (kind == RoutineKind::TrafficStop) lesson_.push_back({novak, "Pulled over: walk up to him and USE - write the ticket, and he's on his way."});
            lesson_.push_back({novak, "Hands up? Walk up and USE to cuff him. Runs? Catch him - or wound him and he gives up."});
            lesson_.push_back({novak, "If he pulls a gun, you may use lethal force. A suspect down is a suspect stopped."});
            lesson_.push_back({novak, "Cuffed, he walks with you. Get in the car close by and he climbs in the back - the call's done."});
            lesson_.push_back({novak, "Ten calls like this and the Chief will want you on the Gang Task Force."});
        }
        return true;
    }
    return false;
}

void RoutineCalls::hint(const std::string& text) {
    if (!first_) return;
    lesson_.assign({{"Officer Novak", text}});
    line_time_ = 0;
}

void RoutineCalls::decide(ai::BattleRoyaleBots& bots, std::string& said) {
    // Face to face with the officer: a speeder takes his ticket; a drunk nearly always gives up; a
    // shoplifter runs; a husband mostly gives up; an armed robber or car thief may run or fight.
    const float r = unit(roll_);
    if (kind_ == RoutineKind::TrafficStop) {
        if (r < .85F) {
            stage_ = Stage::Ticket;
            bots.set_asleep(bot_, true);
            said = "He waits by his car - walk up and USE to write the ticket";
            hint("Walk up to him and USE - write the ticket.");
            return;
        }
        stage_ = Stage::Running;
        said = "He's not waiting for a ticket - he's running for it";
        hint("He's running - catch him! Now he's coming in.");
        return;
    }
    float hands = .4F, runs = .7F;
    switch (kind_) {
        case RoutineKind::DrunkDriver: hands = .85F; runs = 1.0F; break;
        case RoutineKind::Domestic: hands = .65F; runs = .85F; break;
        case RoutineKind::Shoplifter: hands = .15F; runs = 1.0F; break;
        case RoutineKind::BarFight: hands = .7F; runs = 1.0F; break;
        case RoutineKind::StolenCar: hands = .5F; runs = .8F; break;
        default: break;
    }
    if (r < hands) {
        stage_ = Stage::HandsUp;
        bots.set_asleep(bot_, true);
        said = "Hands up! He gives up - walk up and USE to cuff him";
        hint("Hands up - good. Walk up and USE to cuff him.");
    } else if (r < runs) {
        stage_ = Stage::Running;
        said = "He's running for it - catch him, or stop him";
        hint("He's running - catch him, or wound him and he gives up.");
    } else {
        stage_ = Stage::Fighting;
        said = "He's pulled a gun! You may use lethal force";
        hint("Gun! Take cover - you may use lethal force.");
        told_lethal_ = true;
    }
}

int RoutineCalls::to_ticket(Vector3 feet, ai::BattleRoyaleBots& bots) const {
    if (stage_ != Stage::Ticket) return -1;
    const auto* bot = bots.find(bot_);
    if (!bot || !bot->alive() || std::abs(bot->position().y - feet.y) > 1.8F || flat(bot->position(), feet) > Callouts::cuff_reach) return -1;
    return bot_;
}

bool RoutineCalls::ticket(Career& career, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers, vehicles::VehicleSystem& cars,
                          world::VerdaRegion& region) {
    if (stage_ != Stage::Ticket) return false;
    career.suspect_down(true);   // (the call is done)
    bots.set_asleep(bot_, false);
    hint("Ticket written. That's a call done - on to the next.");
    // Back in his car and on his way, slower now.
    const auto* bot = bots.find(bot_);
    const int v = car_.empty() ? -1 : cars.find(car_);
    if (bot && v >= 0 && drivers.dispatch(bots, cars, region, {bot_}, bot->position(), home_, 30, car_)) {
        stage_ = Stage::Leaving;
        return true;
    }
    stage_ = Stage::Leaving;
    if (auto* b = bots.find(bot_)) b->set_roam_center(Vector3Add(b->position(), {20, 0, 20}), 3);
    return true;
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
    // The partner's lesson, a line at a time.
    if (!lesson_.empty() && (line_time_ += frame.dt) >= Callouts::line_seconds) { lesson_.erase(lesson_.begin()); line_time_ = 0; }
    if (stage_ == Stage::Idle) return;
    if (stage_ == Stage::Leaving) {
        // Ticketed, he drives on; out of the game once he's out of sight (or the drive is over).
        const auto* bot = bots.find(bot_);
        if (!bot || !bot->alive() || (!drivers.ride_of(bot_) && (!bot || flat(bot->position(), frame.officer) > 60)) ||
            (bot && flat(bot->position(), frame.officer) > 250))
            end(bots, drivers, cars);
        return;
    }
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
                said = kind_ == RoutineKind::DrunkDriver ? "The drunk driver pulled over" : kind_ == RoutineKind::TrafficStop ? "The speeder pulled over" : "The stolen car pulled over";
                hint("He's pulled over. Out of the car and walk up to him.");
            }
            break;
        }
        case Stage::Waiting:
            career.track(bot->position());
            // (A shoplifter sees the uniform from further off.)
            if (frame.alive && near < (kind_ == RoutineKind::Shoplifter ? face_off * 2.5F : face_off)) decide(bots, said);
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
        case Stage::Ticket:
            career.track(bot->position());
            break;
        case Stage::Leaving: break;
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

bool RoutineCalls::stun(ai::BattleRoyaleBots& bots, std::string& said) {
    if (stage_ != Stage::Waiting && stage_ != Stage::Running && stage_ != Stage::Fighting && stage_ != Stage::Ticket) return false;
    stage_ = Stage::HandsUp;
    bots.set_asleep(bot_, true);
    said = "Tased! He's down - USE to cuff him";
    hint("Good shot. He's done - walk up and USE to cuff him.");
    return true;
}

bool RoutineCalls::cuff(ai::BattleRoyaleBots& bots) {
    if (stage_ != Stage::HandsUp) return false;
    stage_ = Stage::Cuffed;
    bots.set_asleep(bot_, true);
    hint("Cuffed. Walk him to the car - get in close by and he climbs in the back.");
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
