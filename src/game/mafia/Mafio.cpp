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
        case life::PlaceKind::Dock: score = 5; break;
        case life::PlaceKind::Home: score = 8; break;
        default: return -1; // not the church, the police station, the clinic, the town hall, farms or squares
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

int Mafio::start(const std::vector<life::Place>& places, const world::VerdaRegion& region, ai::BattleRoyaleBots& bots, std::uint64_t seed,
                 const std::vector<int>& lived_in, const std::vector<std::string>& not_for_gangs) {
    crews_.clear();
    members_.clear();
    wars_.clear();
    patrols_.clear();
    heists_.clear();
    provoked_until_.fill(-1e9);
    next_war_ = 0;
    seed_ = seed;
    rng_ = (seed ^ 0x51ed2701f3a5c3b9ULL) | 1;
    message_.clear();
    message_time_ = 0;
    const auto& towns = region.settlements();
    for (std::size_t s = 0; s < towns.size(); ++s) {
        const auto& town = towns[s];
        if (town.id.starts_with("ranch_")) continue; // the Vakeroj's ranches answer to nobody
        const life::Place* best = nullptr;
        float best_score = 1e9F;
        const bool capital = town.id == "capital_verda";
        const bool green_hand = gang_of_zone(world::zones::zone_at(town.center)) == 0;   // they stay put
        // Everyone but the Green Hand: a plain house nobody lives in (any building, if the town has none).
        // (One nobody lives in if there is one; else a family has to put up with them.)
        bool empty_only = true;
        const auto plain_house = [&](std::size_t i) {
            const auto& place = places[i];
            if (place.building_id.starts_with("gang_house_")) return true;   // built for them (dev::gang_houses)
            if (place.kind != life::PlaceKind::Home || place.mixed_home || !place.indoor || place.sealed) return false;
            if (empty_only && i < lived_in.size() && lived_in[i] > 0) return false;
            return std::find(not_for_gangs.begin(), not_for_gangs.end(), place.building_id) == not_for_gangs.end();
        };
        bool houses = false;
        if (!green_hand)
            for (const bool strict : {true, false}) {
                empty_only = strict;
                for (std::size_t i = 0; i < places.size() && !houses; ++i)
                    houses = places[i].settlement == static_cast<int>(s) && plain_house(i);
                if (houses) break;
            }
        for (std::size_t i = 0; i < places.size(); ++i) {
            const auto& place = places[i];
            if (place.settlement != static_cast<int>(s)) continue;
            if (houses && !plain_house(i)) continue;
            float score = suitability(place);
            if (score < 0) continue;
            // Not on the police's doorstep: well away from any station (the Green Hand keep the
            // places they have always had).
            for (const auto& station : places)
                if (!green_hand && station.kind == life::PlaceKind::Police)
                    score += std::max(0.0F, station_clearance - flat(place.door, station.door)) * .25F;
            if (capital && place.kind != life::PlaceKind::Pub) score += 2; // the casino wants a bar
            if (houses && place.building_id.find("main_street") != std::string::npos) score += 45;   // a house of its own, not a row
            if (place.building_id.starts_with("gang_house_")) score -= 1000;                            // their own house
            // Fixed: the same building every game - nearest the middle of town, a hash for ties.
            score += flat(place.door, town.center) / 500.0F + static_cast<float>(hash(place.id, 0x7e55a) % 1000) / 40000.0F;
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
        crew.base = world::zones::zone(world::zones::zone_at(town.center)).seat == town.id;
        crew.house = houses;
        if (crew.house) {
            // The tag: on the front wall (the door stands 1.6 m out from it), beside the door.
            Vector3 n = Vector3Subtract(crew.door, crew.interior);
            n.y = 0;
            n = Vector3Length(n) > .1F ? Vector3Normalize(n) : Vector3{0, 0, 1};
            const Vector3 along{-n.z, 0, n.x};
            crew.tag = Vector3Add(Vector3Subtract(crew.door, Vector3Scale(n, 1.6F)), Vector3Scale(along, 1.9F));
            crew.tag.y = crew.door.y + 1.7F;
            crew.tag_normal = n;
        }
        const auto& gang = gangs[static_cast<std::size_t>(crew.gang)];
        int size = gang.crew;
        if (crew.gang == 0 && !capital) size = 4;          // the Green Hand's outposts are smaller
        if (crew.base) size = std::max(size, 6);           // every gang's main base is well manned
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
            m.officer = k == 0 || (crew.casino && k < 4);
            if (k < 2) { // the door
                m.post = Vector3Add(Vector3Add(crew.door, Vector3Scale(out, 1.4F)), Vector3Scale(side, k == 0 ? 1.8F : -1.8F));
                m.post_radius = k == 0 ? .8F : 3;   // the boss holds his spot by the door
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
        // The street patrol: the last soldiers of the crew (never the boss or the card table) walk
        // up and down the street in front of the hideout - its nearest road, from 60 m one way to
        // 60 m the other - out on the street from the start (a straight walk, nothing in the way,
        // past the walls a rival would tag).
        if (!crew.casino && size >= 3) {
            const world::Road* street = nullptr;
            float near = 60, along = 0;
            for (const auto& road : town.roads) {
                if (road.type == world::RoadType::Highway) continue;
                const Vector3 d{road.end.x - road.start.x, 0, road.end.z - road.start.z};
                const float length = std::hypot(d.x, d.z);
                if (length < 30) continue;
                const float t = std::clamp(((crew.door.x - road.start.x) * d.x + (crew.door.z - road.start.z) * d.z) / (length * length), 0.0F, 1.0F);
                const Vector3 foot{road.start.x + d.x * t, 0, road.start.z + d.z * t};
                if (const float gap = flat(foot, crew.door); gap < near) { near = gap; street = &road; along = t * length; }
            }
            std::vector<Vector3> route;
            if (street) {
                const Vector3 d{street->end.x - street->start.x, 0, street->end.z - street->start.z};
                const float length = std::hypot(d.x, d.z);
                for (const float offset : {-60.0F, -30.0F, 0.0F, 30.0F, 60.0F}) {
                    const float s = std::clamp(along + offset, 0.0F, length);
                    const Vector3 p = ground({street->start.x + d.x * s / length, 0, street->start.z + d.z * s / length}, region);
                    if (route.empty() || flat(p, route.back()) > 10) route.push_back(p);
                }
                // Up the street and back: there, then the way back (not a jump from the far end).
                for (int k = static_cast<int>(route.size()) - 2; k > 0; --k) route.push_back(route[static_cast<std::size_t>(k)]);
            }
            if (route.size() >= 2)
                for (int k = 0; k < patrols_per_crew && k < size - 2; ++k) {
                    Patrol patrol;
                    patrol.member = static_cast<int>(members_.size()) - 1 - k;
                    patrol.route = route;
                    patrol.leg = static_cast<int>((route.size() / 2) * static_cast<std::size_t>(k) % route.size());   // the two start apart
                    // On the street from the start: his post is where the street passes the door.
                    auto& m = members_[static_cast<std::size_t>(patrol.member)];
                    std::size_t nearest = 0;
                    for (std::size_t r = 1; r < route.size(); ++r) if (flat(route[r], crew.door) < flat(route[nearest], crew.door)) nearest = r;
                    m.post = route[nearest];
                    m.post_radius = 3;
                    if (auto* b = bots.find(m.bot)) { b->set_position(m.post); b->set_roam_center(m.post, m.post_radius); }
                    patrols_.push_back(std::move(patrol));
                }
        }
        crews_.push_back(std::move(crew));
    }
    return static_cast<int>(crews_.size());
}

void Mafio::clear(ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers, vehicles::VehicleSystem& cars) {
    patrols_.clear();
    if (!crews_.empty()) {
        drivers.clear(bots, cars);
        bots.clear();
    }
    crews_.clear();
    members_.clear();
    wars_.clear();
    heists_.clear();
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

void Mafio::provoke_gang(int gang, double now, const std::string& why) {
    if (gang < 0 || gang >= static_cast<int>(gangs.size())) return;
    if (!provoked(gang, now) && !why.empty()) say(why);
    provoked_until_[static_cast<std::size_t>(gang)] = now + provoked_seconds;
}

void Mafio::set_crew_gang(int crew, int gang) {
    if (crew < 0 || static_cast<std::size_t>(crew) >= crews_.size() || gang < 0 || gang >= static_cast<int>(gangs.size())) return;
    crews_[static_cast<std::size_t>(crew)].gang = gang;
    // Nobody stays at war on behalf of a crew that changed sides.
    std::erase_if(wars_, [&](const War& w) { return w.target == crew || w.from == crew; });
}

void Mafio::forgive() { provoked_until_.fill(-1e9); }

void Mafio::disband(int crew) {
    if (crew < 0 || static_cast<std::size_t>(crew) >= crews_.size()) return;
    crews_[static_cast<std::size_t>(crew)].disbanded = true;
    std::erase_if(wars_, [&](const War& w) { return w.target == crew || w.from == crew; });
}

void Mafio::besiege(int crew, ai::BattleRoyaleBots& bots, const MafioFrame& frame) {
    if (crew == besieged_) return;
    besieged_ = crew < 0 || static_cast<std::size_t>(crew) >= crews_.size() ? -1 : crew;
    if (besieged_ < 0) return;
    // Every man home to defend it: the squads out on a hit are called back.
    for (auto& war : wars_) {
        if (war.from != besieged_) continue;
        for (const int id : war.squad) if (auto* m = member(id)) send_home(*m, bots, frame);
        war.until = 0;
    }
    std::erase_if(wars_, [&](const War& w) { return w.from == besieged_ || w.target == besieged_; });
}

int Mafio::standing(int crew, ai::BattleRoyaleBots& bots) const {
    if (crew < 0 || static_cast<std::size_t>(crew) >= crews_.size()) return 0;
    int n = 0;
    for (const int id : crews_[static_cast<std::size_t>(crew)].members)
        if (const auto* bot = bots.find(id); bot && bot->alive()) ++n;
    return n;
}

bool Mafio::officer(int bot) const {
    for (const auto& m : members_) if (m.bot == bot) return m.officer;
    return false;
}

bool Mafio::hostile(int a, int b, double now) const {
    if (a == b) return false;
    const auto outsider = [&](int id) { return id != ai::BattleRoyaleBots::player_id && gang_of(id) < 0; };
    if (outsider(a) || outsider(b)) return outsiders_ && outsiders_(a, b, now);
    if (a == ai::BattleRoyaleBots::player_id || b == ai::BattleRoyaleBots::player_id) {
        const int other = a == ai::BattleRoyaleBots::player_id ? b : a;
        if (held(other)) return false;
        return provoked(gang_of(other), now);
    }
    if (held(a) || held(b)) return false;
    const int ga = gang_of(a), gb = gang_of(b);
    if (ga < 0 || gb < 0 || ga == gb) return false;
    // The player's own crew fights whoever has turned on the player.
    if ((lent(a) && provoked(gb, now)) || (lent(b) && provoked(ga, now))) return true;
    for (const auto& war : wars_)
        if (war.until > now && ((war.attackers == ga && war.defenders == gb) || (war.attackers == gb && war.defenders == ga))) return true;
    return false;
}

bool Mafio::send_hit_squad(int from, int target, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                           vehicles::VehicleSystem& cars, world::VerdaRegion& region, double now) {
    if (from < 0 || target < 0 || static_cast<std::size_t>(from) >= crews_.size() || static_cast<std::size_t>(target) >= crews_.size()) return false;
    const auto& home = crews_[static_cast<std::size_t>(from)];
    const auto& victim = crews_[static_cast<std::size_t>(target)];
    if (home.gang == victim.gang || home.disbanded || victim.disbanded || from == besieged_ || target == besieged_) return false;
    std::vector<int> squad;
    const std::size_t want = home.gang == 0 ? 3 : 2;
    for (const int id : home.members) {
        const auto* bot = bots.find(id);
        const bool busy = std::any_of(wars_.begin(), wars_.end(), [&](const War& w) {
            return std::find(w.squad.begin(), w.squad.end(), id) != w.squad.end(); });
        const auto* m = member(id);
        if (bot && bot->alive() && !bots.mounted(id) && !busy && !(m && (m->lent || m->held)) && !heist_of(id)) squad.push_back(id);
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
        if (flat(crews_[i].door, frame.player_feet) > 450 || crews_[i].disbanded) continue;
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

bool Mafio::recruitable(int bot, ai::BattleRoyaleBots& bots) const {
    const Member* m = nullptr;
    for (const auto& x : members_) if (x.bot == bot) m = &x;
    if (!m || m->lent || m->dead || player_gang_ < 0) return false;
    const auto& crew = crews_[static_cast<std::size_t>(m->crew)];
    // Not the boss, nor the men at the casino's poker table.
    const auto at = std::find(crew.members.begin(), crew.members.end(), bot) - crew.members.begin();
    if (at == 0 || (crew.casino && (at == 2 || at == 3))) return false;
    if (crew.gang != player_gang_ || crew.disbanded || bots.hidden(bot) || bots.mounted(bot)) return false;
    for (const auto& war : wars_) if (std::find(war.squad.begin(), war.squad.end(), bot) != war.squad.end()) return false;
    if (heist_of(bot) || m->held) return false;
    const auto* b = bots.find(bot);
    return b && b->alive();
}

void Mafio::lend(int bot, bool lent, ai::BattleRoyaleBots& bots, Vector3 player_feet) {
    for (auto& m : members_) {
        if (m.bot != bot) continue;
        m.lent = lent;
        if (!lent) {
            MafioFrame frame;
            frame.player_feet = player_feet;
            send_home(m, bots, frame);
        }
        return;
    }
}

bool Mafio::lent(int bot) const {
    for (const auto& m : members_) if (m.bot == bot) return m.lent;
    return false;
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
            if (player_gang_ >= 0 && gang_of(m.bot) == player_gang_) continue;   // the player's own crew trusts them
            const Vector3 chest{bot->position().x, bot->position().y + 1.2F, bot->position().z};
            const Vector3 to = Vector3Subtract(chest, frame.aim_origin);
            const float along = Vector3DotProduct(to, direction);
            if (along <= 0 || along > 150) continue;
            if (Vector3Length(Vector3Subtract(to, Vector3Scale(direction, along))) < 1.8F) provoke(m.bot, now);
        }
    }

    // A member of another gang on a rival's turf: they know the colours. A warning first; stay
    // past it and they open fire.
    if (player_gang_ >= 0 && frame.player_alive)
        for (const auto& crew : crews_) {
            if (crew.gang == player_gang_ || crew.disbanded || flat(crew.door, frame.player_feet) > rival_turf_alert) continue;
            const auto g = static_cast<std::size_t>(crew.gang);
            if (now - turf_warned_at_[g] > 40) {
                turf_warned_at_[g] = now;
                say(std::string(gangs[g].name) + " know your colours - get off their turf");
            } else if (now - turf_warned_at_[g] >= turf_grace) {
                provoked_until_[g] = std::max(provoked_until_[g], now + 20);
            }
        }

    // The dead come back to the hideout, out of the player's sight if possible.
    for (std::size_t i = 0; i < members_.size(); ++i) {
        auto& m = members_[i];
        auto* bot = bots.find(m.bot);
        if (!bot) continue;
        if (!bot->alive() && !m.dead) { m.dead = true; m.lent = false; m.held = false; m.respawn_at = now + respawn_seconds; }
        if (m.dead && crews_[static_cast<std::size_t>(m.crew)].disbanded) continue;   // broken up: they stay gone
        if (m.dead && m.crew == besieged_) { m.respawn_at = std::max(m.respawn_at, now + respawn_seconds); continue; }  // under siege
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
        next_war_ = now + (150 + random() * 150) * (1 + 3 * calm_);
        if (wars_.size() < 2) plan_war(frame, bots, drivers, cars, region);
    }

    run_heists(frame, bots, drivers, cars, region);

    // Crews far from the player sleep (in place, at their posts) unless they are at war.
    for (auto& m : members_) {
        auto* bot = bots.find(m.bot);
        if (!bot) continue;
        if (m.lent) continue;   // with the player (game::career::Crew keeps him awake)
        if (m.held) { bots.set_asleep(m.bot, true); continue; }   // hands up or cuffed: the game has him
        bool awake = flat(m.post, frame.player_feet) < wake_radius || flat(bot->position(), frame.player_feet) < wake_radius || bots.mounted(m.bot);
        for (const auto& war : wars_)
            if (war.target == m.crew || std::find(war.squad.begin(), war.squad.end(), m.bot) != war.squad.end()) awake = true;
        for (const auto& heist : heists_)
            if (heist.stage != HeistStage::Over && std::find(heist.squad.begin(), heist.squad.end(), m.bot) != heist.squad.end()) awake = true;
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
    walk_patrols(frame, bots);
    drivers.update(frame.dt, bots, cars, region, frame.player_feet, frame.player_alive, 0, world.environment.line_of_sight);
    bots.update(bf, world);
}

void Mafio::walk_patrols(const MafioFrame& frame, ai::BattleRoyaleBots& bots) {
    for (auto& patrol : patrols_) {
        if (patrol.member < 0 || static_cast<std::size_t>(patrol.member) >= members_.size() || patrol.route.empty()) continue;
        const auto& m = members_[static_cast<std::size_t>(patrol.member)];
        auto* bot = bots.find(m.bot);
        // Not while away (lent to the player, held, on a hit squad or a heist), asleep or dead,
        // nor once the crew is broken up.
        if (!bot || !bot->alive() || m.dead || m.lent || m.held || bots.asleep(m.bot) || bots.mounted(m.bot) || heist_of(m.bot) ||
            crews_[static_cast<std::size_t>(m.crew)].disbanded) continue;
        bool away = false;
        for (const auto& war : wars_) away = away || std::find(war.squad.begin(), war.squad.end(), m.bot) != war.squad.end();
        if (away) continue;
        if (patrol.leg_since <= 0) patrol.leg_since = frame.now;
        const Vector3 point = patrol.route[static_cast<std::size_t>(patrol.leg) % patrol.route.size()];
        if (flat(bot->position(), point) < 4 || frame.now - patrol.leg_since > patrol_leg) {
            patrol.leg = (patrol.leg + 1) % static_cast<int>(patrol.route.size());
            patrol.leg_since = frame.now;
        }
        bot->set_roam_center(patrol.route[static_cast<std::size_t>(patrol.leg)], 1.5F);
    }
}

bool Mafio::patroller(int bot) const {
    return std::any_of(patrols_.begin(), patrols_.end(), [&](const Patrol& p) {
        return p.member >= 0 && static_cast<std::size_t>(p.member) < members_.size() && members_[static_cast<std::size_t>(p.member)].bot == bot; });
}

int Mafio::spotted_by_patrol(Vector3 at, int gang, ai::BattleRoyaleBots& bots) const {
    for (const auto& patrol : patrols_) {
        if (patrol.member < 0 || static_cast<std::size_t>(patrol.member) >= members_.size()) continue;
        const auto& m = members_[static_cast<std::size_t>(patrol.member)];
        if (m.dead || m.held || m.lent || crews_[static_cast<std::size_t>(m.crew)].gang != gang) continue;
        const auto* bot = bots.find(m.bot);
        if (bot && bot->alive() && !bots.asleep(m.bot) && flat(bot->position(), at) < patrol_sight) return m.bot;
    }
    return -1;
}

const Heist* Mafio::heist_of(int bot) const {
    for (const auto& h : heists_)
        if (h.stage != HeistStage::Over && std::find(h.squad.begin(), h.squad.end(), bot) != h.squad.end()) return &h;
    return nullptr;
}

bool Mafio::surrendered(int bot) const {
    const auto* h = heist_of(bot);
    return h && std::find(h->surrendered.begin(), h->surrendered.end(), bot) != h->surrendered.end();
}

void Mafio::hold(int bot, bool held, ai::BattleRoyaleBots& bots) {
    if (auto* m = member(bot)) {
        m->held = held;
        bots.set_asleep(bot, held);
        if (!held) if (auto* b = bots.find(bot)) b->set_roam_center(b->position(), 3);
    }
}

bool Mafio::held(int bot) const {
    if (bot >= 1 && static_cast<std::size_t>(bot) <= members_.size() && members_[static_cast<std::size_t>(bot - 1)].bot == bot)
        return members_[static_cast<std::size_t>(bot - 1)].held;
    for (const auto& m : members_) if (m.bot == bot) return m.held;
    return false;
}

void Mafio::settle(int bot) {
    for (auto& h : heists_) {
        if (h.stage == HeistStage::Over) continue;
        std::erase(h.squad, bot);
        std::erase(h.running, bot);
        std::erase(h.surrendered, bot);
        if (h.squad.empty()) h.stage = HeistStage::Over;
    }
}

bool Mafio::stage_heist(HeistKind kind, int crew, Vector3 scene, const std::string& car, ai::BattleRoyaleBots& bots, double now) {
    if (crew < 0 || static_cast<std::size_t>(crew) >= crews_.size() || car.empty()) return false;
    const auto& home = crews_[static_cast<std::size_t>(crew)];
    if (home.disbanded) return false;
    std::vector<int> squad;
    for (std::size_t k = 1; k < home.members.size() && squad.size() < 2; ++k) {   // never the boss
        const int id = home.members[k];
        const auto* bot = bots.find(id);
        const auto* m = member(id);
        const bool busy = std::any_of(wars_.begin(), wars_.end(), [&](const War& w) {
            return std::find(w.squad.begin(), w.squad.end(), id) != w.squad.end(); });
        if (bot && bot->alive() && !bots.mounted(id) && !busy && m && !m->lent && !m->held && !bots.hidden(id) && !heist_of(id) &&
            !(home.casino && (k == 2 || k == 3))) squad.push_back(id);
    }
    if (squad.size() < 2) return false;
    Heist h;
    h.kind = kind;
    h.crew = crew;
    h.gang = home.gang;
    h.squad = squad;
    h.car = car;
    h.scene = scene;
    h.started = now;
    // They are at the scene already (it happened before the call came in), by the car.
    for (std::size_t k = 0; k < squad.size(); ++k) {
        auto* bot = bots.find(squad[k]);
        const Vector3 at{scene.x + (k == 0 ? 1.4F : -1.4F), scene.y, scene.z + .8F};
        bot->set_position(at);
        bot->set_roam_center(at, 2.5F);
        bots.set_asleep(squad[k], false);
    }
    heists_.push_back(std::move(h));
    return true;
}

void Mafio::run_heists(const MafioFrame& frame, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                       vehicles::VehicleSystem& cars, world::VerdaRegion& region) {
    const double now = frame.now;
    for (auto& h : heists_) {
        if (h.stage == HeistStage::Over) continue;
        // The dead and the vanished are out of it.
        std::erase_if(h.squad, [&](int id) { const auto* b = bots.find(id); return !b || !b->alive(); });
        std::erase_if(h.running, [&](int id) { return std::find(h.squad.begin(), h.squad.end(), id) == h.squad.end(); });
        std::erase_if(h.surrendered, [&](int id) { return std::find(h.squad.begin(), h.squad.end(), id) == h.squad.end(); });
        if (h.squad.empty()) { h.stage = HeistStage::Over; continue; }
        const auto& home = crews_[static_cast<std::size_t>(h.crew)];
        const int v = cars.find(h.car);
        const Vector3 lead = bots.find(h.squad.front())->position();
        switch (h.stage) {
            case HeistStage::Waiting: {
                const bool seen = frame.player_alive && flat(frame.player_feet, lead) < flee_radius;
                if (!seen && now - h.started < heist_wait) break;
                // Run for it: into the car and home.
                std::vector<int> aboard;
                for (const int id : h.squad) if (!held(id)) aboard.push_back(id);
                if (!aboard.empty() && v >= 0 && drivers.dispatch(bots, cars, region, aboard, lead, home.door, 40, h.car)) {
                    h.stage = HeistStage::Fleeing;
                    if (seen) say(std::string(gangs[static_cast<std::size_t>(h.gang)].name) + " suspects are running for their car");
                } else h.stage = HeistStage::Stopped;
                if (!seen) { h.escaped = true; h.stage = HeistStage::Over; }   // nobody came: they slipped away
                break;
            }
            case HeistStage::Fleeing: {
                const auto* ride = drivers.ride_of(h.squad.front());
                if (!ride) {
                    // Out of the car: home free, or stopped on the way.
                    if (flat(lead, home.door) < 45) {
                        h.escaped = true;
                        h.stage = HeistStage::Over;
                        for (const int id : h.squad) if (auto* m = member(id)) send_home(*m, bots, frame);
                        say("The suspects made it back to their hideout");
                    } else h.stage = HeistStage::Stopped;
                    break;
                }
                // A police car on their bumper, lights on: they pull over.
                if (v >= 0 && frame.player_police_car && frame.player_lights &&
                    flat(frame.player_feet, cars.vehicles()[static_cast<std::size_t>(v)].position) < tail_distance) h.tailed += frame.dt;
                if (h.tailed >= tail_seconds) {
                    drivers.halt(h.squad.front(), bots, cars, region);
                    h.stage = HeistStage::Stopped;
                    say("The suspects pulled over");
                }
                break;
            }
            case HeistStage::Stopped:
                for (const int id : h.squad) {
                    auto* bot = bots.find(id);
                    if (held(id)) continue;
                    const bool decided = std::find(h.settled.begin(), h.settled.end(), id) != h.settled.end();
                    const float near = flat(frame.player_feet, bot->position());
                    if (!decided && frame.player_alive && near < face_off) {
                        h.settled.push_back(id);
                        // Pulled over, alone, or hurt: hands up. Otherwise mostly hands up; some run.
                        const bool give_up = h.tailed >= tail_seconds ? random() < .85F : bot->health() < 70 || h.squad.size() < 2 || random() < .65F;
                        if (give_up) { h.surrendered.push_back(id); hold(id, true, bots); }
                        else { h.running.push_back(id); say("A suspect is running for it - catch him"); }
                    }
                    // A runner heads away from the officer; caught, or hurt, he gives up.
                    if (std::find(h.running.begin(), h.running.end(), id) != h.running.end()) {
                        if ((frame.player_alive && near < caught) || bot->health() < 70) {
                            std::erase(h.running, id);
                            h.surrendered.push_back(id);
                            hold(id, true, bots);
                        } else if (!provoked(h.gang, now)) {
                            Vector3 away{bot->position().x - frame.player_feet.x, 0, bot->position().z - frame.player_feet.z};
                            away = Vector3Length(away) > .1F ? Vector3Normalize(away) : Vector3{1, 0, 0};
                            bot->set_roam_center(Vector3Add(bot->position(), Vector3Scale(away, 30)), 3);
                        }
                    }
                }
                break;
            case HeistStage::Over: break;
        }
    }
    for (auto& h : heists_) if (h.stage == HeistStage::Over && h.ended < 0) h.ended = now;
    std::erase_if(heists_, [&](const Heist& h) { return h.ended >= 0 && now - h.ended > 10; });
}

void Mafio::end_heists(ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers, vehicles::VehicleSystem& cars) {
    for (auto& h : heists_) {
        for (const int id : h.squad) {
            if (drivers.ride_of(id)) drivers.cancel(id, bots, cars);
            if (held(id)) hold(id, false, bots);
            if (auto* m = member(id); m && !m->dead) {
                MafioFrame frame;
                frame.player_feet = {1e6F, 0, 1e6F};
                send_home(*m, bots, frame);
            }
        }
    }
    heists_.clear();
}

} // namespace outland::game::mafia
