#include "outland/game/life/WalkingMan.hpp"
#include "outland/world/Highways.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/ai/BotDrivers.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/Zones.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::life {
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
float yaw_toward(Vector3 from, Vector3 to) { return std::atan2(to.x - from.x, to.z - from.z) * RAD2DEG; }
Vector3 ground(Vector3 p, const world::VerdaRegion& region) {
    const float terrain = world::terrain::TerrainHeight::sample(p.x, p.z);
    const float probe = std::max(terrain, p.y) + 1.2F;
    return {p.x, world::physics::WorldCollision::ground_height({p.x, probe, p.z}, probe, region), p.z};
}
std::uint64_t mix(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL; x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL; x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
// Real seconds per island minute: the island's clock runs `time_scale` times faster.
constexpr float time_scale = 30;
} // namespace

const char* paul_doing_name(PaulDoing doing) {
    switch (doing) {
        case PaulDoing::Home: return "home";
        case PaulDoing::DrivingIn: return "driving in";
        case PaulDoing::WalkingIn: return "walking into town";
        case PaulDoing::Gambling: return "gambling";
        case PaulDoing::ToBar: return "walking to the bar";
        case PaulDoing::Drinking: return "drinking";
        case PaulDoing::ToCar: return "walking to his car";
        case PaulDoing::DrivingHome: return "driving home";
        case PaulDoing::WalkingHome: return "walking home";
        case PaulDoing::Stumbling: return "stumbling home drunk";
        case PaulDoing::Defending: return "defending himself";
        case PaulDoing::Dead: return "dead";
    }
    return "?";
}

bool WalkingMan::drunk_on(int day) const {
    return mix(seed_ * 131 + static_cast<std::uint64_t>(day + 1000) * 7919) % 100 < 50;
}

bool WalkingMan::start(const std::vector<Place>& places, const world::VerdaRegion& region, Vector3 casino_door, Vector3 casino_table,
                       ai::BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, std::uint64_t seed, WorldClock clock) {
    active_ = false;
    bot_ = -1;
    seed_ = seed | 1;
    plan_ = {};
    message_.clear();
    message_time_ = 0;
    angry_until_ = last_threat_ = -1e9;
    aimed_ = 0;
    warned_ = riding_ = false;
    died_at_ = -1;
    // His car, and his trailer beside it.
    bool car_found = false;
    for (const auto& town : region.settlements())
        for (const auto& marker : town.gameplay_markers)
            if (marker.type == world::GameplayMarkerType::VehicleSpawn && marker.id == car_marker) {
                plan_.car_home = marker.position;
                plan_.car_home_yaw = marker.rotation_y;
                car_found = true;
            }
    if (!car_found) return false;
    for (const auto& car : cars.vehicles())
        if (car.id.ends_with(car_marker)) plan_.car = car.id;
    float nearest = 30;
    plan_.home_door = plan_.home = plan_.car_home;
    for (const auto& place : places) {
        if (place.kind != PlaceKind::Home) continue;
        const float d = flat(place.door, plan_.car_home);
        if (d < nearest) { nearest = d; plan_.home_door = place.door; plan_.home = place.interior; }
    }
    plan_.home_door = ground(plan_.home_door, region);
    plan_.home = ground(plan_.home, region);
    // The casino: his seat across the poker table from the door side.
    plan_.casino_door = ground(casino_door, region);
    Vector3 out = Vector3Subtract(casino_door, casino_table);
    out.y = 0;
    out = Vector3Length(out) > .1F ? Vector3Normalize(out) : Vector3{0, 0, 1};
    plan_.table = ground(Vector3Add(casino_table, Vector3Scale(out, 1.25F)), region);
    plan_.table_yaw = yaw_toward(plan_.table, casino_table);
    // The bar: the nearest other pub in town (not the casino).
    float best = 1e9F;
    bool bar = false;
    for (const auto& place : places) {
        if (place.kind != PlaceKind::Pub || !place.indoor || place.sealed) continue;
        if (flat(place.door, casino_door) < 8) continue;
        const float d = flat(place.door, casino_door);
        if (d < best && d < 700) {
            best = d;
            plan_.bar_door = place.door;
            Vector3 in = Vector3Subtract(place.interior, place.door);
            in.y = 0;
            const float l = Vector3Length(in);
            plan_.bar = l > .1F ? Vector3Add(place.door, Vector3Scale(in, std::min(1.0F, (l + 1.5F) / l))) : place.interior;
            plan_.bar_yaw = yaw_toward(plan_.bar_door, plan_.bar);
            bar = true;
        }
    }
    if (!bar) { plan_.bar_door = plan_.casino_door; plan_.bar = plan_.table; plan_.bar_yaw = plan_.table_yaw; plan_.bar_name = "the casino bar"; }
    else plan_.bar_name = "the pub";
    plan_.bar_door = ground(plan_.bar_door, region);
    plan_.bar = ground(plan_.bar, region);
    // Where he leaves the car: on the way home from town, just past the city limits (the Green
    // Core's edge), on the right-hand shoulder.
    roads_.build(region);
    const auto home_way = roads_.route(plan_.casino_door, plan_.car_home);
    plan_.parking = plan_.car_home;
    plan_.parking_yaw = plan_.car_home_yaw;
    if (home_way.complete && home_way.points.size() > 1) {
        float along = 0, past = -1;
        for (std::size_t i = 0; i + 1 < home_way.points.size(); ++i) {
            const Vector3 a = home_way.points[i], b = home_way.points[i + 1];
            const float length = flat(a, b);
            for (float s = 0; s < length; s += 4) {
                const Vector3 p = Vector3Lerp(a, b, s / std::max(length, .01F));
                if (past < 0 && world::zones::zone_at(p) != world::zones::ZoneId::Verdkerno) past = along + s;
                if (past >= 0 && along + s >= past + 35) {
                    const Vector3 d = Vector3Normalize({b.x - a.x, 0, b.z - a.z});
                    const Vector3 right{-d.z, 0, d.x};
                    const float lane = i < home_way.lane.size() ? home_way.lane[i] : 1.5F;
                    // Facing back toward town, parked on the shoulder of the town-bound side (a
                    // highway's hard shoulder, beyond the median).
                    const float aside = lane >= world::highways::slow_lane - .01F ? world::highways::lanes_outer + .6F : 2 * std::max(lane, 1.5F) + 2.4F;
                    plan_.parking = ground(Vector3Subtract(p, Vector3Scale(right, aside)), region);
                    plan_.parking_yaw = std::atan2(-d.x, -d.z) * RAD2DEG;
                    i = home_way.points.size();
                    break;
                }
            }
            along += length;
        }
    }
    auto& body = bots.add_bot(plan_.home, combat::WeaponId::Pistol, ai::BotLevel::Medium, seed_ * 977);
    bot_ = body.id();
    bots.set_asleep(bot_, true);
    active_ = true;
    last_minutes_ = clock.minutes;
    WalkingManFrame frame;
    frame.clock = clock;
    resync(frame, bots, cars, const_cast<world::VerdaRegion&>(region));
    return true;
}

Vector3 WalkingMan::position(const ai::BattleRoyaleBots& bots) const {
    for (const auto& bot : bots.bots()) if (bot.id() == bot_) return bot.position();
    return plan_.home;
}

std::string WalkingMan::activity() const {
    switch (doing_) {
        case PaulDoing::Home: return "at home in his trailer, Ruldomaro";
        case PaulDoing::DrivingIn: return "driving in to town";
        case PaulDoing::WalkingIn: return "walking into Verda City";
        case PaulDoing::Gambling: return "playing poker at the casino";
        case PaulDoing::ToBar: return "walking to " + plan_.bar_name;
        case PaulDoing::Drinking: return std::string(drunk_ ? "having one too many at " : "having a drink at ") + plan_.bar_name;
        case PaulDoing::ToCar: return "walking out to his car";
        case PaulDoing::DrivingHome: return "driving home";
        case PaulDoing::WalkingHome: return "walking home to Ruldomaro";
        case PaulDoing::Stumbling: return "drunk, stumbling home to the trailer park";
        case PaulDoing::Defending: return "defending himself!";
        case PaulDoing::Dead: return "dead";
    }
    return {};
}

bool WalkingMan::hostile_to(int a, int b, double now) const {
    if (!active_ || bot_ < 0) return false;
    const int player = ai::BattleRoyaleBots::player_id;
    if ((a == bot_ && b == player) || (b == bot_ && a == player)) return angry(now);
    return false;
}

void WalkingMan::provoke(int bot, double now) {
    if (!active_ || bot != bot_ || doing_ == PaulDoing::Dead) return;
    if (!angry(now)) say(std::string(name) + " draws his pistol - self defence!");
    angry_until_ = now + angry_seconds;
    last_threat_ = now;
}

bool WalkingMan::car_usable(const vehicles::VehicleSystem& cars, const world::VerdaRegion& region) const {
    if (plan_.car.empty() || cars.kinematic(plan_.car) || cars.autopiloted(plan_.car)) return false;
    if (cars.driver() && cars.driver()->id == plan_.car) return false;
    const auto* asset = cars.asset(region, plan_.car);
    return asset && asset->vehicle.enabled && !asset->vehicle.destroyed && asset->vehicle.health > 30;
}

Vector3 WalkingMan::car_position(const vehicles::VehicleSystem& cars) const {
    const int v = plan_.car.empty() ? -1 : cars.find(plan_.car);
    return v < 0 ? plan_.car_home : cars.vehicles()[static_cast<std::size_t>(v)].position;
}

void WalkingMan::walk(std::vector<Vector3> legs, PaulDoing doing, const world::VerdaRegion& region) {
    path_.clear();
    lengths_.clear();
    for (auto& p : legs) {
        if (!path_.empty() && flat(path_.back(), p) < .2F) continue;
        path_.push_back(ground(p, region));
    }
    float total = 0;
    lengths_.push_back(0);
    for (std::size_t i = 1; i < path_.size(); ++i) { total += flat(path_[i - 1], path_[i]); lengths_.push_back(total); }
    progress_ = 0;
    pause_ = 0;
    doing_ = doing;
}

void WalkingMan::walk_route(Vector3 from, Vector3 to, std::vector<Vector3> extra, PaulDoing doing, const world::VerdaRegion& region) {
    std::vector<Vector3> legs{from};
    const auto route = roads_.empty() ? world::roads::Route{} : roads_.route(from, to);
    if (route.complete && route.points.size() > 1) {
        // On the right-hand verge, not down the middle of the road.
        const auto& points = route.points;
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (i == 0 || i + 1 == points.size()) { legs.push_back(points[i]); continue; }
            const Vector3 in = Vector3Normalize({points[i].x - points[i - 1].x, 0, points[i].z - points[i - 1].z});
            const Vector3 out = Vector3Normalize({points[i + 1].x - points[i].x, 0, points[i + 1].z - points[i].z});
            Vector3 d = Vector3Add(in, out);
            d = Vector3Length(d) > .1F ? Vector3Normalize(d) : out;
            const Vector3 right{-d.z, 0, d.x};
            const float lane = std::max(i - 1 < route.lane.size() ? route.lane[i - 1] : 0.0F, i < route.lane.size() ? route.lane[i] : 0.0F);
            legs.push_back(lane > 0 ? Vector3Add(points[i], Vector3Scale(right, 2 * lane + 1.3F)) : points[i]);
        }
    }
    legs.push_back(to);
    for (const auto& p : extra) legs.push_back(p);
    walk(std::move(legs), doing, region);
}

void WalkingMan::settle(PaulDoing doing, Vector3 at, float yaw, ai::BattleRoyaleBots& bots, const world::VerdaRegion& region) {
    path_.clear();
    lengths_.clear();
    doing_ = doing;
    if (auto* body = bots.find(bot_)) {
        body->set_position(ground(at, region));
        body->set_yaw(yaw);
        body->puppet(body->position(), yaw, 0);
    }
}

// Puts him (and his car) where the clock says he is, at once: the start of a session, after the
// player sleeps through the night.
void WalkingMan::resync(const WalkingManFrame& frame, ai::BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region) {
    if (doing_ == PaulDoing::Dead) return;
    riding_ = false;
    const float h = frame.clock.hour();
    const int day = frame.clock.day();
    const bool morning = h < leave_hour;
    drunk_ = drunk_on(morning ? day - 1 : day);
    const auto park = [&](bool out) {
        if (plan_.car.empty() || !car_usable(cars, region)) return;
        cars.place(region, plan_.car, out ? plan_.parking : plan_.car_home, out ? plan_.parking_yaw : plan_.car_home_yaw);
    };
    wake_hour_ = leave_hour;
    if (h >= leave_hour && h < bar_hour) {
        day_ = day; park(true);
        settle(PaulDoing::Gambling, plan_.table, plan_.table_yaw, bots, region);
    } else if (h >= bar_hour && h < closing_hour) {
        day_ = day; park(true);
        settle(PaulDoing::Drinking, plan_.bar, plan_.bar_yaw, bots, region);
    } else if (drunk_ && (h >= closing_hour || h < 6)) {
        // Somewhere on the long walk home.
        day_ = h >= closing_hour ? day : day - 1;
        park(true);
        walk_route(plan_.bar_door, plan_.home_door, {plan_.home}, PaulDoing::Stumbling, region);
        const float minutes = (h >= closing_hour ? h - closing_hour : h + 24 - closing_hour) * 60;
        progress_ = minutes * 60 / time_scale * stumble_speed * .8F;
        if (lengths_.empty() || progress_ >= lengths_.back()) {
            settle(PaulDoing::Home, plan_.home, 0, bots, region);
            wake_hour_ = std::min(13.0F, leave_hour + 2);
        } else if (auto* body = bots.find(bot_)) {
            body->set_position(path_.front());
        }
    } else {
        day_ = morning ? day - 1 : day;
        park(morning && drunk_);
        settle(PaulDoing::Home, plan_.home, 0, bots, region);
    }
}

void WalkingMan::drive(Vector3 goal, float goal_yaw, PaulDoing doing, const WalkingManFrame& frame, ai::BattleRoyaleBots& bots,
                       ai::BotDrivers& drivers, vehicles::VehicleSystem& cars, world::VerdaRegion& region) {
    const Vector3 here = position(bots);
    const Vector3 car = car_position(cars);
    const bool seen = flat(here, frame.player_feet) < drive_watch || flat(goal, frame.player_feet) < drive_watch;
    doing_ = doing;
    path_.clear();
    if (seen && drivers.dispatch(bots, cars, region, {bot_}, car, goal, 30, plan_.car)) { riding_ = true; return; }
    // Unseen: the trip happens off-screen.
    cars.place(region, plan_.car, goal, goal_yaw);
    const float a = goal_yaw * DEG2RAD;
    const Vector3 side{-std::cos(a), 0, std::sin(a)}; // the driver's door (left)
    if (auto* body = bots.find(bot_)) body->set_position(ground(Vector3Add(goal, Vector3Scale(side, 2.4F)), region));
    riding_ = false;
}

bool WalkingMan::threatened(const WalkingManFrame& frame, Vector3 feet) {
    if (!frame.player_alive || frame.player_in_vehicle) { aimed_ = 0; return false; }
    const Vector3 chest{feet.x, feet.y + 1.2F, feet.z};
    const Vector3 direction = Vector3Length(frame.aim_direction) > 1e-4F ? Vector3Normalize(frame.aim_direction) : Vector3{0, 0, 1};
    const Vector3 to = Vector3Subtract(chest, frame.aim_origin);
    const float along = Vector3DotProduct(to, direction);
    const float miss = Vector3Length(Vector3Subtract(to, Vector3Scale(direction, std::max(0.0F, along))));
    // Shot at (or past) him.
    if (frame.player_fired && along > 0 && along < 120 && miss < 2.2F) return true;
    // A gun held on him close up: he warns, then draws.
    const bool covered = frame.player_aiming && along > 0 && along < aim_range && miss < 1.0F &&
                         (!frame.line_of_sight || frame.line_of_sight(frame.aim_origin, chest));
    if (covered) {
        aimed_ += frame.dt;
        if (aimed_ >= warn_seconds && !warned_) {
            warned_ = true;
            say(std::string(name) + ": \"Whoa - easy, friend. Put that down.\"");
        }
        if (aimed_ >= aim_seconds) return true;
    } else {
        aimed_ = std::max(0.0F, aimed_ - frame.dt * .5F);
        if (aimed_ <= 0) warned_ = false;
    }
    return false;
}

void WalkingMan::next_stage(const WalkingManFrame& frame, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                            vehicles::VehicleSystem& cars, world::VerdaRegion& region) {
    const float h = frame.clock.hour();
    const int day = frame.clock.day();
    const Vector3 here = position(bots);
    const bool walking = !path_.empty();
    const bool arrived = walking && progress_ >= lengths_.back() - .05F;
    // Wherever the clock says he belongs now, on foot from where he is.
    const auto head_out = [&] {
        const Vector3 from = position(bots);
        if (h >= bar_hour && h < closing_hour) walk_route(from, plan_.bar_door, {plan_.bar}, PaulDoing::ToBar, region);
        else if (h >= closing_hour || h < 5) walk_route(from, plan_.home_door, {plan_.home}, drunk_ ? PaulDoing::Stumbling : PaulDoing::WalkingHome, region);
        else walk_route(from, plan_.casino_door, {plan_.table}, PaulDoing::WalkingIn, region);
    };
    switch (doing_) {
        case PaulDoing::Home:
            if (day > day_ && h >= wake_hour_ && h < closing_hour) {
                day_ = day;
                wake_hour_ = leave_hour;
                drunk_ = drunk_on(day);
                if (car_usable(cars, region) && flat(car_position(cars), plan_.car_home) < 40) {
                    walk({here, plan_.home_door}, PaulDoing::DrivingIn, region); // out of the door first
                } else {
                    // The car is still out by the city limits (or gone): he walks.
                    walk_route(plan_.home_door, plan_.casino_door, {plan_.table}, PaulDoing::WalkingIn, region);
                    path_.insert(path_.begin(), here);
                    walk(path_, PaulDoing::WalkingIn, region);
                }
            }
            break;
        case PaulDoing::DrivingIn:
            if (riding_) {
                if (!drivers.ride_of(bot_)) { riding_ = false; head_out(); }
            } else if (arrived) {
                drive(plan_.parking, plan_.parking_yaw, PaulDoing::DrivingIn, frame, bots, drivers, cars, region);
                if (!riding_) head_out();
            }
            break;
        case PaulDoing::WalkingIn:
            if (arrived) settle(PaulDoing::Gambling, plan_.table, plan_.table_yaw, bots, region);
            break;
        case PaulDoing::Gambling:
            if (h >= bar_hour || h < leave_hour) walk_route(here, plan_.bar_door, {plan_.bar}, PaulDoing::ToBar, region);
            break;
        case PaulDoing::ToBar:
            if (arrived) settle(PaulDoing::Drinking, plan_.bar, plan_.bar_yaw, bots, region);
            break;
        case PaulDoing::Drinking:
            if (h >= closing_hour || h < leave_hour) {
                if (drunk_) {
                    say(std::string(name) + " has had a few too many - he's walking home");
                    walk_route(here, plan_.home_door, {plan_.home}, PaulDoing::Stumbling, region);
                } else if (car_usable(cars, region) && flat(car_position(cars), plan_.parking) < 60) {
                    const Vector3 car = car_position(cars);
                    walk_route(here, car, {}, PaulDoing::ToCar, region);
                } else {
                    walk_route(here, plan_.home_door, {plan_.home}, PaulDoing::WalkingHome, region);
                }
            }
            break;
        case PaulDoing::ToCar:
            if (arrived) {
                if (car_usable(cars, region) && flat(car_position(cars), here) < 12) {
                    drive(plan_.car_home, plan_.car_home_yaw, PaulDoing::DrivingHome, frame, bots, drivers, cars, region);
                    if (!riding_) walk({position(bots), plan_.home_door, plan_.home}, PaulDoing::WalkingHome, region);
                } else {
                    walk_route(here, plan_.home_door, {plan_.home}, PaulDoing::WalkingHome, region);
                }
            }
            break;
        case PaulDoing::DrivingHome:
            if (riding_ && !drivers.ride_of(bot_)) {
                riding_ = false;
                walk({position(bots), plan_.home_door, plan_.home}, PaulDoing::WalkingHome, region);
            }
            break;
        case PaulDoing::WalkingHome:
        case PaulDoing::Stumbling:
            if (arrived) {
                // In late from a long night: he sleeps in.
                const bool late = h >= 4 && h < 12;
                wake_hour_ = late ? std::clamp(h + 3.5F, leave_hour, 13.5F) : leave_hour;
                settle(PaulDoing::Home, plan_.home, 0, bots, region);
            }
            break;
        case PaulDoing::Defending:
        case PaulDoing::Dead:
            break;
    }
}

void WalkingMan::update(const WalkingManFrame& frame, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                        vehicles::VehicleSystem& cars, world::VerdaRegion& region) {
    if (!active_ || !std::isfinite(frame.dt) || frame.dt <= 0) return;
    auto* body = bots.find(bot_);
    if (!body) { active_ = false; return; }
    message_time_ = std::max(0.0F, message_time_ - frame.dt);
    const double now = frame.now;
    const float h = frame.clock.hour();
    const int day = frame.clock.day();

    // Dead: back home the next morning, when nobody is watching the trailer.
    if (!body->alive()) {
        if (doing_ != PaulDoing::Dead) {
            doing_ = PaulDoing::Dead;
            died_at_ = frame.clock.minutes;
            angry_until_ = -1e9;
            path_.clear();
            bots.set_asleep(bot_, true);
        }
        const bool next_morning = frame.clock.minutes - died_at_ > 6 * 60 && h >= 6 && h < 12;
        if (next_morning && flat(plan_.home, frame.player_feet) > 60) {
            bots.respawn(bot_, plan_.home, 0, seed_ + static_cast<std::uint64_t>(day) * 31);
            bots.set_asleep(bot_, true);
            doing_ = PaulDoing::Home;
            day_ = day - 1;
            wake_hour_ = leave_hour;
        }
        last_minutes_ = frame.clock.minutes;
        return;
    }

    // The player slept (or the clock jumped): he is where his day says.
    if (last_minutes_ >= 0 && std::abs(frame.clock.minutes - last_minutes_) > 45 && doing_ != PaulDoing::Defending)
        resync(frame, bots, cars, region);
    last_minutes_ = frame.clock.minutes;

    if (!frame.player_alive) angry_until_ = std::min(angry_until_, now); // the fight is over
    const bool mounted = bots.mounted(bot_);
    if (!mounted && doing_ != PaulDoing::Defending && threatened(frame, body->position())) provoke(bot_, now);

    // Self defence: the combat brain takes over.
    if (angry(now)) {
        if (doing_ != PaulDoing::Defending && !mounted) {
            doing_ = PaulDoing::Defending;
            path_.clear();
            riding_ = false;
            bots.set_asleep(bot_, false);
            body->set_roam_center(body->position(), 6);
        }
        return;
    }
    if (doing_ == PaulDoing::Defending) {
        // It is over: holster, and get on with the day from wherever he is.
        bots.set_asleep(bot_, true);
        aimed_ = 0;
        warned_ = false;
        say(std::string(name) + " puts his pistol away");
        doing_ = PaulDoing::WalkingIn;
        if (h >= bar_hour && h < closing_hour) walk_route(body->position(), plan_.bar_door, {plan_.bar}, PaulDoing::ToBar, region);
        else if (h >= leave_hour && h < bar_hour) walk_route(body->position(), plan_.casino_door, {plan_.table}, PaulDoing::WalkingIn, region);
        else walk_route(body->position(), plan_.home_door, {plan_.home}, drunk_ ? PaulDoing::Stumbling : PaulDoing::WalkingHome, region);
    }
    bots.set_asleep(bot_, true);
    if (mounted) return; // at the wheel: BotDrivers moves him

    // On foot along the path.
    if (!path_.empty() && lengths_.size() == path_.size()) {
        const bool stumbling = doing_ == PaulDoing::Stumbling;
        // Out of the player's sight the walk keeps up with the island's quick clock.
        const bool watched = flat(body->position(), frame.player_feet) < watch_radius;
        float speed = (stumbling ? stumble_speed : walk_speed) * (watched ? 1.0F : unseen_pace);
        if (stumbling) {
            // Now and then he stops to get his bearings (or lean on something).
            if (pause_ > 0) { pause_ -= frame.dt; speed = 0; }
            else if (std::fmod(progress_, 47.0F) < speed * frame.dt) pause_ = 1.5F + static_cast<float>(mix(static_cast<std::uint64_t>(progress_ * 10)) % 25) / 10.0F;
        }
        progress_ = std::min(progress_ + speed * frame.dt, lengths_.back());
        std::size_t i = 1;
        while (i + 1 < lengths_.size() && lengths_[i] < progress_) ++i;
        const float span = std::max(.01F, lengths_[i] - lengths_[i - 1]);
        const float t = std::clamp((progress_ - lengths_[i - 1]) / span, 0.0F, 1.0F);
        Vector3 at = Vector3Lerp(path_[i - 1], path_[i], t);
        float yaw = yaw_toward(path_[i - 1], path_[i]);
        if (stumbling) {
            // Weaving: off the line and back, the body swinging with it.
            const Vector3 d = Vector3Normalize({path_[i].x - path_[i - 1].x, 0, path_[i].z - path_[i - 1].z});
            const Vector3 right{-d.z, 0, d.x};
            const float weave = .85F * std::sin(progress_ * .42F) + .35F * std::sin(progress_ * 1.37F + 1);
            const float fade = std::min({1.0F, progress_ / 4, (lengths_.back() - progress_) / 4});
            at = Vector3Add(at, Vector3Scale(right, weave * fade));
            yaw += 28 * std::cos(progress_ * .42F) * fade;
        }
        at = ground(at, region);
        body->puppet(at, yaw, frame.dt);
    } else {
        // Sitting at the table, at the bar, at home: still.
        float yaw = body->yaw();
        if (doing_ == PaulDoing::Gambling) yaw = plan_.table_yaw;
        else if (doing_ == PaulDoing::Drinking) yaw = plan_.bar_yaw + (drunk_ && h > 19 ? 8 * std::sin(static_cast<float>(now) * .7F) : 0.0F);
        body->puppet(body->position(), yaw, frame.dt);
    }
    next_stage(frame, bots, drivers, cars, region);
}

void WalkingMan::dress(characters::NpcSystem& npcs) const {
    if (!active_ || bot_ < 0) return;
    // The pistol stays in his waistband until he needs it.
    if (doing_ != PaulDoing::Defending) npcs.set_bot_weapon(bot_, -1);
}

} // namespace outland::game::life
