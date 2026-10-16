#include "outland/game/career/Career.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace outland::game::career {
namespace {
constexpr float rank_floor[]{0, 15, 40, 70, 95};
constexpr const char* gang_ranks[]{"Novulo", "Soldato", "Kapo", "Leŭtenanto", "Estro"};
constexpr const char* police_ranks[]{"Kadeto", "Patrolano", "Serĝento", "Leŭtenanto", "Komisaro"};
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
}

const char* job_name(JobKind kind) {
    switch (kind) {
        case JobKind::KillRivals: return "HIT";
        case JobKind::RobRig: return "ROB THE RIG";
        case JobKind::DriveBy: return "DRIVE-BY";
        case JobKind::Patrol: return "PATROL";
        case JobKind::Raid: return "RAID";
        case JobKind::BreakUpWar: return "GANG WAR";
        case JobKind::None: break;
    }
    return "";
}

int Career::rank_of(float respect) {
    int rank = 0;
    for (int r = 0; r < 5; ++r) if (respect >= rank_floor[r]) rank = r;
    return rank;
}

const char* Career::rank_name() const {
    if (state_.side == Side::Civilian) return "";
    return (state_.side == Side::Police ? police_ranks : gang_ranks)[rank()];
}

float Career::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 7; rng_ ^= rng_ << 17;
    return static_cast<float>(rng_ >> 40) / 16777216.0F;
}

bool Career::join_gang(int gang, const std::string& gang_name, std::string& why) {
    if (gang < 0) { why = "Nobody here to join"; return false; }
    if (state_.side == Side::Gang && state_.gang == gang) { why = "You are already one of them"; return false; }
    const bool was_cop = state_.side == Side::Police;
    state_.side = Side::Gang;
    state_.gang = gang;
    state_.respect = 0;            // a new crew: respect is earned again
    state_.record = true;          // for good
    state_.patrol_car.clear();
    job_ = {};
    next_offer_ = -1;
    say(was_cop ? "You turned: the badge is gone, " + gang_name + " take you in. The Policio will never have you back"
                : "Welcome to " + gang_name + ". Respect is earned - wait for the word");
    return true;
}

bool Career::join_police(const std::string& patrol_car, std::string& why) {
    if (state_.record) { why = "The Policio checked your record: no Krimuloj on the force"; return false; }
    if (state_.side == Side::Police) { why = "You are already on the force"; return false; }
    state_.side = Side::Police;
    state_.gang = -1;
    state_.respect = 0;
    state_.patrol_car = patrol_car;
    job_ = {};
    next_offer_ = -1;
    say("Sworn in: Kadeto of the Policio de Verda. Your patrol car is outside - listen for calls");
    return true;
}

void Career::quit() {
    if (state_.side == Side::Civilian) return;
    say(state_.side == Side::Police ? "You handed in your badge" : "You walked away from the gang");
    state_.side = Side::Civilian;
    state_.gang = -1;
    state_.respect = 0;
    state_.patrol_car.clear();
    job_ = {};
}

void Career::gain(float amount) {
    const int before = rank();
    state_.respect = std::clamp(state_.respect + amount, 0.0F, respect_max);
    if (rank() > before) say(std::string("Promoted: ") + rank_name() + " - better pay from now on");
}

void Career::killed(int gang, bool from_vehicle) {
    if (gang < 0 || state_.side == Side::Civilian) return;
    if (state_.side == Side::Gang && gang == state_.gang) {
        // One of their own: the crew does not forget.
        if (state_.respect < 15) {
            say("You killed one of your own - you are out of the gang");
            state_.side = Side::Civilian; state_.gang = -1; state_.respect = 0; job_ = {};
            return;
        }
        state_.respect = std::max(0.0F, state_.respect - 15);
        say("You killed one of your own: -15 respect");
        return;
    }
    bool counted = false;
    switch (job_.kind) {
        case JobKind::KillRivals: case JobKind::Raid: counted = gang == job_.gang; break;
        case JobKind::DriveBy: counted = gang == job_.gang && from_vehicle; break;
        case JobKind::BreakUpWar: counted = gang == job_.gang || gang == job_.crew; break;
        default: break;
    }
    if (counted) {
        ++job_.done;
        if (job_.done < job_.needed) say(std::string(job_name(job_.kind)) + ": " + std::to_string(job_.done) + " of " + std::to_string(job_.needed));
    } else {
        gain(.5F);                 // a rival (or, for the police, any Krimulo) off the street
    }
}

void Career::crime(bool serious) {
    if (state_.side != Side::Police || !serious) return;
    // A crime on duty: off the force, and a record from now on.
    state_.side = Side::Civilian;
    state_.respect = 0;
    state_.record = true;
    state_.patrol_car.clear();
    job_ = {};
    say("A crime on duty: the Policio takes your badge. You have a record now");
}

void Career::died() {
    if (state_.side == Side::Civilian) return;
    if (job_.kind != JobKind::None) finish(false, "You went down - the job is off");
    else gain(-3);
}

void Career::finish(bool success, const std::string& why) {
    if (success) {
        const int paid = static_cast<int>(std::lround(static_cast<float>(job_.pay) * pay_scale()));
        pay_ += paid;
        ++state_.jobs_done;
        say(std::string(job_name(job_.kind)) + " DONE: +" + std::to_string(static_cast<int>(job_.respect)) + " respect, +" +
            std::to_string(paid) + " kronoj");
        gain(job_.respect);        // after the line, so a promotion is what shows
    } else {
        gain(-job_.respect * .5F);
        say(why + " (-" + std::to_string(static_cast<int>(std::ceil(job_.respect * .5F))) + " respect)");
    }
    job_ = {};
    next_offer_ = -1;
}

void Career::offer(const CareerFrame& frame) {
    const auto name_of = [&](int gang) {
        return gang >= 0 && static_cast<std::size_t>(gang) < frame.gang_names.size() ? frame.gang_names[static_cast<std::size_t>(gang)] : std::string("Krimuloj");
    };
    // The crews worth going after (enough of them standing), nearest first; one of the closest few.
    const auto pick_crew = [&](bool rivals_only) -> const CrewSpot* {
        std::vector<const CrewSpot*> pool;
        for (const auto& c : frame.crews)
            if (c.alive >= 2 && (!rivals_only || c.gang != state_.gang)) pool.push_back(&c);
        if (pool.empty()) return nullptr;
        std::sort(pool.begin(), pool.end(), [&](auto* a, auto* b) { return flat(a->door, frame.player) < flat(b->door, frame.player); });
        const std::size_t choices = std::min<std::size_t>(3, pool.size());
        return pool[std::min(choices - 1, static_cast<std::size_t>(random() * static_cast<float>(choices)))];
    };
    Job job;
    if (state_.side == Side::Gang) {
        const float roll = random();
        if (roll < .3F && !frame.rigs.empty()) {
            const auto& rig = frame.rigs[std::min(frame.rigs.size() - 1, static_cast<std::size_t>(random() * static_cast<float>(frame.rigs.size())))];
            job.kind = JobKind::RobRig;
            job.vehicle = rig.vehicle;
            job.target = rig.position;
            job.detail = "Hijack the " + rig.name + " and drive it to your crew's hideout";
            job.needed = 1; job.pay = 900; job.respect = 12; job.deadline = frame.now + 480;
        } else if (const auto* crew = pick_crew(true)) {
            const bool drive_by = roll < .6F;
            job.kind = drive_by ? JobKind::DriveBy : JobKind::KillRivals;
            job.gang = crew->gang; job.crew = crew->crew; job.target = crew->door;
            job.needed = std::min(drive_by ? 2 : 3, crew->alive);
            job.detail = drive_by ? "Drive past the " + name_of(crew->gang) + " at " + crew->town + " and hit " + std::to_string(job.needed) + " from the car"
                                  : "Take out " + std::to_string(job.needed) + " of the " + name_of(crew->gang) + " at " + crew->town;
            job.pay = drive_by ? 450 : 320; job.respect = drive_by ? 9 : 6;
            job.deadline = frame.now + (drive_by ? 420 : 360);
        }
    } else if (state_.side == Side::Police) {
        const float roll = random();
        if (!frame.wars.empty() && roll < .5F) {
            const auto& war = frame.wars.front();
            job.kind = JobKind::BreakUpWar;
            job.gang = war.attackers; job.crew = war.defenders; job.target = war.where;
            job.needed = 2;
            job.detail = "Shots fired at " + war.town + ": " + name_of(war.attackers) + " hit the " + name_of(war.defenders) + ". Stop 2 of them";
            job.pay = 500; job.respect = 10; job.deadline = frame.now + 300;
        } else if (roll < .55F && frame.towns.size() >= 2) {
            job.kind = JobKind::Patrol;
            job.needed = 3;
            std::vector<const TownSpot*> towns;
            for (const auto& t : frame.towns) if (flat(t.centre, frame.player) > 120) towns.push_back(&t);
            if (towns.empty()) for (const auto& t : frame.towns) towns.push_back(&t);
            const auto* first = towns[std::min(towns.size() - 1, static_cast<std::size_t>(random() * static_cast<float>(towns.size())))];
            job.target = first->centre;
            job.detail = "Patrol in your car: show the flag in " + first->name + " and two more towns";
            job.pay = 260; job.respect = 5; job.deadline = frame.now + 600;
        } else if (const auto* crew = pick_crew(false)) {
            job.kind = JobKind::Raid;
            job.gang = crew->gang; job.crew = crew->crew; job.target = crew->door;
            job.needed = std::min(3, crew->alive);
            job.detail = "Raid the " + name_of(crew->gang) + " hideout at " + crew->town + ": take down " + std::to_string(job.needed);
            job.pay = 480; job.respect = 9; job.deadline = frame.now + 420;
        }
    }
    if (job.kind == JobKind::None) { next_offer_ = frame.now + job_gap; return; }
    job.title = job_name(job.kind);
    job_ = job;
    say((state_.side == Side::Police ? "CALL: " : "THE WORD: ") + job.detail);
}

void Career::update(const CareerFrame& frame) {
    if (state_.side == Side::Civilian) { job_ = {}; return; }
    if (state_.side == Side::Gang && state_.gang < 0) { quit(); return; }
    if (next_offer_ < 0) next_offer_ = frame.now + (state_.jobs_done == 0 ? 4 : job_gap);
    if (job_.kind == JobKind::None) {
        if (frame.alive && frame.now >= next_offer_) offer(frame);
        return;
    }
    if (frame.now > job_.deadline) { finish(false, std::string(job_name(job_.kind)) + ": too slow"); return; }
    switch (job_.kind) {
        case JobKind::RobRig: {
            const bool at_wheel = !job_.vehicle.empty() && frame.driving == job_.vehicle;
            if (at_wheel && !job_.taken) { job_.taken = true; say("ROB THE RIG: you have it - get it to the hideout"); }
            // Home: the nearest hideout of the player's own gang.
            const CrewSpot* home = nullptr;
            for (const auto& c : frame.crews)
                if (c.gang == state_.gang && (!home || flat(c.door, frame.player) < flat(home->door, frame.player))) home = &c;
            if (job_.taken && home) {
                job_.target = home->door;
                if (at_wheel && flat(frame.player, home->door) < drop_radius) { job_.done = 1; finish(true, ""); return; }
            } else {
                const auto rig = std::find_if(frame.rigs.begin(), frame.rigs.end(), [&](const RigSpot& r) { return r.vehicle == job_.vehicle; });
                if (rig == frame.rigs.end() && !at_wheel) { finish(false, "ROB THE RIG: the rig is gone"); return; }
                if (rig != frame.rigs.end()) job_.target = rig->position;
            }
            break;
        }
        case JobKind::Patrol:
            if (frame.driving == state_.patrol_car && !frame.driving.empty() && flat(frame.player, job_.target) < arrive_radius) {
                ++job_.done;
                if (job_.done >= job_.needed) break;
                // On to another town, not the one just visited.
                std::vector<const TownSpot*> towns;
                for (const auto& t : frame.towns) if (flat(t.centre, frame.player) > 150) towns.push_back(&t);
                if (towns.empty()) { job_.done = job_.needed; break; }
                const auto* next = towns[std::min(towns.size() - 1, static_cast<std::size_t>(random() * static_cast<float>(towns.size())))];
                job_.target = next->centre;
                say("PATROL: " + std::to_string(job_.done) + " of " + std::to_string(job_.needed) + " - on to " + next->name);
            }
            break;
        case JobKind::BreakUpWar: {
            const auto war = std::find_if(frame.wars.begin(), frame.wars.end(), [&](const WarSpot& w) { return w.attackers == job_.gang; });
            if (war != frame.wars.end()) job_.target = war->where;
            break;
        }
        default: break;
    }
    if (job_.needed > 0 && job_.done >= job_.needed) finish(true, "");
}

std::string Career::serialise() const {
    std::ostringstream out;
    out << "career 1 " << static_cast<int>(state_.side) << ' ' << state_.gang << ' ' << state_.respect << ' '
        << (state_.record ? 1 : 0) << ' ' << state_.jobs_done << ' ' << (state_.patrol_car.empty() ? "-" : state_.patrol_car) << '\n';
    return out.str();
}

bool Career::parse(const std::string& text) {
    std::istringstream in(text);
    std::string tag, car;
    int version = 0, side = 0, gang = -1, record = 0, jobs = 0;
    float respect = 0;
    if (!(in >> tag >> version >> side >> gang >> respect >> record >> jobs >> car) || tag != "career" || version != 1 ||
        side < 0 || side > 2 || !std::isfinite(respect))
        return false;
    CareerState s;
    s.side = static_cast<Side>(side);
    s.gang = s.side == Side::Gang ? gang : -1;
    s.respect = std::clamp(respect, 0.0F, respect_max);
    s.record = record != 0 || s.side == Side::Gang;
    s.jobs_done = std::max(0, jobs);
    s.patrol_car = s.side == Side::Police && car != "-" ? car : std::string();
    if (s.side == Side::Gang && s.gang < 0) s = {};
    restore(s);
    next_offer_ = -1;
    return true;
}

} // namespace outland::game::career
