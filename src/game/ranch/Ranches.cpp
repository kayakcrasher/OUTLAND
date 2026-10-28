#include "outland/game/ranch/Ranches.hpp"
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/roads/RoadGraph.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace outland::game::ranch {
namespace rr = world::ranches;
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
std::uint64_t mix(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL; x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL; x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
int alive_head(const RanchState& r) {
    return static_cast<int>(std::count_if(r.herd.begin(), r.herd.end(), [](const Cow& c) { return c.state != CowState::Dead && c.state != CowState::Gone; }));
}
Vector3 on_ground(Vector3 p) { return {p.x, world::terrain::TerrainHeight::sample(p.x, p.z), p.z}; }
float ground_at(Vector3 p, const world::VerdaRegion& region) {
    const float terrain = world::terrain::TerrainHeight::sample(p.x, p.z);
    return world::physics::WorldCollision::ground_height({p.x, terrain + 1.2F, p.z}, terrain + 1.2F, region);
}
constexpr std::array<const char*, 10> men{"Rafael", "Diego", "Joaquin", "Emilio", "Santiago", "Mateo", "Tomas", "Rodrigo", "Ernesto", "Lalo"};
constexpr std::array<const char*, 10> women{"Lupe", "Rosa", "Elena", "Carmen", "Ines", "Marisol", "Ana", "Pilar", "Dolores", "Paloma"};
// The chute (world/Ranches.hpp, model ramp): the ground end and the top, in the ranch frame.
constexpr float chute_foot_v = rr::chute.y + 2.2F + 3.2F, chute_top_v = rr::chute.y + 2.2F - 1.7F, chute_height = 1.35F;
bool frame_of(const world::Settlement& town, rr::Frame& frame) {
    if (town.roads.empty()) return false;
    const auto& track = town.roads.front();
    Vector3 d{track.end.x - track.start.x, 0, track.end.z - track.start.z};
    if (Vector3Length(d) < 1) return false;
    frame.gate = on_ground(track.start);
    frame.up_track = Vector3Normalize(d);
    return true;
}
}

float Ranches::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 7; rng_ ^= rng_ << 17;
    return static_cast<float>(rng_ >> 40) / static_cast<float>(1ULL << 24);
}

Vector3 Ranches::pasture_point(const RanchState& r) {
    const auto& a = rr::pasture;
    for (int tries = 0; tries < 8; ++tries) {
        const float u = a.u0 + 4 + random() * (a.u1 - a.u0 - 8), v = a.v0 + 4 + random() * (a.v1 - a.v0 - 8);
        if (std::hypot(u - rr::windmill.x, v - rr::windmill.y) < 5) continue;
        return on_ground(r.frame.at(u, v));
    }
    return on_ground(r.frame.at((a.u0 + a.u1) * .5F, (a.v0 + a.v1) * .5F));
}

int Ranches::start(const world::VerdaRegion& region, ai::BattleRoyaleBots& bots, vehicles::VehicleSystem& cars,
                   const characters::CharacterRegistry* characters, std::uint64_t seed, life::WorldClock clock) {
    ranches_.clear();
    vakeros_.clear();
    stockyard_ = {};
    quay_ = {};
    message_.clear();
    rng_ = (seed ^ 0x9a3c51f7d2e4b081ULL) | 1;
    // Bodies: everyday civilians (no uniforms), men and women.
    std::vector<std::string> male, female;
    if (characters)
        for (const auto* d : characters->pool(characters::CharacterPool::Civilian)) {
            if (d->id.find("police") != std::string::npos || d->id.find("doctor") != std::string::npos) continue;
            (d->id.find("female") != std::string::npos ? female : male).push_back(d->id);
        }
    // The cattle quay, where the cargo terminal has its gangway.
    for (const auto& town : region.settlements())
        for (const auto& asset : town.assets)
            if (asset.model_path.ends_with("civic/cattle_gangway.glb")) {
                quay_.found = true;
                quay_.frame = rr::Frame(rr::cattle_quay_gate, rr::cattle_quay_up);
                quay_.frame.gate = on_ground(quay_.frame.gate);
            }
    for (const auto& town : region.settlements()) {
        if (town.id == rr::stockyard_id) {
            stockyard_.found = frame_of(town, stockyard_.frame);
            // A busy market: cattle already penned.
            for (int i = 0; stockyard_.found && i < stockyard_size / 2; ++i) {
                Cow cow;
                cow.position = stockyard_spot(i % 2);
                cow.yaw = random() * 360;
                cow.angus = random() < .4F;
                cow.state = CowState::InCorral;
                stockyard_.pen.push_back(cow);
            }
            continue;
        }
        const auto spec = std::find_if(rr::ranches.begin(), rr::ranches.end(), [&](const rr::Ranch& r) { return r.id == town.id; });
        if (spec == rr::ranches.end()) continue;
        RanchState r;
        r.spec = &*spec;
        r.settlement = town.id;
        if (!frame_of(town, r.frame)) continue;
        const int index = static_cast<int>(ranches_.size());
        for (int i = 0; i < herd_size; ++i) {
            Cow cow;
            cow.position = cow.target = pasture_point(r);
            cow.yaw = random() * 360;
            cow.angus = random() < .4F;
            cow.pause = random() * 10;
            r.herd.push_back(cow);
        }
        r.calved_day = clock.day();
        // The family: the rancher (herd), the wife (fields), a son (the side-by-side), a daughter (barn).
        const std::uint64_t family_seed = mix(seed + static_cast<std::uint64_t>(index) * 7919);
        for (int k = 0; k < family_size; ++k) {
            Vakero v;
            v.ranch = index;
            v.role = static_cast<Role>(k);
            v.female = k == 1 || k == 3;
            const auto& names = v.female ? women : men;
            v.name = std::string(names[(family_seed % names.size() + static_cast<std::size_t>(k) * 3) % names.size()]) + " " + std::string(spec->family);
            const auto& bodies = v.female ? female : male;
            if (!bodies.empty()) v.character = bodies[(family_seed / 3 + static_cast<std::uint64_t>(k) * 13 + static_cast<std::uint64_t>(index) * 5) % bodies.size()];
            const Vector3 at = on_ground(r.frame.at(rr::yard.x + static_cast<float>(k) * 1.5F - 2, rr::yard.y + 2));
            auto& bot = bots.add_bot(at, combat::WeaponId::Shotgun, ai::BotLevel::Medium, family_seed + static_cast<std::uint64_t>(k));
            bot.set_roam_center(at, 4);
            v.bot = bot.id();
            bots.set_asleep(v.bot, true);
            r.family.push_back(static_cast<int>(vakeros_.size()));
            vakeros_.push_back(std::move(v));
        }
        // The side-by-side and its farm-road loop: round the fields and back to the yard.
        const std::string marker = "vehicle_spawn_sxs_" + town.id.substr(6) + "_1";
        for (const auto& car : cars.vehicles()) if (car.id.ends_with(marker)) r.sxs = car.id;
        for (const Vector2 p : {Vector2{rr::sxs.x, rr::sxs.y}, Vector2{8, 80}, Vector2{72, 80}, Vector2{72, 126}, Vector2{-28, 126},
                                Vector2{-28, 80}, Vector2{8, 80}, Vector2{rr::sxs.x, rr::sxs.y}})
            r.loop.push_back(on_ground(r.frame.at(p)));
        for (std::size_t i = 1; i < r.loop.size(); ++i) r.loop_length += flat(r.loop[i - 1], r.loop[i]);
        ranches_.push_back(std::move(r));
    }
    return static_cast<int>(ranches_.size());
}

void Ranches::clear(vehicles::VehicleSystem& cars) {
    for (auto& r : ranches_) if (r.sxs_running) cars.release(r.sxs);
    ranches_.clear();
    vakeros_.clear();
    quay_ = {};
    stockyard_ = {};
}

const Vakero* Ranches::vakero(int bot) const {
    for (const auto& v : vakeros_) if (v.bot == bot) return &v;
    return nullptr;
}

bool Ranches::angry(int ranch, double now) const {
    return ranch >= 0 && static_cast<std::size_t>(ranch) < ranches_.size() && ranches_[static_cast<std::size_t>(ranch)].provoked_until > now;
}

void Ranches::provoke_ranch(int ranch, double now, const std::string& why) {
    if (ranch < 0 || static_cast<std::size_t>(ranch) >= ranches_.size()) return;
    auto& r = ranches_[static_cast<std::size_t>(ranch)];
    if (!angry(ranch, now)) say(why.empty() ? "The " + std::string(r.spec->family) + "s reach for their shotguns" : why);
    r.provoked_until = now + provoked_seconds;
}

void Ranches::provoke(int bot, double now) {
    if (const auto* v = vakero(bot)) provoke_ranch(v->ranch, now, v->name + " is hit - the " + std::string(ranches_[static_cast<std::size_t>(v->ranch)].spec->family) + "s grab their shotguns");
}

void Ranches::forgive() {
    for (auto& r : ranches_) r.provoked_until = -1e9;
}

bool Ranches::hostile_to(int a, int b, double now) const {
    const int player = ai::BattleRoyaleBots::player_id;
    if (a != player && b != player) return false;
    const auto* v = vakero(a == player ? b : a);
    return v && angry(v->ranch, now);
}

int Ranches::ranch_at(Vector3 p) const {
    for (std::size_t i = 0; i < ranches_.size(); ++i)
        if (ranches_[i].frame.inside(p, rr::footprint)) return static_cast<int>(i);
    return -1;
}

std::vector<SeatedDriver> Ranches::drivers() const {
    std::vector<SeatedDriver> out;
    for (const auto& r : ranches_) {
        if (!r.sxs_running) continue;
        for (const int k : r.family)
            if (vakeros_[static_cast<std::size_t>(k)].role == Role::Driver) out.push_back({r.sxs, vakeros_[static_cast<std::size_t>(k)].character, true});
    }
    return out;
}

// ------------------------------------------------------------------------------------ cattle
void Ranches::call_cattle(int ranch, int count) {
    if (ranch < 0 || static_cast<std::size_t>(ranch) >= ranches_.size()) return;
    auto& r = ranches_[static_cast<std::size_t>(ranch)];
    r.calling = true;
    const float lane_u = (rr::lane.u0 + rr::lane.u1) * .5F;
    for (auto& cow : r.herd) {
        if (count <= 0) break;
        if (cow.state != CowState::Grazing && cow.state != CowState::Walking) continue;
        cow.state = CowState::ToCorral;
        cow.path = {on_ground(r.frame.at(lane_u, rr::pasture.v1 - 4)), on_ground(r.frame.at(lane_u, rr::lane.v1 + 3)),
                    on_ground(r.frame.at(rr::corral.u0 + 3 + random() * (rr::corral.u1 - rr::corral.u0 - 6), rr::corral.v0 + 3 + random() * 7))};
        --count;
    }
}

int Ranches::in_corral(int ranch) const {
    if (ranch < 0 || static_cast<std::size_t>(ranch) >= ranches_.size()) return 0;
    const auto& herd = ranches_[static_cast<std::size_t>(ranch)].herd;
    return static_cast<int>(std::count_if(herd.begin(), herd.end(), [](const Cow& c) { return c.state == CowState::InCorral; }));
}

int Ranches::load_cattle(int ranch, int max) {
    if (ranch < 0 || static_cast<std::size_t>(ranch) >= ranches_.size()) return 0;
    auto& r = ranches_[static_cast<std::size_t>(ranch)];
    int loaded = 0;
    for (auto& cow : r.herd) {
        if (loaded >= max) break;
        if (cow.state != CowState::InCorral) continue;
        cow.state = CowState::Loading;
        const rr::Frame load = rr::load_frame(r.frame);
        const Vector3 foot = on_ground(load.at(rr::chute.x, chute_foot_v + 1.5F));
        const Vector3 base = on_ground(load.at(rr::chute.x, chute_foot_v));
        Vector3 top = on_ground(load.at(rr::chute.x, chute_top_v));
        top.y += chute_height;
        Vector3 in = on_ground(load.at(rr::chute.x, -2.5F));
        in.y = top.y;
        cow.path = {foot, base, top, in};
        cow.pause = static_cast<float>(loaded) * 2.5F; // single file up the chute
        ++loaded;
    }
    r.shipped += loaded;
    r.calling = false;
    return loaded;
}

void Ranches::unload_cattle(int count) {
    if (!stockyard_.found) return;
    const auto& f = stockyard_.frame;
    for (int i = 0; i < count; ++i) {
        Cow cow;
        Vector3 top = on_ground(f.at(rr::stockyard_chute.x, chute_top_v));
        top.y += chute_height;
        cow.position = top;
        cow.angus = random() < .4F;
        cow.state = CowState::ToCorral;
        cow.pause = static_cast<float>(i) * 2.0F;
        // Down the chute into the receiving pen, then through to one of the holding pens.
        const int side = random() < .5F ? 0 : 1;
        const float gate_u = side == 0 ? rr::stockyard_receiving.u0 : rr::stockyard_receiving.u1;
        const float out = side == 0 ? -3.0F : 3.0F;
        cow.path = {on_ground(f.at(rr::stockyard_chute.x, chute_foot_v)), on_ground(f.at(rr::stockyard_chute.x, 15)),
                    on_ground(f.at(gate_u - out, 17)), on_ground(f.at(gate_u + out, 17)), stockyard_spot(side)};
        stockyard_.pen.push_back(cow);
    }
    stockyard_.received += count;
    while (stockyard_.pen.size() > static_cast<std::size_t>(stockyard_size)) stockyard_.pen.erase(stockyard_.pen.begin()); // sold at auction
}

int Ranches::ship_cattle(int ranch, int count) {
    if (count <= 0) return 0;
    if (quay_.found) {
        const auto& f = quay_.frame;
        const auto& pen = rr::cattle_quay_pen;
        for (int i = 0; i < count; ++i) {
            Cow cow;
            Vector3 top = on_ground(f.at(rr::cattle_quay_chute.x, rr::cattle_quay_chute.y + 2.2F - 1.7F));
            top.y += chute_height;
            cow.position = top;
            cow.angus = random() < .4F;
            cow.state = CowState::ToCorral;
            cow.pause = static_cast<float>(i) * 2.0F;
            cow.path = {on_ground(f.at(rr::cattle_quay_chute.x, rr::cattle_quay_chute.y + 2.2F + 3.2F)),
                        on_ground(f.at(pen.u0 + 3 + random() * (pen.u1 - pen.u0 - 6), pen.v0 + 4 + random() * (pen.v1 - pen.v0 - 7)))};
            quay_.pen.push_back(cow);
        }
        quay_.boarding = 40 + 2.0F * static_cast<float>(count);
    }
    quay_.shipped += count;
    if (ranch < 0 || static_cast<std::size_t>(ranch) >= ranches_.size()) return 0;
    auto& r = ranches_[static_cast<std::size_t>(ranch)];
    const int paid = count * cattle_price;
    r.earned += paid;
    say(std::to_string(count) + " head shipped from Porto Luma - " + std::string(r.spec->name) + " paid " + std::to_string(paid) + " kr");
    return paid;
}

Vector3 Ranches::stockyard_spot(int pen) {
    const auto& a = rr::stockyard_pens[pen % 2];
    return on_ground(stockyard_.frame.at(a.u0 + 3 + random() * (a.u1 - a.u0 - 6), a.v0 + 3 + random() * (a.v1 - a.v0 - 6)));
}

void Ranches::update_herd(RanchState& r, int index, float dt, const world::VerdaRegion& region, bool near) {
    (void)region;
    // Out of sight the herd keeps still; only the truck's errands move on (at once).
    for (std::size_t i = 0; i < r.herd.size(); ++i) {
        auto& cow = r.herd[i];
        if (cow.state == CowState::Dead) {
            cow.dead_for += dt;
            continue;
        }
        if (!near) {
            if ((cow.state == CowState::ToCorral || cow.state == CowState::Loading) && !cow.path.empty()) {
                cow.position = cow.path.back();
                cow.path.clear();
                cow.state = cow.state == CowState::ToCorral ? CowState::InCorral : CowState::Gone;
            }
            continue;
        }
        if (cow.pause > 0 && cow.state != CowState::Grazing) { cow.pause -= dt; continue; }
        Vector3 goal = cow.target;
        float speed = .9F;
        switch (cow.state) {
            case CowState::Grazing:
                cow.pause -= dt;
                if (cow.pause <= 0) { cow.target = pasture_point(r); cow.state = CowState::Walking; }
                continue;
            case CowState::InCorral:
            case CowState::Gone:
                continue;
            case CowState::Walking:
                break;
            case CowState::ToCorral:
            case CowState::Loading:
                if (cow.path.empty()) { cow.state = cow.state == CowState::ToCorral ? CowState::InCorral : CowState::Gone; continue; }
                goal = cow.path.front();
                speed = cow.state == CowState::Loading ? 1.1F : 1.3F;
                break;
            case CowState::Dead:
                break;
        }
        const Vector3 to{goal.x - cow.position.x, 0, goal.z - cow.position.z};
        const float distance = Vector3Length(to);
        if (distance < .4F) {
            if (cow.state == CowState::Walking) { cow.state = CowState::Grazing; cow.pause = 5 + random() * 10; }
            else if (!cow.path.empty()) cow.path.erase(cow.path.begin());
            continue;
        }
        // Turn toward the goal, then walk; keep a little apart from the others.
        const float want = std::atan2(to.x, to.z) * RAD2DEG;
        const float turn = std::remainder(want - cow.yaw, 360.0F);
        cow.yaw += std::clamp(turn, -110 * dt, 110 * dt);
        if (std::abs(turn) > 60) continue;
        Vector3 step = Vector3Scale(Vector3Normalize(to), std::min(distance, speed * dt));
        if (cow.state == CowState::Walking)
            for (std::size_t j = 0; j < r.herd.size(); ++j) {
                if (j == i || r.herd[j].state == CowState::Gone) continue;
                const float d = flat(r.herd[j].position, cow.position);
                if (d < 2.2F && d > .01F) {
                    step.x += (cow.position.x - r.herd[j].position.x) / d * .4F * dt;
                    step.z += (cow.position.z - r.herd[j].position.z) / d * .4F * dt;
                }
            }
        cow.position.x += step.x;
        cow.position.z += step.z;
        cow.stride += Vector3Length(step);
        // On the chute the cow climbs with the ramp; elsewhere it walks on the ground.
        const Vector2 local = rr::load_frame(r.frame).local(cow.position);
        float y = world::terrain::TerrainHeight::sample(cow.position.x, cow.position.z);
        if (cow.state == CowState::Loading && std::abs(local.x - rr::chute.x) < 1.2F && local.y < chute_foot_v)
            y += chute_height * std::clamp((chute_foot_v - local.y) / (chute_foot_v - chute_top_v), 0.0F, 1.0F);
        cow.position.y = y;
    }
    // The dead and the shipped leave the herd.
    std::erase_if(r.herd, [](const Cow& c) { return c.state == CowState::Gone || (c.state == CowState::Dead && c.dead_for > 180); });
    (void)index;
}

// ------------------------------------------------------------------------------------ family
void Ranches::update_family(RanchState& r, const RanchFrame& frame, ai::BattleRoyaleBots& bots, bool near) {
    const float h = frame.clock.hour();
    const bool night = h < 6 || h >= 20;
    const bool lunch = h >= 12 && h < 13;
    const int ranch = static_cast<int>(&r - ranches_.data());
    const bool hostile = angry(ranch, frame.now);
    for (const int k : r.family) {
        const auto& v = vakeros_[static_cast<std::size_t>(k)];
        auto* bot = bots.find(v.bot);
        if (!bot || bots.mounted(v.bot)) continue;
        if (!bot->alive()) {
            // Back from the hospital the next day, out of the player's sight.
            if (!near && h >= 7 && h < 8) {
                bots.respawn(v.bot, on_ground(r.frame.at(rr::house)), r.frame.yaw(), mix(static_cast<std::uint64_t>(v.bot) + static_cast<std::uint64_t>(frame.clock.day())));
                bots.set_asleep(v.bot, true);
            }
            continue;
        }
        Vector3 centre;
        float radius = 3;
        if (night) { centre = r.frame.at(rr::house); radius = 2.5F; }
        else if (lunch) { centre = r.frame.at(rr::house.x + 9, rr::house.y); radius = 4; }
        else switch (v.role) {
            case Role::Herd: centre = r.frame.at((rr::pasture.u0 + rr::pasture.u1) * .5F, (rr::pasture.v0 + rr::pasture.v1) * .5F); radius = 22; break;
            case Role::Fields: centre = r.frame.at(48, 102); radius = 16; break;
            case Role::Driver: centre = r.frame.at(rr::yard.x + 6, rr::yard.y + 2); radius = 5; break;
            case Role::Barn: centre = r.frame.at(-16, 30); radius = 14; break;
        }
        // While the truck is coming the herder works the corral.
        if (!night && r.calling && v.role == Role::Herd) { centre = r.frame.at((rr::corral.u0 + rr::corral.u1) * .5F, rr::corral.v0 - 2); radius = 6; }
        centre = on_ground(centre);
        if (hostile) {
            bots.set_asleep(v.bot, false);
            bot->set_roam_center(bot->position(), 10);
            continue;
        }
        if (!near) {
            // Asleep out of sight, wherever the day has them.
            if (flat(bot->position(), centre) > radius + 10) { bot->set_position(centre); bot->set_roam_center(centre, radius); }
            bots.set_asleep(v.bot, true);
            continue;
        }
        bots.set_asleep(v.bot, false);
        bot->set_roam_center(centre, radius);
    }
}

void Ranches::update_sxs(RanchState& r, const RanchFrame& frame, ai::BattleRoyaleBots& bots, vehicles::VehicleSystem& cars,
                         world::VerdaRegion& region, bool near) {
    if (r.sxs.empty() || r.loop_length <= 0) return;
    const int ranch = static_cast<int>(&r - ranches_.data());
    int driver = -1;
    for (const int k : r.family) if (vakeros_[static_cast<std::size_t>(k)].role == Role::Driver) driver = vakeros_[static_cast<std::size_t>(k)].bot;
    auto* body = bots.find(driver);
    const auto* asset = cars.asset(region, r.sxs);
    const float h = frame.clock.hour();
    const bool taken = frame.player_vehicle == r.sxs;
    const bool shift = h >= 8.5F && h < 17 && !angry(ranch, frame.now);
    const bool can = near && shift && body && body->alive() && asset && !asset->vehicle.destroyed && !taken;
    // Someone drives off in it on the ranch: the family won't have it.
    if (taken && frame.player_alive && flat(frame.player_feet, r.frame.at(rr::yard)) < 160)
        provoke_ranch(ranch, frame.now, "The " + std::string(r.spec->family) + "s see you take their side-by-side!");
    const auto stop = [&] {
        if (!r.sxs_running) return;
        r.sxs_running = false;
        cars.release(r.sxs);
        if (body) {
            bots.set_mounted(driver, false);
            if (const auto* a = cars.asset(region, r.sxs)) {
                const float yaw = a->rotation_y * DEG2RAD;
                body->set_position(on_ground({a->position.x + std::cos(yaw) * 1.6F, 0, a->position.z - std::sin(yaw) * 1.6F}));
            }
        }
    };
    if (!can) {
        stop();
        return;
    }
    if (!r.sxs_running) {
        // Only from the yard: in and away.
        if (!asset || flat(asset->position, r.loop.front()) > 6) return;
        r.sxs_running = true;
        r.sxs_progress = 0;
        r.sxs_speed = 0;
        r.sxs_wait = 2;
        bots.set_mounted(driver, true);
    }
    // Along the loop, slow at the corners, a rest in the yard after each lap; wait for the player.
    world::roads::Route route;
    route.points = r.loop;
    route.lane.assign(r.loop.size() - 1, 0);
    route.length = r.loop_length;
    route.complete = true;
    float limit = sxs_speed;
    float yaw_near = 0, yaw_far = 0;
    world::roads::route_point(route, std::fmod(r.sxs_progress + 2, r.loop_length), &yaw_near, false);
    world::roads::route_point(route, std::fmod(r.sxs_progress + 9, r.loop_length), &yaw_far, false);
    if (std::abs(std::remainder(yaw_far - yaw_near, 360.0F)) > 30) limit = 2.5F;
    const float left = r.loop_length - r.sxs_progress;
    limit = std::min(limit, std::sqrt(2 * 3.0F * std::max(0.0F, left - .3F)));
    if (asset) {
        const float a = asset->rotation_y * DEG2RAD;
        const Vector3 forward{std::sin(a), 0, std::cos(a)};
        const float ahead = (frame.player_feet.x - asset->position.x) * forward.x + (frame.player_feet.z - asset->position.z) * forward.z;
        const float side = std::abs((frame.player_feet.x - asset->position.x) * forward.z - (frame.player_feet.z - asset->position.z) * forward.x);
        if (ahead > 0 && ahead < 7 && side < 1.6F) limit = 0;
    }
    if (r.sxs_wait > 0) { r.sxs_wait -= frame.dt; limit = 0; }
    r.sxs_speed = r.sxs_speed < limit ? std::min(limit, r.sxs_speed + 2.5F * frame.dt) : std::max(limit, r.sxs_speed - 5.0F * frame.dt);
    r.sxs_progress += r.sxs_speed * frame.dt;
    if (r.sxs_progress >= r.loop_length - .05F) { r.sxs_progress = 0; r.sxs_speed = 0; r.sxs_wait = 40; }
    float front_yaw = 0;
    const Vector3 front = world::roads::route_point(route, std::min(r.sxs_progress + 1.0F, r.loop_length), &front_yaw, false);
    const Vector3 rear = world::roads::route_point(route, std::max(r.sxs_progress - .95F, 0.0F), nullptr, false);
    Vector3 centre = Vector3Lerp(rear, front, .487F);
    centre.y = ground_at(centre, region);
    float yaw = std::atan2(front.x - rear.x, front.z - rear.z) * RAD2DEG;
    if (r.sxs_progress < .5F && asset) yaw = asset->rotation_y;
    const float steering = std::remainder(front_yaw - yaw, 360.0F);
    if (!cars.glide(region, r.sxs, centre, yaw, r.sxs_speed, steering, frame.dt)) stop();
    if (body) body->set_position(centre);
}

void Ranches::update(const RanchFrame& frame, ai::BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region) {
    if (ranches_.empty() || !std::isfinite(frame.dt) || frame.dt <= 0) return;
    const float dt = std::min(frame.dt, .25F);
    message_time_ = std::max(0.0F, message_time_ - dt);
    if (!frame.player_alive) forgive();
    const Vector3 direction = Vector3Length(frame.aim_direction) > 1e-4F ? Vector3Normalize(frame.aim_direction) : Vector3{0, 0, 1};
    const auto on_line = [&](Vector3 p, float radius) {
        const Vector3 to = Vector3Subtract(p, frame.aim_origin);
        const float along = Vector3DotProduct(to, direction);
        return along > 0 && along < 160 && Vector3Length(Vector3Subtract(to, Vector3Scale(direction, along))) < radius;
    };
    for (std::size_t i = 0; i < ranches_.size(); ++i) {
        auto& r = ranches_[i];
        const int ranch = static_cast<int>(i);
        const Vector3 yard = r.frame.at(rr::yard);
        const bool near = flat(yard, frame.player_feet) < wake_radius;
        // Shots: at the family, or into the herd.
        if (frame.player_fired && frame.player_alive && flat(yard, frame.aim_origin) < 260) {
            for (const int k : r.family) {
                const auto* bot = bots.find(vakeros_[static_cast<std::size_t>(k)].bot);
                if (bot && bot->alive() && on_line(Vector3Add(bot->position(), {0, 1.2F, 0}), 1.8F))
                    provoke_ranch(ranch, frame.now, "You shot at " + vakeros_[static_cast<std::size_t>(k)].name + " - the " + std::string(r.spec->family) + "s shoot back");
            }
            for (auto& cow : r.herd) {
                if (cow.state == CowState::Dead || cow.state == CowState::Gone) continue;
                if (!on_line(Vector3Add(cow.position, {0, 1.05F, 0}), .6F)) continue;
                cow.state = CowState::Dead;
                cow.path.clear();
                provoke_ranch(ranch, frame.now, "You shot one of the " + std::string(r.spec->family) + "s' cattle - they are coming for you");
                break;
            }
        }
        // Calves: born each morning, up to the herd's size.
        if (frame.clock.day() != r.calved_day && frame.clock.hour() >= 6) {
            r.calved_day = frame.clock.day();
            for (int calves = 0; calves < 4 && alive_head(r) < herd_size; ++calves) {
                Cow cow;
                cow.position = cow.target = pasture_point(r);
                cow.angus = random() < .4F;
                cow.yaw = random() * 360;
                r.herd.push_back(cow);
            }
        }
        update_herd(r, ranch, dt, region, flat(yard, frame.player_feet) < cattle_radius);
        update_family(r, frame, bots, near);
        update_sxs(r, frame, bots, cars, region, near);
    }
    // The cattle quay: down the chute into the pen; after a while, single file across the quay and
    // up the gangway into the ship.
    if (quay_.found) {
        const auto& f = quay_.frame;
        const bool near = flat(f.gate, frame.player_feet) < cattle_radius;
        if (quay_.boarding > 0 && (quay_.boarding -= dt) <= 0) {
            const Vector3 foot{rr::cattle_gangway_foot.x, 0, rr::cattle_gangway_foot.y}, top{rr::cattle_gangway_top.x, 0, rr::cattle_gangway_top.y};
            int k = 0;
            for (auto& cow : quay_.pen) {
                if (cow.state != CowState::InCorral) continue;
                cow.state = CowState::Loading;
                const Vector3 base = on_ground(foot);
                cow.path = {on_ground(f.at(rr::cattle_quay_pen.u1 - 2, 12)), on_ground(f.at(rr::cattle_quay_pen.u1 + 4, 12)), base,
                            {top.x, base.y + 2.4F, top.z}, {top.x + 5, base.y + 2.4F, top.z}};
                cow.pause = static_cast<float>(k++) * 2.5F;
            }
        }
        for (auto& cow : quay_.pen) {
            if (cow.state != CowState::ToCorral && cow.state != CowState::Loading) continue;
            const CowState done = cow.state == CowState::ToCorral ? CowState::InCorral : CowState::Gone;
            if (!near || cow.path.empty()) {
                if (!cow.path.empty()) cow.position = cow.path.back();
                cow.path.clear();
                cow.state = done;
                continue;
            }
            if (cow.pause > 0) { cow.pause -= dt; continue; }
            const Vector3 goal = cow.path.front();
            const Vector3 to{goal.x - cow.position.x, goal.y - cow.position.y, goal.z - cow.position.z};
            const float distance = std::hypot(to.x, to.z);
            if (distance < .4F) { cow.path.erase(cow.path.begin()); continue; }
            cow.yaw = std::atan2(to.x, to.z) * RAD2DEG;
            const float step = std::min(distance, 1.2F * dt);
            cow.position.x += to.x / distance * step;
            cow.position.z += to.z / distance * step;
            cow.position.y += to.y * step / distance;   // down the chute, up the gangway
            cow.stride += step;
        }
        std::erase_if(quay_.pen, [](const Cow& c) { return c.state == CowState::Gone; });
    }
    // The stockyard pen: newcomers walk down the chute into the pen and stand about.
    if (stockyard_.found) {
        const bool near = flat(stockyard_.frame.at(0, 24), frame.player_feet) < cattle_radius;
        for (auto& cow : stockyard_.pen) {
            if (cow.state != CowState::ToCorral) continue;
            if (!near || cow.path.empty()) {
                if (!cow.path.empty()) cow.position = cow.path.back();
                cow.path.clear();
                cow.state = CowState::InCorral;
                continue;
            }
            if (cow.pause > 0) { cow.pause -= dt; continue; }
            const Vector3 goal = cow.path.front();
            const Vector3 to{goal.x - cow.position.x, 0, goal.z - cow.position.z};
            const float distance = Vector3Length(to);
            if (distance < .4F) { cow.path.erase(cow.path.begin()); continue; }
            cow.yaw = std::atan2(to.x, to.z) * RAD2DEG;
            const Vector3 step = Vector3Scale(Vector3Normalize(to), std::min(distance, 1.2F * dt));
            cow.position.x += step.x;
            cow.position.z += step.z;
            cow.stride += Vector3Length(step);
            cow.position.y = std::max(world::terrain::TerrainHeight::sample(cow.position.x, cow.position.z), cow.position.y - 1.5F * dt);
        }
    }
}

} // namespace outland::game::ranch
