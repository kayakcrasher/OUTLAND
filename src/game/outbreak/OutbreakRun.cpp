#include "outland/game/outbreak/OutbreakRun.hpp"
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/game/ai/BotDrivers.hpp"
#include "outland/game/combat/WeaponDefinition.hpp"
#include "outland/game/economy/Homes.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/game/zombies/ZombieEcology.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::outbreak {
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
float facing(Vector3 from, Vector3 to) { return std::atan2(to.x - from.x, to.z - from.z) * RAD2DEG; }
int weapon(combat::WeaponId id) { return static_cast<int>(id); }

// The floor under a point: a building's floor if there is one, else the ground.
Vector3 floor_at(const world::VerdaRegion& region, Vector3 at) {
    const float top = std::max(at.y, world::terrain::TerrainHeight::sample(at.x, at.z)) + 1.5F;
    const float floor = world::physics::WorldCollision::ground_height({at.x, top, at.z}, top, region);
    return {at.x, std::isfinite(floor) ? floor : world::terrain::TerrainHeight::sample(at.x, at.z), at.z};
}

// Where the farm is.
world::ranches::Frame the_farm() {
    for (const auto& r : world::ranches::ranches)
        if (r.id == ranch) return world::ranches::Frame(r.gate, r.up_track);
    return world::ranches::Frame(world::ranches::ranches[0].gate, world::ranches::ranches[0].up_track);
}

// The farm's points (u along the road, v up the track; the barn's front doors face the yard).
constexpr Vector2 barn_front{24, 53}, dario_spot{22, 53.5F}, ron_spot{27, 54}, truck_spot{8, 40}, arrive_spot{12, 42};
constexpr Vector2 hand_spots[5]{{-24, 58}, {-18, 59}, {-21, 72}, {-40, 50}, {36, 66}};
}

int Run::spawn(characters::NpcSystem& npcs, Person p) {
    p.bot = bot_base + static_cast<int>(people_.size());
    npcs.spawn_bot(p.bot, p.character, p.at, p.yaw);
    npcs.set_bot(p.bot, p.at, p.yaw, 100, characters::NpcState::Idle);
    if (p.weapon >= 0) npcs.set_bot_weapon(p.bot, p.weapon);
    npcs.set_bot_hidden(p.bot, p.hidden);
    people_.push_back(std::move(p));
    return static_cast<int>(people_.size()) - 1;
}

void Run::place(characters::NpcSystem& npcs, Person& p, Vector3 at, float yaw) {
    p.at = at;
    p.yaw = yaw;
    npcs.set_bot(p.bot, at, yaw, p.dead ? 0 : 100, p.dead ? characters::NpcState::Dead : characters::NpcState::Idle);
    npcs.set_bot_hidden(p.bot, p.hidden);
}

Person* Run::find(const std::string& name) {
    for (auto& p : people_) if (p.name == name) return &p;
    return nullptr;
}

const Person* Run::person(int bot) const {
    if (bot < bot_base || bot >= bot_base + static_cast<int>(people_.size())) return nullptr;
    return &people_[static_cast<std::size_t>(bot - bot_base)];
}

bool Run::found(int holdout) const {
    if (holdout < 0 || holdout >= static_cast<int>(holdouts_.size())) return false;
    const auto id = outbreak::holdouts[static_cast<std::size_t>(holdouts_[static_cast<std::size_t>(holdout)].spec)].id;
    return std::find(state_.found.begin(), state_.found.end(), id) != state_.found.end();
}

void Run::clear(characters::NpcSystem& npcs) {
    for (const auto& p : people_) npcs.despawn_bot(p.bot);
    people_.clear();
    infected_.clear();
    ranchers_.clear();
    rigs_.clear();
    active_ = false;
}

std::string Run::skip_to_farm(world::VerdaRegion& region, characters::NpcSystem& npcs, vehicles::VehicleSystem& cars) {
    if (state_.truck.empty() || cars.find(state_.truck) < 0) return {};
    story_.start(Stage::Drive);
    state_.stage = Stage::Drive;
    const Vector3 at = floor_at(region, ranch_.at(4, 22));
    cars.place(region, state_.truck, at, ranch_.yaw());
    ron_car_ = state_.truck;
    if (Person* ron = find(Story::ron)) { ron->hidden = true; place(npcs, *ron, at, ron->yaw); }
    return state_.truck;
}

std::vector<std::pair<Vector3, float>> Run::safe_ground() const {
    std::vector<std::pair<Vector3, float>> zones;
    for (const auto& h : holdouts_) zones.emplace_back(h.centre, h.radius);
    if (story_.stage() == Stage::Free) zones.emplace_back(hideout_, 120.0F);
    return zones;
}

void Run::start(const OutbreakState& state, world::VerdaRegion& region, characters::NpcSystem& npcs, vehicles::VehicleSystem& cars,
                zombies::ZombieEcology& zombies, const characters::CharacterRegistry* registry, bool respawn) {
    clear(npcs);
    active_ = true;
    registry_ = registry;
    state_ = state;
    events_ = {};
    saying_ = false;
    ron_car_.clear();
    fire_clock_ = -1;
    // Before the barn is theirs, the story starts again from the bed (a save from the middle of
    // it, or a death on the way).
    if (state_.stage != Stage::Free) state_.stage = Stage::WakeUp;
    story_.start(state_.stage);
    holdouts_ = find_holdouts(region);
    leader_talks_.assign(holdouts_.size(), 0);
    tower_ = find_tower(region);
    ranch_ = the_farm();
    const auto at = [&](Vector2 p) { return floor_at(region, ranch_.at(p)); };
    hideout_ = at(barn_front);
    const float to_gate = ranch_.yaw() + 180;

    const auto man = [&](std::uint64_t seed, characters::CharacterPool pool) {
        std::string body = "character_05";
        if (!registry) return body;
        for (std::uint64_t k = 0; k < 40; ++k)
            if (const auto* d = registry->choose(pool, seed + k * 13);
                d && d->id != "character_05" && d->id.find("female") == std::string::npos && d->id.find("police") == std::string::npos && d->id.find("doctor") == std::string::npos) {
                body = d->id;
                break;
            }
        return body;
    };
    // The family: Ron (as on Explore's quay) and Dario in his suit.
    Person ron;
    ron.name = Story::ron;
    ron.character = man(9100, characters::CharacterPool::Civilian);
    Person dario;
    dario.name = Story::dario;
    dario.character = "character_05";   // the outfit body: the game dresses him (the Green Hand's suit)
    dario.gang = 0;
    dario.weapon = weapon(combat::WeaponId::Pistol);

    if (story_.stage() == Stage::Free) {
        // At the hideout: the pickup in the yard, Ron and Dario by the barn doors.
        ron.at = at(ron_spot); ron.yaw = to_gate;
        dario.at = at(dario_spot); dario.yaw = to_gate;
        spawn(npcs, ron);
        spawn(npcs, dario);
        if (state_.truck.empty() || cars.find(state_.truck) < 0) {
            if (cars.spawn(region, at(truck_spot), ranch_.yaw(), "truck")) state_.truck = cars.last_spawned();
        }
        if (respawn || !state_.has_position) {
            start_feet_ = at({24, 49});
            start_yaw_ = facing(start_feet_, ranch_.at(barn_front)) * DEG2RAD;
        } else {
            start_feet_ = floor_at(region, state_.position);
            start_yaw_ = state_.yaw * DEG2RAD;
        }
    } else {
        // Ron's house in Verda City: the first of Okcidenta Verda's houses (as in Explore).
        const auto homes = economy::plan_homes(region);
        const economy::Listing* home = nullptr;
        for (const auto& h : homes) if (h.house) { home = &h; break; }
        if (home) {
            const float a = home->yaw * DEG2RAD, front = -home->size.z * .5F;
            const auto local = [&](float x, float z) {
                return Vector3{home->centre.x + x * std::cos(a) + z * std::sin(a), home->centre.y, home->centre.z - x * std::sin(a) + z * std::cos(a)};
            };
            // Shaken awake inside; Ron over the player.
            start_feet_ = floor_at(region, local(1.6F, 1.4F));
            ron.at = floor_at(region, local(.2F, .4F));
            ron.yaw = facing(ron.at, start_feet_);
            start_yaw_ = facing(start_feet_, ron.at) * DEG2RAD;
            // His pickup on the drive.
            Vector3 truck = local(home->size.x * .5F + 2.6F, front + 2.2F);
            for (const float side : {1.0F, -1.0F}) {
                truck = floor_at(region, local(side * (home->size.x * .5F + 2.6F), front + 2.2F));
                if (!world::physics::WorldCollision::body_blocked(truck, truck.y, region, 1.0F)) break;
            }
            if (!state_.truck.empty() && cars.find(state_.truck) >= 0) cars.place(region, state_.truck, truck, home->yaw + 180);
            else if (cars.spawn(region, truck, home->yaw + 180, "truck")) state_.truck = cars.last_spawned();
        } else {
            start_feet_ = floor_at(region, {0, 0, 0});
            ron.at = Vector3Add(start_feet_, {1.5F, 0, 0});
        }
        spawn(npcs, ron);
    }
    // The holdouts: a leader and guards on each.
    std::uint64_t seed = 4400;
    for (std::size_t k = 0; k < holdouts_.size(); ++k) {
        const auto& h = holdouts_[k];
        const auto& spec = outbreak::holdouts[static_cast<std::size_t>(h.spec)];
        for (int g = 0; g <= spec.guards; ++g) {
            Person p;
            p.holdout = static_cast<int>(k);
            p.leader = g == 0;
            p.gang = spec.gang;
            p.name = p.leader ? std::string(spec.leader) : std::string(spec.faction);
            p.character = spec.gang >= 0 ? std::string("character_05")
                        : spec.site == SiteKind::Prison ? std::string(g % 2 ? "character_18_police" : "character_17_police")
                        : man(seed += 31, characters::CharacterPool::Civilian);
            p.weapon = weapon(p.leader ? combat::WeaponId::Pistol : g % 2 ? combat::WeaponId::Rifle : combat::WeaponId::Shotgun);
            // The leader near the middle; the guards round the edge, looking out.
            const float angle = static_cast<float>(g) * 2 * PI / static_cast<float>(std::max(1, spec.guards)) + static_cast<float>(k);
            const float r = p.leader ? 4.0F : h.radius * .35F;
            Vector3 spot = h.centre;
            for (int attempt = 0; attempt < 8; ++attempt) {
                const float t = angle + static_cast<float>(attempt) * .55F, rr = r * (1 - static_cast<float>(attempt) * .08F);
                spot = floor_at(region, {h.centre.x + std::sin(t) * rr, h.centre.y, h.centre.z + std::cos(t) * rr});
                if (!world::physics::WorldCollision::body_blocked(spot, spot.y, region, .4F)) break;
            }
            p.at = spot;
            p.yaw = p.leader ? 0 : facing(h.centre, spot);
            spawn(npcs, p);
        }
    }
    // The Brotherhood's rigs: out of their camp, onto the roads between the holdouts.
    roads_.build(region);
    stops_.clear();
    for (const auto& h : holdouts_) stops_.push_back(h.centre);
    std::vector<std::string> kept;
    for (const auto& id : state_.rigs) if (cars.find(id) >= 0 && kept.size() < 2) kept.push_back(id);
    state_.rigs = kept;
    for (const auto& h : holdouts_) {
        if (outbreak::holdouts[static_cast<std::size_t>(h.spec)].site != SiteKind::Depot) continue;
        for (int k = static_cast<int>(state_.rigs.size()); k < 2; ++k) {
            const Vector3 at = floor_at(region, {h.centre.x + 14 + static_cast<float>(k) * 9, h.centre.y, h.centre.z + 22});
            if (cars.spawn(region, at, 0, "semi")) state_.rigs.push_back(cars.last_spawned());
        }
    }
    for (std::size_t k = 0; k < state_.rigs.size(); ++k) rigs_.push_back({state_.rigs[k], {}, 0, 0, 0, 0, static_cast<int>(k) * 3});
    zombies.set_safe(safe_ground());
    zombies.clear_area(start_feet_, 70);
}

void Run::roam(float dt, world::VerdaRegion& region, vehicles::VehicleSystem& cars, const std::string& driving) {
    (void)region;
    if (stops_.size() < 2) return;
    for (auto& rig : rigs_) {
        const int v = cars.find(rig.vehicle);
        const auto* asset = v >= 0 ? cars.asset(region, rig.vehicle) : nullptr;
        if (!asset || asset->vehicle.destroyed || rig.vehicle == driving) { cars.clear_autopilot(rig.vehicle); continue; }
        const Vector3 at = cars.vehicles()[static_cast<std::size_t>(v)].position;
        const bool done = !rig.route.complete || rig.route.points.size() < 2 || rig.route.length - rig.progress < 12 || rig.attempts > 8;
        if (done) {
            rig.leg = (rig.leg + 1) % static_cast<int>(stops_.size());
            if (flat(stops_[static_cast<std::size_t>(rig.leg)], at) < 150) rig.leg = (rig.leg + 1) % static_cast<int>(stops_.size());
            rig.route = roads_.route(at, stops_[static_cast<std::size_t>(rig.leg)]);
            rig.progress = rig.stuck = rig.reverse = 0;
            rig.attempts = 0;
        }
        ai::steer_along(rig.route, rig.progress, rig.stuck, rig.reverse, rig.attempts, rig.vehicle, cars, dt, 15, 7);
    }
}

std::optional<Vector3> Run::goal() const {
    switch (story_.stage()) {
        case Stage::WakeUp:
        case Stage::ToTruck:
            if (!people_.empty() && !people_.front().hidden) return people_.front().at;   // Ron, by the pickup
            return std::nullopt;
        case Stage::Drive: return ranch_.at(barn_front);
        case Stage::Clear: return ranch_.at(barn_front);
        default: return std::nullopt;
    }
}

int Run::infected_left() const {
    int n = 0;
    for (const int id : infected_) n += id >= 0;
    return n;
}

const Line* Run::line() const {
    if (const auto* l = story_.line()) return l;
    return saying_ ? &said_ : nullptr;
}

void Run::say(const Line& line, float seconds) {
    said_ = line;
    saying_ = true;
    said_for_ = seconds;
}

std::vector<Clearing> Run::clearings(Vector3 feet) const {
    return outbreak::clearings(state_, feet, hideout_, holdouts_);
}

std::string Run::prompt(Vector3 feet) const {
    if (story_.talking()) return "E - Continue";
    for (const auto& p : people_) {
        if (p.hidden || p.dead || flat(p.at, feet) > talk_range) continue;
        if (p.name == Story::ron || p.name == Story::dario) return std::string("E - Talk to ") + p.name;
        if (p.leader) return std::string("E - Talk to ") + p.name;
    }
    return {};
}

bool Run::use(Vector3 feet) {
    if (story_.talking()) return story_.talk();
    if (story_.stage() == Stage::WakeUp) return story_.talk();
    for (const auto& p : people_) {
        if (p.hidden || p.dead || flat(p.at, feet) > talk_range) continue;
        if ((p.name == Story::ron || p.name == Story::dario) && story_.stage() == Stage::Free) {
            // In turn, from whoever is nearer; Ron's lines are his and Dario's his.
            Line line = Story::free_line(state_.talks++);
            say(line);
            events_.save = true;
            return true;
        }
        if (p.leader && p.holdout >= 0) {
            const auto& spec = outbreak::holdouts[static_cast<std::size_t>(holdouts_[static_cast<std::size_t>(p.holdout)].spec)];
            int& turn = leader_talks_[static_cast<std::size_t>(p.holdout)];
            say(Line{spec.leader.data(), spec.lines[static_cast<std::size_t>(turn++ % 3)]});
            return true;
        }
    }
    if (saying_) { saying_ = false; return true; }
    return false;
}

// Everyone holding ground: the guards shoot the dead that come inside their walls, and Dario
// keeps the farm.
void Run::hold_ground(float dt, characters::NpcSystem& npcs, zombies::ZombieEcology& zombies) {
    (void)zombies;
    guard_clock_ += dt;
    if (guard_clock_ < .8F) return;
    guard_clock_ = 0;
    const auto& actors = npcs.actors();
    for (std::size_t k = 0; k < holdouts_.size(); ++k) {
        const auto& h = holdouts_[k];
        // The nearest of the dead inside.
        int target = -1;
        float best = h.radius;
        for (std::size_t i = 0; i < actors.size(); ++i) {
            const auto& a = actors[i];
            if (!zombies::ZombieEcology::is_zombie(a.resident) || a.state == characters::NpcState::Dead || a.health <= 0) continue;
            if (const float d = flat(a.position, h.centre); d < best) { best = d; target = static_cast<int>(i); }
        }
        if (target < 0) continue;
        const Vector3 aim = actors[static_cast<std::size_t>(target)].position;
        Person* shooter = nullptr;
        float near = 1e9F;
        for (auto& p : people_)
            if (p.holdout == static_cast<int>(k) && !p.dead && p.weapon >= 0)
                if (const float d = flat(p.at, aim); d < near) { near = d; shooter = &p; }
        if (!shooter || near > 70) continue;
        place(npcs, *shooter, shooter->at, facing(shooter->at, aim));
        npcs.damage(static_cast<std::size_t>(target), 70, shooter->at);
        events_.shots.push_back({Vector3Add(shooter->at, {0, 1.45F, 0}), Vector3Add(aim, {0, 1.2F, 0}), shooter->weapon});
    }
}

void Run::farm(const RunFrame& frame, world::VerdaRegion& region, characters::NpcSystem& npcs, vehicles::VehicleSystem& cars,
               zombies::ZombieEcology& zombies) {
    const auto at = [&](Vector2 p) { return floor_at(region, ranch_.at(p)); };
    Person* ron = find(Story::ron);
    Person* dario = find(Story::dario);
    const float to_gate = ranch_.yaw() + 180;
    const auto stage = story_.stage();
    // Ron gets in beside the player (whatever they drive, once he's done talking).
    if (ron && (stage == Stage::ToTruck || stage == Stage::Drive) && ron_car_.empty() && !frame.driving.empty()) {
        if (const int v = cars.find(frame.driving); v >= 0 && flat(cars.vehicles()[static_cast<std::size_t>(v)].position, ron->at) < 25) {
            ron_car_ = frame.driving;
            ron->hidden = true;
            place(npcs, *ron, ron->at, ron->yaw);
            if (stage == Stage::ToTruck) {
                story_.boarded();
                events_.message = "Uncle Ron climbs in beside you. \"Go - north, to the Garza place.\"";
            }
        }
    }
    // Riding along: where he is goes with the car; if it is wrecked or gone, he gets out.
    if (ron && !ron_car_.empty()) {
        const int v = cars.find(ron_car_);
        const auto* asset = v >= 0 ? cars.asset(region, ron_car_) : nullptr;
        if (!asset || asset->vehicle.destroyed) {
            ron->hidden = false;
            place(npcs, *ron, floor_at(region, v >= 0 ? cars.vehicles()[static_cast<std::size_t>(v)].position : ron->at), ron->yaw);
            ron_car_.clear();
        } else ron->at = cars.vehicles()[static_cast<std::size_t>(v)].position;
    }
    // At the farm: Ron gets out, Dario is at the barn, the Garzas come out with their guns.
    if (stage == Stage::Drive && ron && !ron_car_.empty()) {
        const int v = cars.find(ron_car_);
        if (v >= 0 && flat(cars.vehicles()[static_cast<std::size_t>(v)].position, ranch_.at(arrive_spot)) < 45 &&
            std::abs(cars.vehicles()[static_cast<std::size_t>(v)].speed) < 3) {
            const auto& car = cars.vehicles()[static_cast<std::size_t>(v)];
            const float a = car.yaw * DEG2RAD;
            ron->hidden = false;
            place(npcs, *ron, floor_at(region, {car.position.x - std::cos(a) * 2.2F, car.position.y, car.position.z + std::sin(a) * 2.2F}), to_gate + 180);
            ron_car_.clear();
            zombies.clear_area(ranch_.at(barn_front), 160);
            Person d;
            d.name = Story::dario;
            d.character = "character_05";
            d.gang = 0;
            d.weapon = weapon(combat::WeaponId::Pistol);
            d.at = at(dario_spot);
            d.yaw = facing(d.at, car.position);
            spawn(npcs, d);
            ranchers_.clear();
            for (int k = 0; k < 4; ++k) {
                Person r;
                r.name = k == 0 ? Story::rancher : "Garza";
                std::string body = "character_05";
                if (registry_)
                    for (std::uint64_t s = 0; s < 40; ++s)
                        if (const auto* def = registry_->choose(characters::CharacterPool::Civilian, 7300 + static_cast<std::uint64_t>(k) * 97 + s * 13);
                            def && def->id.find("police") == std::string::npos && def->id.find("doctor") == std::string::npos) {body = def->id; break;}
                r.character = body;
                r.weapon = weapon(k % 2 ? combat::WeaponId::Rifle : combat::WeaponId::Shotgun);
                // Across the track in front of the pickup, between it and the barn.
                const Vector3 ahead = Vector3Normalize(Vector3Subtract(ranch_.at(barn_front), car.position));
                const Vector3 side{-ahead.z, 0, ahead.x};
                const float across = (static_cast<float>(k) - 1.5F) * 2.6F;
                r.at = floor_at(region, Vector3Add(car.position, Vector3Add(Vector3Scale(ahead, 11.0F), Vector3Scale(side, across))));
                r.yaw = facing(r.at, car.position);
                ranchers_.push_back(spawn(npcs, r));
                npcs.set_bot(people_.back().bot, r.at, r.yaw, 100, characters::NpcState::Alert);
            }
            story_.arrived();
        }
    }
    // The shots: Dario drops them one after another.
    if (fire_clock_ >= 0 && dario) {
        const float before = fire_clock_;
        fire_clock_ += frame.dt;
        for (std::size_t k = 0; k < ranchers_.size(); ++k) {
            const float when = .15F + static_cast<float>(k) * .42F;
            if (before < when && fire_clock_ >= when) {
                auto& r = people_[static_cast<std::size_t>(ranchers_[k])];
                place(npcs, *dario, dario->at, facing(dario->at, r.at));
                r.dead = true;
                place(npcs, r, r.at, r.yaw);
                events_.shots.push_back({Vector3Add(dario->at, {0, 1.45F, 0}), Vector3Add(r.at, {0, 1.2F, 0}), dario->weapon});
            }
        }
        if (fire_clock_ > 2.5F) fire_clock_ = -1;
    }
    // Dario keeps the farm: the nearest of the dead coming at him or the barn.
    dario_clock_ += frame.dt;
    if (dario && !dario->hidden && (stage == Stage::Clear || stage == Stage::Free) && dario_clock_ >= 1.1F) {
        dario_clock_ = 0;
        const auto& actors = npcs.actors();
        int target = -1;
        float best = stage == Stage::Clear ? 16.0F : 28.0F;
        for (std::size_t i = 0; i < actors.size(); ++i) {
            const auto& a = actors[i];
            if (!zombies::ZombieEcology::is_zombie(a.resident) || a.state == characters::NpcState::Dead || a.health <= 0) continue;
            if (const float d = flat(a.position, dario->at); d < best) { best = d; target = static_cast<int>(i); }
        }
        if (target >= 0) {
            const Vector3 aim = actors[static_cast<std::size_t>(target)].position;
            place(npcs, *dario, dario->at, facing(dario->at, aim));
            npcs.damage(static_cast<std::size_t>(target), stage == Stage::Clear ? 45.0F : 70.0F, dario->at);
            events_.shots.push_back({Vector3Add(dario->at, {0, 1.45F, 0}), Vector3Add(aim, {0, 1.2F, 0}), dario->weapon});
        }
    }
    // Clearing the farm: the story's dead stay down.
    if (stage == Stage::Clear) {
        for (int& id : infected_) if (id >= 0 && zombies.dead(id)) id = -1;
        if (infected_left() == 0 && !infected_.empty()) {
            story_.cleared();
            // Into the barn's doorway.
            if (ron) place(npcs, *ron, at(ron_spot), to_gate);
            if (dario) place(npcs, *dario, at(dario_spot), to_gate);
        }
    }
}

const RunEvents& Run::update(const RunFrame& frame, world::VerdaRegion& region, characters::NpcSystem& npcs, vehicles::VehicleSystem& cars,
                             zombies::ZombieEcology& zombies) {
    events_ = RunEvents{};
    if (!active_ || !std::isfinite(frame.dt) || frame.dt <= 0) return events_;
    // A long frame (loading, a hitch) doesn't skip lines of a scene.
    RunFrame step = frame;
    step.dt = std::min(frame.dt, .1F);
    state_.played += step.dt;
    story_.update(step.dt);
    if (saying_ && (said_for_ -= step.dt) <= 0) saying_ = false;
    switch (story_.take_cue()) {
        case Cue::Fire: fire_clock_ = 0; break;
        case Cue::Rise: {
            // They get back up - and the farmhands with them.
            infected_.clear();
            for (const int k : ranchers_) {
                auto& r = people_[static_cast<std::size_t>(k)];
                r.hidden = true;
                place(npcs, r, r.at, r.yaw);
                infected_.push_back(zombies.spawn_at(r.at));
            }
            for (const auto& spot : hand_spots) infected_.push_back(zombies.spawn_at(floor_at(region, ranch_.at(spot))));
            events_.message = "The Garzas are getting back up - kill them all";
            break;
        }
        case Cue::Claimed:
            events_.rifle = true;
            events_.save = true;
            events_.message = "The barn is your hideout. Garza's hunting rifle is yours";
            zombies.set_safe(safe_ground());
            break;
        default: break;
    }
    if (story_.take_changed()) { state_.stage = story_.stage(); events_.save = state_.stage == Stage::Free; }
    farm(step, region, npcs, cars, zombies);
    roam(step.dt, region, cars, frame.driving);
    hold_ground(step.dt, npcs, zombies);
    // What the player comes across.
    for (std::size_t k = 0; k < holdouts_.size(); ++k) {
        const auto& h = holdouts_[k];
        if (found(static_cast<int>(k)) || flat(frame.feet, h.centre) > h.radius + 70) continue;
        const auto& spec = outbreak::holdouts[static_cast<std::size_t>(h.spec)];
        state_.found.emplace_back(std::string(spec.id));
        events_.message = std::string(spec.faction) + " hold out here - " + std::string(spec.leader) + " leads them";
        events_.save = true;
    }
    if (tower_.found && !state_.tower_seen && flat(frame.feet, tower_.centre) < 110) {
        state_.tower_seen = true;
        events_.message = "The black tower. No windows. No doors. Something inside is humming";
        events_.save = true;
    }
    if (story_.stage() == Stage::Free && frame.alive) {
        state_.has_position = true;
        state_.position = frame.feet;
    }
    return events_;
}

} // namespace outland::game::outbreak
