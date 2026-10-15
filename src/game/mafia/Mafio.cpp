#include "outland/game/mafia/Mafio.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::mafia {

namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
std::uint64_t hash(std::string_view text, std::uint64_t seed) {
    std::uint64_t h = 1469598103934665603ULL ^ seed;
    for (const char c : text) { h ^= static_cast<unsigned char>(c); h *= 1099511628211ULL; }
    return h;
}
// How well a place suits a crew: lower is better; negative never.
float suitability(const life::Place& place) {
    if (!place.indoor || place.sealed) return -1;
    float score = 0;
    switch (place.kind) {
        case life::PlaceKind::Pub: score = 0; break;
        case life::PlaceKind::Industrial: score = 1; break;
        case life::PlaceKind::Garage: score = 2; break;
        case life::PlaceKind::Shop: score = 3; break;
        case life::PlaceKind::Office: score = 4; break;
        case life::PlaceKind::Dock: score = 5; break;
        case life::PlaceKind::Home: score = 8; break;
        default: return -1; // not the church, the police station, the clinic, farms or squares
    }
    return score + (place.mixed_home ? .5F : 0);
}
Vector3 ground(Vector3 p, const world::VerdaRegion& region) {
    const float terrain = world::terrain::TerrainHeight::sample(p.x, p.z);
    const float probe = std::max(terrain, p.y) + 1.2F;
    return {p.x, world::physics::WorldCollision::ground_height({p.x, probe, p.z}, probe, region), p.z};
}
}

int gang_of_zone(world::zones::ZoneId zone) {
    for (std::size_t i = 0; i < gangs.size(); ++i) if (gangs[i].zone == zone) return static_cast<int>(i);
    return 0;
}

float Mafio::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 7; rng_ ^= rng_ << 17;
    return static_cast<float>(rng_ >> 40) / static_cast<float>(1ULL << 24);
}

int Mafio::start(const std::vector<life::Place>& places, const world::VerdaRegion& region, ai::BattleRoyaleBots& bots, std::uint64_t seed) {
    crews_.clear();
    members_.clear();
    wars_.clear();
    provoked_until_.fill(-1e9);
    next_war_ = 0;
    seed_ = seed;
    rng_ = (seed ^ 0x51ed2701f3a5c3b9ULL) | 1;
    message_.clear();
    message_time_ = 0;
    const auto& towns = region.settlements();
    for (std::size_t s = 0; s < towns.size(); ++s) {
        const auto& town = towns[s];
        const life::Place* best = nullptr;
        float best_score = 1e9F;
        const bool capital = town.id == "capital_verda";
        for (const auto& place : places) {
            if (place.settlement != static_cast<int>(s)) continue;
            float score = suitability(place);
            if (score < 0) continue;
            if (capital && place.kind != life::PlaceKind::Pub) score += 2; // the casino wants a bar
            score += static_cast<float>(hash(place.id, seed) % 1000) / 4000.0F;
            if (score < best_score) { best_score = score; best = &place; }
        }
        if (!best) continue;
        Crew crew;
        crew.gang = gang_of_zone(world::zones::zone_at(town.center));
        crew.settlement = static_cast<int>(s);
        crew.town = town.name.empty() ? town.id : town.name;
        crew.building = best->building_id;
        crew.kind = best->kind;
        crew.door = ground(best->door, region);
        crew.interior = ground(best->interior, region);
        crew.yaw = best->building_yaw;
        crew.casino = capital;
        crew.table = crew.interior;
        const auto& gang = gangs[static_cast<std::size_t>(crew.gang)];
        int size = gang.crew;
        if (crew.gang == 0 && !capital) size = 4;          // the Green Hand's outposts are smaller
        if (town.id.starts_with("hamlet_")) size = 3;
        // Facing out of the door; posts beside it, inside and (Verda City) round the table.
        Vector3 out = Vector3Subtract(crew.door, crew.interior);
        out.y = 0;
        out = Vector3Length(out) > .1F ? Vector3Normalize(out) : Vector3{0, 0, 1};
        const Vector3 side{-out.z, 0, out.x};
        const float facing = std::atan2(out.x, out.z) * RAD2DEG;
        const int crew_index = static_cast<int>(crews_.size());
        for (int k = 0; k < size; ++k) {
            Member m;
            m.crew = crew_index;
            if (k < 2) { // the door
                m.post = Vector3Add(Vector3Add(crew.door, Vector3Scale(out, 1.4F)), Vector3Scale(side, k == 0 ? 1.8F : -1.8F));
                m.post_radius = 3;
                m.yaw = facing;
            } else if (crew.casino && k < 4) { // at the poker table
                m.post = Vector3Add(crew.table, Vector3Scale(side, k == 2 ? 1.3F : -1.3F));
                m.post_radius = .6F;
                m.yaw = facing + (k == 2 ? -90.0F : 90.0F);
            } else {
                const float spread = static_cast<float>((k % 3) - 1) * 1.8F;
                m.post = Vector3Add(Vector3Add(crew.interior, Vector3Scale(side, spread)), Vector3Scale(out, crew.casino ? -2.2F : .8F));
                m.post_radius = 2.5F;
                m.yaw = facing + 180;
            }
            m.post = ground(m.post, region);
            auto& bot = bots.add_bot(m.post, gang.arms[static_cast<std::size_t>(k) % gang.arms.size()], gang.level,
                                     seed + static_cast<std::uint64_t>(members_.size()) * 7919ULL);
            bot.set_roam_center(m.post, m.post_radius);
            bot.set_yaw(m.yaw);
            m.bot = bot.id();
            crew.members.push_back(m.bot);
            members_.push_back(m);
        }
        crews_.push_back(std::move(crew));
    }
    return static_cast<int>(crews_.size());
}

void Mafio::clear(ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers, vehicles::VehicleSystem& cars) {
    if (!crews_.empty()) {
        drivers.clear(bots, cars);
        bots.clear();
    }
    crews_.clear();
    members_.clear();
    wars_.clear();
}

Member* Mafio::member(int bot) {
    if (bot >= 1 && static_cast<std::size_t>(bot) <= members_.size() && members_[static_cast<std::size_t>(bot - 1)].bot == bot)
        return &members_[static_cast<std::size_t>(bot - 1)];
    for (auto& m : members_) if (m.bot == bot) return &m;
    return nullptr;
}

int Mafio::crew_of(int bot) const {
    if (bot >= 1 && static_cast<std::size_t>(bot) <= members_.size() && members_[static_cast<std::size_t>(bot - 1)].bot == bot)
        return members_[static_cast<std::size_t>(bot - 1)].crew;
    for (const auto& m : members_) if (m.bot == bot) return m.crew;
    return -1;
}

int Mafio::gang_of(int bot) const {
    const int crew = crew_of(bot);
    return crew < 0 ? -1 : crews_[static_cast<std::size_t>(crew)].gang;
}

const Crew* Mafio::casino() const {
    for (const auto& crew : crews_) if (crew.casino) return &crew;
    return nullptr;
}

int Mafio::turf_at(Vector3 feet, float radius) const {
    for (std::size_t i = 0; i < crews_.size(); ++i)
        if (flat(feet, crews_[i].door) < radius || flat(feet, crews_[i].interior) < radius) return static_cast<int>(i);
    return -1;
}

bool Mafio::provoked(int gang, double now) const {
    return gang >= 0 && static_cast<std::size_t>(gang) < provoked_until_.size() && provoked_until_[static_cast<std::size_t>(gang)] > now;
}

void Mafio::provoke(int bot, double now) {
    const int gang = gang_of(bot);
    if (gang < 0) return;
    if (!provoked(gang, now)) say(std::string(gangs[static_cast<std::size_t>(gang)].name) + " shoot back - the police are paid off");
    provoked_until_[static_cast<std::size_t>(gang)] = now + provoked_seconds;
}

void Mafio::forgive() { provoked_until_.fill(-1e9); }

bool Mafio::hostile(int a, int b, double now) const {
    if (a == b) return false;
    const auto outsider = [&](int id) { return id != ai::BattleRoyaleBots::player_id && gang_of(id) < 0; };
    if (outsider(a) || outsider(b)) return outsiders_ && outsiders_(a, b, now);
    if (a == ai::BattleRoyaleBots::player_id || b == ai::BattleRoyaleBots::player_id)
        return provoked(gang_of(a == ai::BattleRoyaleBots::player_id ? b : a), now);
    const int ga = gang_of(a), gb = gang_of(b);
    if (ga < 0 || gb < 0 || ga == gb) return false;
    for (const auto& war : wars_)
        if (war.until > now && ((war.attackers == ga && war.defenders == gb) || (war.attackers == gb && war.defenders == ga))) return true;
    return false;
}

bool Mafio::send_hit_squad(int from, int target, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                           vehicles::VehicleSystem& cars, world::VerdaRegion& region, double now) {
    if (from < 0 || target < 0 || static_cast<std::size_t>(from) >= crews_.size() || static_cast<std::size_t>(target) >= crews_.size()) return false;
    const auto& home = crews_[static_cast<std::size_t>(from)];
    const auto& victim = crews_[static_cast<std::size_t>(target)];
    if (home.gang == victim.gang) return false;
    std::vector<int> squad;
    const std::size_t want = home.gang == 0 ? 3 : 2;
    for (const int id : home.members) {
        const auto* bot = bots.find(id);
        const bool busy = std::any_of(wars_.begin(), wars_.end(), [&](const War& w) {
            return std::find(w.squad.begin(), w.squad.end(), id) != w.squad.end(); });
        if (bot && bot->alive() && !bots.mounted(id) && !busy) squad.push_back(id);
        if (squad.size() >= want) break;
    }
    if (squad.size() < 2) return false;
    if (!drivers.dispatch(bots, cars, region, squad, home.door, victim.door, 200)) return false;
    for (const int id : squad) bots.set_asleep(id, false);
    War war;
    war.attackers = home.gang;
    war.defenders = victim.gang;
    war.target = target;
    war.from = from;
    war.squad = squad;
    war.until = now + 240;
    wars_.push_back(std::move(war));
    say(std::string(gangs[static_cast<std::size_t>(home.gang)].name) + " are driving on " +
        std::string(gangs[static_cast<std::size_t>(victim.gang)].name) + " in " + victim.town);
    return true;
}

void Mafio::plan_war(const MafioFrame& frame, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                     vehicles::VehicleSystem& cars, world::VerdaRegion& region) {
    // A crew near the player (so the fight can be seen) is hit by the nearest rival crew.
    std::vector<int> targets;
    for (std::size_t i = 0; i < crews_.size(); ++i) {
        if (flat(crews_[i].door, frame.player_feet) > 450) continue;
        const bool attacked = std::any_of(wars_.begin(), wars_.end(), [&](const War& w) { return w.target == static_cast<int>(i); });
        if (!attacked) targets.push_back(static_cast<int>(i));
    }
    if (targets.empty()) return;
    const int target = targets[static_cast<std::size_t>(random() * static_cast<float>(targets.size())) % targets.size()];
    const auto& victim = crews_[static_cast<std::size_t>(target)];
    std::vector<int> rivals;
    for (std::size_t i = 0; i < crews_.size(); ++i) if (crews_[i].gang != victim.gang) rivals.push_back(static_cast<int>(i));
    std::sort(rivals.begin(), rivals.end(), [&](int a, int b) {
        return flat(crews_[static_cast<std::size_t>(a)].door, victim.door) < flat(crews_[static_cast<std::size_t>(b)].door, victim.door); });
    if (rivals.size() > 4) rivals.resize(4);
    for (const int from : rivals)
        if (send_hit_squad(from, target, bots, drivers, cars, region, frame.now)) return;
}

void Mafio::send_home(Member& m, ai::BattleRoyaleBots& bots, const MafioFrame& frame) {
    auto* bot = bots.find(m.bot);
    if (!bot || !bot->alive()) return;
    // Out of sight, they are simply back home; otherwise they walk.
    if (flat(bot->position(), frame.player_feet) > 150 && flat(m.post, frame.player_feet) > 150) {
        bot->set_position(m.post);
        bot->set_yaw(m.yaw);
    }
    bot->set_roam_center(m.post, m.post_radius);
}

void Mafio::update(const MafioFrame& frame, ai::BattleRoyaleBots& bots, ai::BotWorld world, ai::BotDrivers& drivers,
                   vehicles::VehicleSystem& cars, world::VerdaRegion& region) {
    if (crews_.empty() || !std::isfinite(frame.dt) || frame.dt <= 0) return;
    const double now = frame.now;
    message_time_ = std::max(0.0F, message_time_ - frame.dt);
    if (next_war_ <= 0) next_war_ = now + 100 + random() * 80;

    // Shooting at a Krimulo (the round passing within a body's width) is as good as hitting one.
    if (frame.player_fired && frame.player_alive) {
        const Vector3 direction = Vector3Length(frame.aim_direction) > 1e-4F ? Vector3Normalize(frame.aim_direction) : Vector3{0, 0, 1};
        for (const auto& m : members_) {
            const auto* bot = bots.find(m.bot);
            if (!bot || !bot->alive() || bots.mounted(m.bot)) continue;
            const Vector3 chest{bot->position().x, bot->position().y + 1.2F, bot->position().z};
            const Vector3 to = Vector3Subtract(chest, frame.aim_origin);
            const float along = Vector3DotProduct(to, direction);
            if (along <= 0 || along > 150) continue;
            if (Vector3Length(Vector3Subtract(to, Vector3Scale(direction, along))) < 1.8F) provoke(m.bot, now);
        }
    }

    // The dead come back to the hideout, out of the player's sight if possible.
    for (std::size_t i = 0; i < members_.size(); ++i) {
        auto& m = members_[i];
        auto* bot = bots.find(m.bot);
        if (!bot) continue;
        if (!bot->alive() && !m.dead) { m.dead = true; m.respawn_at = now + respawn_seconds; }
        if (m.dead && now >= m.respawn_at && (flat(m.post, frame.player_feet) > 45 || now > m.respawn_at + 90)) {
            bots.respawn(m.bot, m.post, m.yaw, seed_ + static_cast<std::uint64_t>(now * 10) + i);
            if (auto* back = bots.find(m.bot)) back->set_roam_center(m.post, m.post_radius);
            m.dead = false;
        }
    }

    // Wars: the squad gets there, fights, and goes home when it is over.
    for (auto& war : wars_) {
        bool riding = false, standing = false;
        for (const int id : war.squad) {
            if (drivers.ride_of(id)) riding = true;
            const auto* bot = bots.find(id);
            const auto* m = member(id);
            if (bot && bot->alive() && m && !m->dead) standing = true;
        }
        if (!war.arrived && !riding) {
            war.arrived = true;
            const auto& victim = crews_[static_cast<std::size_t>(war.target)];
            if (flat(victim.door, frame.player_feet) < 350)
                say("Drive-by! " + std::string(gangs[static_cast<std::size_t>(war.attackers)].name) + " hit " +
                    std::string(gangs[static_cast<std::size_t>(war.defenders)].name) + " in " + victim.town);
        }
        if (!standing && war.arrived) war.until = std::min(war.until, now);
    }
    for (auto& war : wars_) {
        if (war.until > now) continue;
        for (const int id : war.squad) if (auto* m = member(id)) send_home(*m, bots, frame);
    }
    std::erase_if(wars_, [&](const War& w) { return w.until <= now; });
    if (now >= next_war_) {
        next_war_ = now + 150 + random() * 150;
        if (wars_.size() < 2) plan_war(frame, bots, drivers, cars, region);
    }

    // Crews far from the player sleep (in place, at their posts) unless they are at war.
    for (auto& m : members_) {
        auto* bot = bots.find(m.bot);
        if (!bot) continue;
        bool awake = flat(m.post, frame.player_feet) < wake_radius || flat(bot->position(), frame.player_feet) < wake_radius || bots.mounted(m.bot);
        for (const auto& war : wars_)
            if (war.target == m.crew || std::find(war.squad.begin(), war.squad.end(), m.bot) != war.squad.end()) awake = true;
        if (!awake && !bots.asleep(m.bot) && bot->alive() && flat(bot->position(), m.post) > 30) {
            bot->set_position(m.post);
            bot->set_yaw(m.yaw);
            bot->set_roam_center(m.post, m.post_radius);
        }
        bots.set_asleep(m.bot, !awake);
    }

    world.environment.hostile = [this, now](int a, int b) { return hostile(a, b, now); };
    ai::BattleRoyaleFrame bf;
    bf.dt = frame.dt;
    bf.now = now;
    bf.player_feet = frame.player_feet;
    bf.player_velocity = frame.player_velocity;
    bf.player_alive = frame.player_alive;
    bf.player_fired = frame.player_fired;
    bf.player_weapon = frame.player_weapon;
    bf.player_in_vehicle = frame.player_in_vehicle;
    drivers.update(frame.dt, bots, cars, region, frame.player_feet, frame.player_alive, 0, world.environment.line_of_sight);
    bots.update(bf, world);
}

} // namespace outland::game::mafia
