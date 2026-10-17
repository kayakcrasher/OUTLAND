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
        case JobKind::Quest: return "ORDERS";
        case JobKind::Smuggle: return "SMUGGLE";
        case JobKind::Extort: return "EXTORTION";
        case JobKind::TakeTurf: return "TAKE TURF";
        case JobKind::CleanUp: return "CLEAN UP";
        case JobKind::HitChief: return "THE BIG HIT";
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
    state_.honourable = true;      // and an honour of their own to keep
    state_.defended = 0;
    state_.patrol_car.clear();
    job_ = {};
    next_offer_ = -1;
    if (was_cop) state_.record = true;   // a cop gone bad: on file for good
    say(was_cop ? "You turned: the badge is gone, " + gang_name + " take you in. The Policio will never have you back"
                : "Welcome to " + gang_name + ". Respect is earned - and keep your honour: gangsters only");
    return true;
}

bool Career::join_police(const std::string& patrol_car, std::string& why) {
    if (state_.chief_dead) { why = "The Policio has no head since the Chief was hit - the gangs run Verda"; return false; }
    if (state_.record) { why = "The Policio checked your record: no criminals on the force"; return false; }
    if (state_.side == Side::Gang) { why = "You still run with a gang - leave them first"; return false; }
    if (state_.side == Side::Police) { why = "You are already on the force"; return false; }
    state_.side = Side::Police;
    state_.gang = -1;
    state_.respect = 0;
    state_.patrol_car = patrol_car;
    state_.police_stage = PoliceStage::Report;
    job_ = {};
    next_offer_ = -1;
    say(std::string("Sworn in as a Kadeto. Report to ") + chief_name + " at Verda City HQ");
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
        case JobKind::Extort: counted = job_.taken && gang == job_.gang && job_.needed > 1; break;
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
    if (state_.side == Side::Gang) {
        // The people of Verda are not gangsters: the honour is gone, the record is for good.
        state_.record = true;
        if (state_.honourable) {
            state_.honourable = false;
            say("You hurt the people of Verda: no honour now - and a record for good");
        }
        return;
    }
    if (state_.side != Side::Police || !serious) return;
    // A crime on duty: off the force, and a record from now on.
    state_.side = Side::Civilian;
    state_.respect = 0;
    state_.record = true;
    state_.patrol_car.clear();
    job_ = {};
    say("A crime on duty: the Policio takes your badge. You have a record now");
}

void Career::chief_killed() {
    state_.chief_dead = true;
    if (job_.kind != JobKind::HitChief) return;
    // A cop killed: a crime, whoever ordered it.
    state_.record = true;
    state_.honourable = false;
    job_.done = job_.needed;
    finish(true, "");
    say(std::string(chief_name) + " is dead. Verda is yours - +" + std::to_string(hit_pay) + " kronoj");
}

void Career::defended(const std::string& town) {
    if (state_.side != Side::Gang) return;
    ++state_.defended;
    gain(2);
    say("You defended " + town + (state_.honourable ? " - a just fight" : ""));
}

void Career::warned_off_capital(double now) {
    if (state_.side != Side::Police || now - capital_warned_ < 20) return;
    capital_warned_ = now;
    state_.respect = std::max(0.0F, state_.respect - 3);
    say(std::string(chief_name) + ": Not Verda City. Not yet - the towns first. (-3 respect)");
}

int Career::towns_left(const CareerFrame& frame) {
    int n = 0;
    for (const auto& c : frame.crews) n += !c.cleared && !c.capital;
    return n;
}

std::string Career::chief_talk(bool& issue) {
    issue = false;
    const std::string chief = std::string(chief_name) + ": ";
    if (state_.side != Side::Police) {
        if (state_.record) return chief + "I know your record. Get out of my station before I remember your face.";
        return chief + "Verda needs officers. Sign up at any station - then come and see me.";
    }
    switch (state_.police_stage) {
        case PoliceStage::Report:
            state_.police_stage = PoliceStage::Training;
            job_ = {};
            return chief + "So you're the new Kadeto. Nobody wears my badge until they can shoot straight. "
                           "The training camp is outside the city - qualify on all three drills, then report back.";
        case PoliceStage::Training:
            return chief + "Not yet. Three drills at the camp, all qualified. Pistol, rifle, then the movers.";
        case PoliceStage::Graduated: {
            state_.police_stage = PoliceStage::Serving;
            job_ = {};
            next_offer_ = -1;
            issue = true;
            if (state_.respect < 15) gain(15 - state_.respect);
            return chief + "The range says you'll do. Patrolano now - and you get the Interceptor out front. "
                           "L works the lights. The radio is yours.";
        }
        case PoliceStage::Serving: break;
    }
    if (verda_clean_) return chief + "The gangs are gone. Verda is free - you did that. Komisaro suits you.";
    if (campaign_left_ > 0)
        return chief + "Our work is the gangs. " + std::to_string(campaign_left_) + (campaign_left_ == 1 ? " town" : " towns") +
               " still under them - start out there, not here. Verda City is mine to say when.";
    if (campaign_left_ == 0) return chief + "The towns are clean. Now Verda City: bring them down, every one.";
    if (rank() >= 3) return chief + "Leŭtenanto. Verda sleeps better with you on the street. Keep it that way.";
    if (job_.kind != JobKind::None) return chief + "You have a call. Why are you standing in my office?";
    return chief + "Keep your radio on, and keep your nose clean. One crime on duty and the badge is mine.";
}

void Career::passed_training() {
    if (state_.side != Side::Police || state_.police_stage != PoliceStage::Training) return;
    state_.police_stage = PoliceStage::Graduated;
    job_ = {};
    say(std::string("QUALIFIED on every drill - report back to ") + chief_name);
}

void Career::extort_result(bool paid, int family_faction) {
    if (job_.kind != JobKind::Extort) return;
    if (paid) { job_.done = job_.needed; finish(true, ""); return; }
    job_.gang = family_faction;
    job_.needed = 2;
    job_.done = 0;
    job_.detail = "They won't pay: take down two of the family, then they will";
    say("EXTORTION: they reach for their shotguns");
}

void Career::turf_taken(int turf, const std::string& name) {
    if (state_.side != Side::Gang) return;
    if (job_.kind == JobKind::TakeTurf && job_.turf == turf) { job_.done = 1; finish(true, ""); return; }
    gain(5);
    say(name + " is ours now (+5 respect)");
}

void Career::died() {
    if (state_.side == Side::Civilian) return;
    if (job_.kind == JobKind::Quest) return;            // orders stand
    if (job_.kind != JobKind::None) finish(false, "You went down - the job is off");
    else gain(-3);
}

void Career::finish(bool success, const std::string& why) {
    if (success) {
        finished_ = job_;
        const int paid = job_.kind == JobKind::HitChief ? job_.pay : static_cast<int>(std::lround(static_cast<float>(job_.pay) * pay_scale()));
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
            if (c.alive >= 2 && !c.cleared && (!rivals_only || c.gang != state_.gang) &&
                (state_.side != Side::Police || !c.capital || capital_allowed(frame))) pool.push_back(&c);
        if (pool.empty()) return nullptr;
        std::sort(pool.begin(), pool.end(), [&](auto* a, auto* b) { return flat(a->door, frame.player) < flat(b->door, frame.player); });
        const std::size_t choices = std::min<std::size_t>(3, pool.size());
        return pool[std::min(choices - 1, static_cast<std::size_t>(random() * static_cast<float>(choices)))];
    };
    Job job;
    if (state_.side == Side::Gang && !state_.chief_dead && frame.chief_known && state_.respect >= hit_rank_respect &&
        frame.crew_share >= hit_island_share && random() < .35F) {
        // The summit: the boss orders the Chief dead.
        job.kind = JobKind::HitChief;
        job.gang = state_.gang;
        job.target = frame.chief;
        job.needed = 1;
        job.detail = "The boss of " + name_of(state_.gang) + " wants " + chief_name + " dead - at Verda City HQ. Do it and Verda is ours";
        job.pay = hit_pay; job.respect = 30; job.deadline = frame.now + 1800;
    } else if (state_.side == Side::Gang) {
        const float roll = random();
        const auto home = [&]() -> const CrewSpot* {
            const CrewSpot* h = nullptr;
            for (const auto& c : frame.crews)
                if (c.gang == state_.gang && (!h || flat(c.door, frame.player) < flat(h->door, frame.player))) h = &c;
            return h;
        };
        // Rival towns, weakest hold first (the next one to take).
        std::vector<const TurfSpot*> rival_towns;
        for (const auto& t : frame.turfs) if (t.owner != state_.gang && t.owner >= 0 && t.owner < 5) rival_towns.push_back(&t);
        std::sort(rival_towns.begin(), rival_towns.end(), [&](auto* a, auto* b) {
            return a->hold + flat(a->centre, frame.player) * .02F < b->hold + flat(b->centre, frame.player) * .02F; });
        std::vector<const RanchSpot*> leanable;
        for (const auto& r : frame.ranches) if (!r.yours) leanable.push_back(&r);
        if (roll < .16F && !frame.docks.empty() && home()) {
            const auto& dock = frame.docks[std::min(frame.docks.size() - 1, static_cast<std::size_t>(random() * static_cast<float>(frame.docks.size())))];
            job.kind = JobKind::Smuggle;
            job.target = dock.point;
            job.detail = "Contraband came ashore at " + dock.name + ": collect it in a car and run it to your hideout - the police will hear";
            job.needed = 1; job.pay = 1300; job.respect = 12; job.deadline = frame.now + 600;
        } else if (roll < .32F && !leanable.empty()) {
            const auto* r = leanable[std::min(leanable.size() - 1, static_cast<std::size_t>(random() * static_cast<float>(leanable.size())))];
            job.kind = JobKind::Extort;
            job.ranch = r->ranch;
            job.target = r->gate;
            job.detail = "Lean on the " + r->family + " family at " + r->name + ": protection money, or else";
            job.needed = 1; job.pay = 700; job.respect = 9; job.deadline = frame.now + 600;
        } else if (roll < .5F && !rival_towns.empty()) {
            const auto* t = rival_towns.front();
            job.kind = JobKind::TakeTurf;
            job.turf = t->turf;
            job.gang = t->owner;
            job.target = t->centre;
            job.detail = "Take " + t->name + " from the " + name_of(t->owner) + ": tag their walls, hit their crew, break their hold";
            job.needed = 1; job.pay = 1500; job.respect = 15; job.deadline = frame.now + 1200;
        } else if (roll < .62F && !frame.rigs.empty()) {
            const auto& rig = frame.rigs[std::min(frame.rigs.size() - 1, static_cast<std::size_t>(random() * static_cast<float>(frame.rigs.size())))];
            job.kind = JobKind::RobRig;
            job.vehicle = rig.vehicle;
            job.target = rig.position;
            job.detail = "Hijack the " + rig.name + " and drive it to your crew's hideout";
            job.needed = 1; job.pay = 900; job.respect = 12; job.deadline = frame.now + 480;
        } else if (const auto* crew = pick_crew(true)) {
            const bool drive_by = roll < .8F;
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
        // The campaign: the Chief's orders to break up the gangs, the towns first, Verda City last.
        const bool capital_ok = capital_allowed(frame);
        const CrewSpot* order = nullptr;
        for (const auto& c : frame.crews)
            if (!c.cleared && (!c.capital || capital_ok) && (!order || flat(c.door, frame.player) < flat(order->door, frame.player))) order = &c;
        if (order && random() < .4F) {
            job.kind = JobKind::CleanUp;
            job.gang = order->gang; job.crew = order->crew; job.target = order->door;
            job.needed = std::max(1, order->alive);
            job.detail = std::string(chief_name) + "'s orders: break up the " + name_of(order->gang) + " in " + order->town +
                         (order->capital ? " - Verda City, at last. Every one of them" : " - every one of them, so they never come back");
            job.pay = order->capital ? 4000 : 1400; job.respect = order->capital ? 25 : 14; job.deadline = frame.now + 1500;
        } else if (!frame.wars.empty() && roll < .5F) {
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
    if (job_.kind == JobKind::HitChief && frame.chief_known) job_.target = frame.chief;
    if (state_.side == Side::Police && state_.police_stage != PoliceStage::Serving) {
        // A recruit has orders, not calls.
        Job orders;
        orders.kind = JobKind::Quest;
        orders.title = job_name(JobKind::Quest);
        if (state_.police_stage == PoliceStage::Training) {
            orders.detail = "Qualify on all three drills at the training camp";
            orders.target = frame.camp;
            if (!frame.camp_known) orders.target = frame.player;
        } else {
            orders.detail = std::string(state_.police_stage == PoliceStage::Report ? "Report to " : "Qualified - report back to ") + chief_name + " at Verda City HQ";
            orders.target = frame.chief_known ? frame.chief : frame.player;
        }
        job_ = orders;
        return;
    }
    if (state_.side == Side::Police && !frame.crews.empty()) {
        // The campaign's progress: how many towns the gangs still hold, and the end of them.
        campaign_left_ = towns_left(frame);
        const bool all = std::all_of(frame.crews.begin(), frame.crews.end(), [](const CrewSpot& c) { return c.cleared; });
        if (all && !verda_clean_) {
            verda_clean_ = true;
            if (state_.respect < 95) gain(95 - state_.respect);
            say("VERDA IS CLEAN: the last of the gangs is gone - " + std::string(chief_name) + " makes you Komisaro");
        }
        if (all && job_.kind == JobKind::None) return;   // nothing left to fight
    }
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
        case JobKind::Smuggle: {
            const bool in_car = !frame.driving.empty();
            if (!job_.taken && in_car && flat(frame.player, job_.target) < 15) {
                job_.taken = true;
                tipoff_ = true;
                say("SMUGGLE: the load is aboard - someone tipped off the police. Get it home");
            }
            if (job_.taken) {
                const CrewSpot* home = nullptr;
                for (const auto& c : frame.crews)
                    if (c.gang == state_.gang && (!home || flat(c.door, frame.player) < flat(home->door, frame.player))) home = &c;
                if (home) {
                    job_.target = home->door;
                    if (in_car && flat(frame.player, home->door) < drop_radius) { job_.done = 1; finish(true, ""); return; }
                }
            }
            break;
        }
        case JobKind::Extort:
            if (!job_.taken && flat(frame.player, job_.target) < 25) { job_.taken = true; demanded_ = true; }
            break;
        case JobKind::BreakUpWar: {
            const auto war = std::find_if(frame.wars.begin(), frame.wars.end(), [&](const WarSpot& w) { return w.attackers == job_.gang; });
            if (war != frame.wars.end()) job_.target = war->where;
            break;
        }
        case JobKind::CleanUp: {
            const auto crew = std::find_if(frame.crews.begin(), frame.crews.end(), [&](const CrewSpot& c) { return c.crew == job_.crew; });
            if (crew == frame.crews.end() || crew->cleared) { job_.done = job_.needed; break; }
            job_.done = std::max(0, job_.needed - crew->alive);
            if (crew->alive == 0) job_.done = job_.needed;
            break;
        }
        default: break;
    }
    if (job_.needed > 0 && job_.done >= job_.needed) finish(true, "");
}

std::string Career::serialise() const {
    std::ostringstream out;
    out << "career 4 " << static_cast<int>(state_.side) << ' ' << state_.gang << ' ' << state_.respect << ' '
        << (state_.record ? 1 : 0) << ' ' << state_.jobs_done << ' ' << (state_.patrol_car.empty() ? "-" : state_.patrol_car) << ' '
        << static_cast<int>(state_.police_stage) << ' ' << (state_.honourable ? 1 : 0) << ' ' << state_.defended << ' ' << (state_.chief_dead ? 1 : 0) << '\n';
    return out.str();
}

bool Career::parse(const std::string& text) {
    std::istringstream in(text);
    std::string tag, car;
    int version = 0, side = 0, gang = -1, record = 0, jobs = 0;
    float respect = 0;
    if (!(in >> tag >> version >> side >> gang >> respect >> record >> jobs >> car) || tag != "career" || version < 1 || version > 4 ||
        side < 0 || side > 2 || !std::isfinite(respect))
        return false;
    // Officers from before the questline are already serving.
    int stage = static_cast<int>(PoliceStage::Serving);
    if (version >= 2 && (!(in >> stage) || stage < 0 || stage > 3)) return false;
    int honour = 1, defended = 0;
    if (version >= 3 && (!(in >> honour >> defended) || defended < 0)) return false;
    int chief_dead = 0;
    if (version >= 4 && !(in >> chief_dead)) return false;
    CareerState s;
    s.side = static_cast<Side>(side);
    s.gang = s.side == Side::Gang ? gang : -1;
    s.respect = std::clamp(respect, 0.0F, respect_max);
    s.record = record != 0;
    s.honourable = honour != 0;
    s.defended = defended;
    s.chief_dead = chief_dead != 0;
    s.jobs_done = std::max(0, jobs);
    s.patrol_car = s.side == Side::Police && car != "-" ? car : std::string();
    s.police_stage = s.side == Side::Police ? static_cast<PoliceStage>(stage) : PoliceStage::Report;
    if (s.side == Side::Gang && s.gang < 0) s = {};
    restore(s);
    next_offer_ = -1;
    return true;
}

} // namespace outland::game::career
