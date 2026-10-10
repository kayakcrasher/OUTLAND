#include "outland/game/law/Justice.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/game/life/LifeSimulation.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::law {
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
Vector3 eye(Vector3 feet) { return {feet.x, feet.y + 1.6F, feet.z}; }
bool officer(const life::Resident& r) { return r.occupation == life::Occupation::PoliceOfficer; }
}

const char* crime_name(CrimeKind kind) {
    switch (kind) {
        case CrimeKind::Assault: return "assault";
        case CrimeKind::Murder: return "murder";
        case CrimeKind::AttackOnPolice: return "attack on an officer";
        default: return "shooting";
    }
}

int crime_severity(CrimeKind kind) {
    switch (kind) {
        case CrimeKind::Assault: return 2;
        case CrimeKind::Murder: return 3;
        case CrimeKind::AttackOnPolice: return 4;
        default: return 1;
    }
}

float Justice::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 7; rng_ ^= rng_ << 17;
    return static_cast<float>(rng_ >> 40) / static_cast<float>(1ULL << 24);
}

float Justice::sight(float daylight) const {
    daylight = std::isfinite(daylight) ? std::clamp(daylight, 0.0F, 1.0F) : 1.0F;
    return config_.sight_night + (config_.sight_day - config_.sight_night) * daylight;
}

void Justice::reset(life::LifeSimulation* life, characters::NpcSystem* npcs) {
    if (life && npcs) for (const int id : responders_) {npcs->arm_resident(id, false); life->release(id);}
    pending_.clear(); responders_.clear();
    wanted_ = reports_ = 0; last_seen_ = last_fired_ = investigate_until_ = redirect_at_ = -1e9;
    arrest_clock_ = 0; arrested_ = false; message_.clear(); message_time_ = 0;
}

void Justice::crime(CrimeKind kind, Vector3 where, int victim, const JusticeFrame& frame, const life::LifeSimulation& life) {
    if (!std::isfinite(where.x) || !std::isfinite(where.z)) return;
    if (kind == CrimeKind::ShotsFired) last_fired_ = frame.now;
    const float seeing = sight(frame.daylight);
    int nearest_hearer = -1; float nearest_hearing = config_.hearing;
    for (const auto& r : life.residents()) {
        if (!r.alive) continue;
        const auto at = life.position(r);
        const float distance = flat(at, frame.player);
        bool saw = false;
        if (r.id == victim) saw = kind == CrimeKind::Assault || kind == CrimeKind::AttackOnPolice; // the living victim saw it all
        else if (r.physical) saw = distance <= seeing && (!frame.line_of_sight || frame.line_of_sight(eye(at), eye(frame.player)));
        else saw = !life.indoors(r) && distance <= seeing * .5F; // off-screen, outdoors and close
        if (!saw) {
            // Shots are heard much further than they are seen, but only one caller reports them.
            const float to_shot = flat(at, where);
            if (kind == CrimeKind::ShotsFired && to_shot < nearest_hearing && !life.indoors(r)) {nearest_hearing = to_shot; nearest_hearer = r.id;}
            continue;
        }
        Witness witness{r.id, kind, where, true, frame.now};
        if (!officer(r)) witness.call_at = frame.now + config_.call_min + (config_.call_max - config_.call_min) * random();
        // One pending call per witness: keep the worst thing they saw.
        const auto existing = std::find_if(pending_.begin(), pending_.end(), [&](const Witness& w) {return w.resident == r.id;});
        if (existing == pending_.end()) pending_.push_back(witness);
        else if (!existing->saw || crime_severity(kind) > crime_severity(existing->kind)) {
            existing->kind = kind; existing->where = where; existing->saw = true;
        }
    }
    if (nearest_hearer >= 0 && std::none_of(pending_.begin(), pending_.end(), [&](const Witness& w) {return w.resident == nearest_hearer;})) {
        const auto& r = life.residents()[static_cast<std::size_t>(nearest_hearer)];
        pending_.push_back({nearest_hearer, kind, where, false,
            officer(r) ? frame.now : frame.now + config_.call_min + (config_.call_max - config_.call_min) * random()});
    }
}

void Justice::dispatch(int count, Vector3 where, life::LifeSimulation& life) {
    dispatched_to_ = where;
    for (const int id : responders_) life.dispatch(id, where);
    std::vector<std::pair<float, int>> free;
    for (const auto& r : life.residents())
        if (r.alive && officer(r) && !r.responding) free.emplace_back(flat(life.position(r), where), r.id);
    std::sort(free.begin(), free.end());
    for (const auto& [distance, id] : free) {
        if (static_cast<int>(responders_.size()) >= count) break;
        life.dispatch(id, where);
        responders_.push_back(id);
    }
}

void Justice::report(const Witness& witness, const JusticeFrame& frame, life::LifeSimulation& life, characters::NpcSystem& npcs) {
    (void)npcs;
    ++reports_;
    const auto& r = life.residents()[static_cast<std::size_t>(witness.resident)];
    if (witness.saw) {
        const int before = wanted_;
        wanted_ = std::max(wanted_, crime_severity(witness.kind));
        last_known_ = witness.where; last_seen_ = std::max(last_seen_, frame.now);
        dispatch(1 + wanted_, witness.where, life);
        say(officer(r) ? std::string("An officer saw the ") + crime_name(witness.kind)
            : wanted_ > before ? "A witness called the police: " + std::string(crime_name(witness.kind))
                               : std::string("Another witness called the police"));
    } else {
        // Anonymous "shots fired": someone comes to look, nobody knows who to look for.
        investigate_until_ = frame.now + config_.investigate_seconds;
        dispatch(std::max(1, static_cast<int>(responders_.size())), witness.where, life);
        if (wanted_ == 0) say("Shots fired reported - police on the way");
    }
}

void Justice::stand_down(life::LifeSimulation& life, characters::NpcSystem& npcs, const char* why) {
    for (const int id : responders_) {npcs.arm_resident(id, false); life.release(id);}
    responders_.clear();
    wanted_ = 0; arrest_clock_ = 0; investigate_until_ = -1e9;
    if (why) say(why);
}

void Justice::update(const JusticeFrame& frame, life::LifeSimulation& life, characters::NpcSystem& npcs) {
    arrested_ = false;
    if (std::isfinite(frame.dt) && frame.dt > 0) message_time_ = std::max(0.0F, message_time_ - frame.dt);
    if (frame.player_fired) last_fired_ = frame.now;
    const auto& residents = life.residents();
    if (!frame.player_alive) {
        if (!responders_.empty() || wanted_ > 0) stand_down(life, npcs, nullptr);
        pending_.clear();
        return;
    }
    // Calls get through, unless the witness did not live to make them.
    std::vector<Witness> due;
    std::erase_if(pending_, [&](const Witness& w) {
        if (w.resident < 0 || static_cast<std::size_t>(w.resident) >= residents.size() || !residents[static_cast<std::size_t>(w.resident)].alive) return true;
        if (w.call_at > frame.now) return false;
        due.push_back(w);
        return true;
    });
    for (const auto& w : due) report(w, frame, life, npcs);
    std::erase_if(responders_, [&](int id) {return !residents[static_cast<std::size_t>(id)].alive;});

    // Officers who can see the player keep the description current; any officer on the street
    // joins in once the player is wanted.
    const float seeing = sight(frame.daylight);
    bool seen = false;
    if (wanted_ > 0) for (const auto& r : residents) {
        if (!r.alive || !officer(r) || !r.physical) continue;
        if (flat(r.body, frame.player) > seeing) continue;
        if (frame.line_of_sight && !frame.line_of_sight(eye(r.body), eye(frame.player))) continue;
        seen = true;
        if (!r.responding) {life.dispatch(r.id, frame.player); responders_.push_back(r.id);}
    }
    if (seen) {last_seen_ = frame.now; last_known_ = frame.player;}
    if (wanted_ > 0 && frame.now >= redirect_at_ && flat(last_known_, dispatched_to_) > 10) {
        redirect_at_ = frame.now + 2;
        dispatched_to_ = last_known_;
        for (const int id : responders_) life.dispatch(id, last_known_);
    }
    for (const int id : responders_) npcs.arm_resident(id, wanted_ >= 2);

    // One star: an officer at arm's length and a calm player is an arrest.
    if (wanted_ == 1) {
        bool close = false;
        for (const int id : responders_) {
            const auto& r = residents[static_cast<std::size_t>(id)];
            close |= r.physical && flat(r.body, frame.player) <= config_.arrest_radius;
        }
        if (close && frame.now - last_fired_ >= config_.calm_seconds) arrest_clock_ += frame.dt;
        else arrest_clock_ = 0;
        if (arrest_clock_ >= config_.arrest_seconds) {
            arrested_ = true;
            stand_down(life, npcs, "ARRESTED - let off with a warning");
            return;
        }
    }
    // Out of sight long enough and the search is called off.
    if (wanted_ > 0 && frame.now - last_seen_ > config_.evade_base + config_.evade_per_star * static_cast<float>(wanted_)) {
        stand_down(life, npcs, "You lost the police");
        return;
    }
    if (wanted_ == 0 && !responders_.empty() && frame.now > investigate_until_) stand_down(life, npcs, nullptr);
}
}
