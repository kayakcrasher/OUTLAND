#include "outland/game/zombies/ZombieEcology.hpp"
#include "outland/world/Highways.hpp"
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/game/sound/SoundBus.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::zombies {
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
Vector3 toward(Vector3 from, Vector3 to, float step) {
    const float d = flat(from, to);
    if (d <= step || d < 1e-3F) return {to.x, from.y, to.z};
    return {from.x + (to.x - from.x) / d * step, from.y, from.z + (to.z - from.z) / d * step};
}
}

const char* zombie_state_name(ZombieState state) {
    switch (state) {
        case ZombieState::Dormant: return "dormant";
        case ZombieState::Wander: return "wandering";
        case ZombieState::Investigate: return "investigating";
        case ZombieState::Horde: return "in a horde";
        case ZombieState::Dead: return "dead";
    }
    return "?";
}

float ZombieEcology::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 7; rng_ ^= rng_ << 17;
    return static_cast<float>(rng_ >> 40) / static_cast<float>(1ULL << 24);
}

void ZombieEcology::build(const world::VerdaRegion& region, const characters::CharacterRegistry* registry, std::uint64_t seed) {
    region_ = &region;
    seed_ = seed | 1; rng_ = seed_ * 0x9e3779b97f4a7c15ULL | 1;
    sources_.clear(); zombies_.clear(); hordes_.clear(); towns_.clear();
    roads_.build(region);
    const auto& settlements = region.settlements();
    const auto standable = [&](Vector3 p) {
        const float ground = world::terrain::TerrainHeight::sample(p.x, p.z);
        if (!std::isfinite(ground) || ground <= world::layout::sea_level + .5F) return false;
        return !world::physics::WorldCollision::body_blocked({p.x, ground, p.z}, ground, region, .5F);
    };
    const auto apart = [&](Vector3 p, float distance) {
        return std::none_of(sources_.begin(), sources_.end(), [&](const ZombieSource& s) {return flat(s.position, p) < distance;});
    };
    // Authored nests.
    for (std::size_t s = 0; s < settlements.size(); ++s)
        for (const auto& marker : settlements[s].gameplay_markers)
            if (marker.type == world::GameplayMarkerType::ZombieSpawn && marker.enabled)
                sources_.push_back({marker.position, static_cast<int>(s), std::max(6.0F, marker.size.x * 4), true, false});
    // Nests round every town: the capital is the worst.
    for (std::size_t s = 0; s < settlements.size(); ++s) {
        const auto& town = settlements[s];
        towns_.push_back(town.center);
        if (town.id.starts_with("ranch_")) continue; // farms are not towns: no nests
        const bool capital = town.id.find("capital") != std::string::npos;
        const bool hamlet = town.id.starts_with("hamlet_"); // a few trailers: two nests
        const int wanted = town.infected > 0 ? std::clamp(town.infected / 6, 2, 12) : capital ? 8 : hamlet ? 2 : 4;
        const float reach = capital ? 150.0F : 90.0F;
        int placed = 0;
        for (int attempt = 0; attempt < 80 && placed < wanted; ++attempt) {
            const float angle = random() * 2 * PI, radius = 25 + random() * reach;
            const Vector3 p{town.center.x + std::cos(angle) * radius, 0, town.center.z + std::sin(angle) * radius};
            if (!standable(p) || !apart(p, 25)) continue;
            sources_.push_back({{p.x, world::terrain::TerrainHeight::sample(p.x, p.z), p.z}, static_cast<int>(s), 14, false, false});
            ++placed;
        }
    }
    // Wrecks by the long roads between towns (a highway comes in short pieces round its curves;
    // any piece of it out in the country will do).
    int roadside = 0;
    for (const auto& edge : roads_.edges()) {
        const bool highway = edge.width >= world::highways::min_width;
        if (roadside >= 10 || edge.length < (highway ? 40 : 250)) continue;
        const auto a = roads_.nodes()[static_cast<std::size_t>(edge.a)], b = roads_.nodes()[static_cast<std::size_t>(edge.b)];
        const Vector3 mid = Vector3Lerp(a, b, .5F);
        if (highway && std::any_of(settlements.begin(), settlements.end(), [&](const world::Settlement& town) {
                return flat(town.center, mid) < 300; })) continue;
        const float l = flat(a, b);
        const Vector3 side{-(b.z - a.z) / l, 0, (b.x - a.x) / l};
        const float off = edge.width * .5F + 12;
        const Vector3 p{mid.x + side.x * off, 0, mid.z + side.z * off};
        if (!standable(p) || !apart(p, highway ? 500 : 120)) continue;
        sources_.push_back({{p.x, world::terrain::TerrainHeight::sample(p.x, p.z), p.z}, -1, 16, false, true});
        ++roadside;
    }
    if (sources_.empty()) return;
    // The population, spread over the nests.
    std::vector<const characters::CharacterDefinition*> looks;
    for (int k = 0; k < 6 && registry; ++k)
        if (const auto* d = registry->choose(characters::CharacterPool::Creature, seed_ + static_cast<std::uint64_t>(k) * 7919)) looks.push_back(d);
    for (int i = 0; i < config_.population; ++i) {
        Zombie z;
        z.id = i;
        z.source = i % static_cast<int>(sources_.size());
        const auto& source = sources_[static_cast<std::size_t>(z.source)];
        const float angle = random() * 2 * PI, radius = random() * source.radius;
        z.position = {source.position.x + std::cos(angle) * radius, source.position.y, source.position.z + std::sin(angle) * radius};
        z.target = z.position;
        if (!looks.empty()) z.character_id = looks[static_cast<std::size_t>(i) % looks.size()]->id;
        zombies_.push_back(z);
    }
}

void ZombieEcology::reset(characters::NpcSystem& npcs) {
    for (auto& z : zombies_) {
        if (z.physical) npcs.despawn_resident(body_id_base + z.id);
        z.source = z.id % static_cast<int>(sources_.size()); // home nest again
        const auto& source = sources_[static_cast<std::size_t>(z.source)];
        const float angle = random() * 2 * PI, radius = random() * source.radius;
        z.position = {source.position.x + std::cos(angle) * radius, source.position.y, source.position.z + std::sin(angle) * radius};
        z.target = z.position; z.state = ZombieState::Wander; z.horde = -1; z.physical = false;
        z.think_at = 0; z.until = 0; z.died_at = -1e9;
    }
    hordes_.clear();
    now_ = horde_clock_ = 0; bridge_clock_ = 0; killed_ = 0; tuned_ = -1; heard_up_to_ = 0;
}

void ZombieEcology::clear_area(Vector3 around, float radius) {
    std::vector<int> elsewhere;
    for (std::size_t s = 0; s < sources_.size(); ++s) if (flat(sources_[s].position, around) > radius + 30) elsewhere.push_back(static_cast<int>(s));
    if (elsewhere.empty()) return;
    for (auto& z : zombies_) {
        if (z.physical || flat(z.position, around) > radius) continue;
        z.source = elsewhere[static_cast<std::size_t>(random() * static_cast<float>(elsewhere.size())) % elsewhere.size()];
        z.position = sources_[static_cast<std::size_t>(z.source)].position;
        z.horde = -1;
        wander_target(z);
    }
}

void ZombieEcology::clear(characters::NpcSystem& npcs) {
    for (const auto& z : zombies_) if (z.physical) npcs.despawn_resident(body_id_base + z.id);
    zombies_.clear(); sources_.clear(); hordes_.clear();
}

float ZombieEcology::speed(ZombieState state) const {
    const bool dark = !day();
    switch (state) {
        case ZombieState::Investigate: return dark ? config_.night_investigate_speed : config_.investigate_speed;
        case ZombieState::Wander: return dark ? config_.night_wander_speed : config_.wander_speed;
        default: return 0;
    }
}

void ZombieEcology::wander_target(Zombie& z) {
    const auto& source = sources_[static_cast<std::size_t>(z.source)];
    const float angle = random() * 2 * PI, radius = random() * source.radius;
    z.target = {source.position.x + std::cos(angle) * radius, source.position.y, source.position.z + std::sin(angle) * radius};
    z.think_at = now_ + 20 + random() * 30;
    // By day half of them settle down and lie still until the next thought.
    if (day() && random() < .55F) z.state = ZombieState::Dormant;
    else z.state = ZombieState::Wander;
}

void ZombieEcology::step_abstract(Zombie& z, float dt) {
    switch (z.state) {
        case ZombieState::Dead: {
            // The nest sends another, out of the player's sight.
            const auto& source = sources_[static_cast<std::size_t>(z.source)];
            if (now_ - z.died_at >= config_.respawn_seconds && flat(source.position, player_) > config_.respawn_clearance) {
                z.position = source.position; z.horde = -1;
                wander_target(z);
            }
            return;
        }
        case ZombieState::Dormant:
            if (!day() || now_ >= z.think_at) wander_target(z);
            return;
        case ZombieState::Investigate:
            if (now_ >= z.until) {wander_target(z); return;}
            break;
        case ZombieState::Wander:
            if (now_ >= z.think_at) wander_target(z);
            break;
        case ZombieState::Horde: {
            if (z.horde < 0) {wander_target(z); return;}
            const auto found = std::find_if(hordes_.begin(), hordes_.end(), [&](const Horde& h) {return h.id == z.horde;});
            if (found == hordes_.end()) {z.horde = -1; wander_target(z); return;}
            z.position = Vector3Add(found->position, z.offset);
            return;
        }
    }
    z.position = toward(z.position, z.target, speed(z.state) * dt);
}

void ZombieEcology::hear(const sound::SoundBus& sounds) {
    for (const auto& event : sounds.events()) {
        if (event.serial <= heard_up_to_) continue;
        const float hearing = night() ? config_.night_hearing : 1.0F;
        const bool gunshot = event.kind == sound::SoundKind::Gunshot;
        for (auto& z : zombies_) {
            if (z.state == ZombieState::Dead || z.state == ZombieState::Horde) continue;
            if (!sound::SoundBus::audible(event, z.position, hearing)) continue;
            const float angle = random() * 2 * PI, scatter = 2 + random() * 5;
            z.state = ZombieState::Investigate;
            z.target = {event.position.x + std::cos(angle) * scatter, event.position.y, event.position.z + std::sin(angle) * scatter};
            z.until = now_ + (gunshot ? 45 : 25);
        }
        // A gunshot turns a horde that hears it.
        if (gunshot) for (auto& horde : hordes_) {
            if (!sound::SoundBus::audible(event, horde.position, hearing * 1.2F)) continue;
            horde.route = roads_.route(horde.position, event.position);
            horde.progress = 0; horde.destination = -1; horde.turned = true;
        }
    }
    heard_up_to_ = sounds.latest();
}

void ZombieEcology::disband(std::size_t index) {
    const auto& horde = hordes_[index];
    // Whoever is left joins the nest nearest where the horde stopped.
    int nearest = 0; float best = 1e9F;
    for (std::size_t s = 0; s < sources_.size(); ++s) {
        const float d = flat(sources_[s].position, horde.position);
        if (d < best) {best = d; nearest = static_cast<int>(s);}
    }
    for (const int id : horde.members) {
        auto& z = zombies_[static_cast<std::size_t>(id)];
        if (z.horde != horde.id) continue;
        z.horde = -1;
        if (z.state == ZombieState::Horde) {z.source = nearest; wander_target(z);}
    }
    hordes_.erase(hordes_.begin() + static_cast<std::ptrdiff_t>(index));
}

void ZombieEcology::manage_hordes(float dt) {
    // Hordes walk their road; at the end (or at dawn) they break up.
    for (std::size_t i = hordes_.size(); i-- > 0;) {
        auto& horde = hordes_[i];
        horde.progress += config_.horde_speed * dt;
        float yaw = 0;
        horde.position = world::roads::route_point(horde.route, horde.progress, &yaw, false);
        horde.position.y = world::terrain::TerrainHeight::sample(horde.position.x, horde.position.z);
        const bool alive = std::any_of(horde.members.begin(), horde.members.end(), [&](int id) {
            const auto& z = zombies_[static_cast<std::size_t>(id)];
            return z.horde == horde.id && z.state != ZombieState::Dead;});
        if (!alive || horde.progress >= horde.route.length || day()) disband(i);
    }
    // At night a nest may send a horde off along the roads, often toward the player's town.
    horde_clock_ += dt;
    if (!night() || horde_clock_ < config_.horde_interval || static_cast<int>(hordes_.size()) >= config_.max_hordes || towns_.size() < 2) return;
    horde_clock_ = 0;
    std::vector<int> counts(sources_.size(), 0);
    for (const auto& z : zombies_)
        if ((z.state == ZombieState::Wander || z.state == ZombieState::Dormant) && !z.physical) ++counts[static_cast<std::size_t>(z.source)];
    std::vector<std::size_t> ready;
    for (std::size_t s = 0; s < sources_.size(); ++s) if (counts[s] >= 4 && flat(sources_[s].position, player_) > config_.materialize_radius) ready.push_back(s);
    if (ready.empty()) return;
    const auto from = ready[static_cast<std::size_t>(random() * static_cast<float>(ready.size())) % ready.size()];
    // Destination: the player's town half the time, otherwise another town.
    std::size_t player_town = 0, other = 0;
    for (std::size_t t = 0; t < towns_.size(); ++t) if (flat(towns_[t], player_) < flat(towns_[player_town], player_)) player_town = t;
    do other = static_cast<std::size_t>(random() * static_cast<float>(towns_.size())) % towns_.size();
    while (towns_.size() > 1 && flat(towns_[other], sources_[from].position) < 150);
    const std::size_t to = random() < .5F && flat(towns_[player_town], sources_[from].position) > 150 ? player_town : other;
    Horde horde;
    horde.id = ++next_horde_;
    horde.destination = static_cast<int>(to);
    horde.route = roads_.route(sources_[from].position, towns_[to]);
    if (!horde.route.complete || horde.route.length < 60) return;
    horde.position = sources_[from].position;
    int taken = 0;
    const int size = 6 + static_cast<int>(random() * 5);
    for (auto& z : zombies_) {
        if (taken >= size || z.source != static_cast<int>(from) || z.physical) continue;
        if (z.state != ZombieState::Wander && z.state != ZombieState::Dormant) continue;
        z.state = ZombieState::Horde; z.horde = horde.id;
        const float angle = random() * 2 * PI, radius = 1 + random() * 4;
        z.offset = {std::cos(angle) * radius, 0, std::sin(angle) * radius};
        horde.members.push_back(z.id);
        ++taken;
    }
    if (taken >= 4) hordes_.push_back(std::move(horde));
    else for (const int id : horde.members) {auto& z = zombies_[static_cast<std::size_t>(id)]; z.horde = -1; wander_target(z);}
}

// Faster at night, worse eyes in the dark; bodies take the current settings.
void ZombieEcology::tune(characters::NpcSystem& npcs) {
    const int band = night() ? 2 : day() ? 0 : 1;
    if (band == tuned_) return;
    tuned_ = band;
    auto tuning = npcs.tuning(characters::CharacterPool::Creature);
    tuning.movement_speed = band == 2 ? 3.0F : band == 1 ? 2.4F : 1.8F;
    tuning.run_multiplier = band == 2 ? 1.6F : 1.35F;
    tuning.detection_radius = band == 2 ? 18.0F : band == 1 ? 24.0F : 30.0F;
    tuning.health = 120;
    tuning.attack_damage = 12;
    tuning.attack_interval = 1.2F;
    tuning.activation_distance = config_.dematerialize_radius;
    tuning.despawn_distance = config_.dematerialize_radius + 20;
    npcs.configure(characters::CharacterPool::Creature, tuning);
}

void ZombieEcology::sync_bodies(characters::NpcSystem& npcs) {
    using characters::NpcState;
    int bodies = 0;
    for (auto& z : zombies_) {
        if (!z.physical) continue;
        const int index = npcs.find_resident(body_id_base + z.id);
        if (index < 0) {z.physical = false; continue;}
        const auto& actor = npcs.actors()[static_cast<std::size_t>(index)];
        z.position = actor.position;
        if (z.state != ZombieState::Dead && (actor.state == NpcState::Dead || actor.health <= 0)) {
            z.state = ZombieState::Dead; z.died_at = now_; ++killed_;
            if (z.horde >= 0) z.horde = -1;
        }
        // Out of range: back to the abstract tier (corpses are cleared away with it).
        if (flat(z.position, player_) > config_.dematerialize_radius) {
            npcs.despawn_resident(body_id_base + z.id); z.physical = false;
            if (z.state == ZombieState::Horde && z.horde >= 0) {
                const auto horde = std::find_if(hordes_.begin(), hordes_.end(), [&](const Horde& h) {return h.id == z.horde;});
                if (horde != hordes_.end()) z.offset = Vector3Subtract(z.position, horde->position);
            }
            continue;
        }
        ++bodies;
        if (z.state == ZombieState::Dead) continue;
        Vector3 anchor = z.position; float radius = 1; bool hurry = false;
        switch (z.state) {
            case ZombieState::Wander: anchor = z.target; radius = 2; break;
            case ZombieState::Investigate: anchor = z.target; radius = 3; hurry = !day(); break;
            case ZombieState::Horde: {
                const auto horde = std::find_if(hordes_.begin(), hordes_.end(), [&](const Horde& h) {return h.id == z.horde;});
                if (horde != hordes_.end()) anchor = Vector3Add(horde->position, z.offset);
                radius = 3;
                break;
            }
            default: break;
        }
        npcs.direct_resident(body_id_base + z.id, anchor, radius, hurry);
    }
    // Nearest first, within the body budget.
    std::vector<std::pair<float, int>> candidates;
    for (const auto& z : zombies_)
        if (!z.physical && z.state != ZombieState::Dead && !z.character_id.empty()) {
            const float d = flat(z.position, player_);
            if (d <= config_.materialize_radius) candidates.emplace_back(d, z.id);
        }
    std::sort(candidates.begin(), candidates.end());
    for (const auto& [distance, id] : candidates) {
        if (bodies >= config_.max_bodies) break;
        auto& z = zombies_[static_cast<std::size_t>(id)];
        Vector3 start = z.position;
        const float ground = world::terrain::TerrainHeight::sample(start.x, start.z);
        start.y = region_ ? world::physics::WorldCollision::ground_height({start.x, ground, start.z}, ground + .3F, *region_) : ground;
        characters::ResidentSpawn spawn;
        spawn.resident = body_id_base + z.id;
        spawn.character_id = z.character_id;
        spawn.pool = characters::CharacterPool::Creature;
        spawn.position = start;
        spawn.anchor = z.state == ZombieState::Dormant ? start : z.target;
        spawn.anchor_radius = 2;
        spawn.health = npcs.tuning(characters::CharacterPool::Creature).health;
        spawn.yaw_degrees = random() * 360;
        npcs.spawn_resident(spawn);
        z.physical = true; z.position = start;
        ++bodies;
    }
}

void ZombieEcology::update(float dt, const ZombieFrame& frame, characters::NpcSystem& npcs) {
    if (zombies_.empty() || !std::isfinite(dt) || dt <= 0) return;
    dt = std::min(dt, .25F);
    now_ += dt;
    player_ = frame.player;
    daylight_ = std::isfinite(frame.daylight) ? std::clamp(frame.daylight, 0.0F, 1.0F) : 1.0F;
    tune(npcs);
    manage_hordes(dt);
    for (auto& z : zombies_) {
        if (!z.physical) {step_abstract(z, dt); continue;}
        // Bodies move themselves; the ecology only changes their minds (a corpse stays a corpse
        // until it is out of sight and the nest can replace it).
        if (z.state == ZombieState::Dead) continue;
        if ((z.state == ZombieState::Wander || z.state == ZombieState::Dormant) && now_ >= z.think_at) wander_target(z);
        else if (z.state == ZombieState::Investigate && now_ >= z.until) wander_target(z);
    }
    bridge_clock_ += dt;
    if (bridge_clock_ >= .25F) {bridge_clock_ = 0; sync_bodies(npcs);}
}

ZombieStats ZombieEcology::stats() const {
    ZombieStats s;
    s.killed = killed_; s.hordes = static_cast<int>(hordes_.size());
    for (const auto& z : zombies_) {
        if (z.state == ZombieState::Dead) continue;
        ++s.alive;
        s.physical += z.physical;
        s.dormant += z.state == ZombieState::Dormant;
        s.investigating += z.state == ZombieState::Investigate;
        s.in_hordes += z.state == ZombieState::Horde;
        s.near += flat(z.position, player_) < 60;
    }
    return s;
}
}
