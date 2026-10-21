// Careers: joining a gang or the police, respect and ranks, jobs and their pay, and the one-way door.
#include "outland/game/career/Career.hpp"
#include <cmath>
#include <iostream>
using namespace outland::game::career;
namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
CareerFrame world(double now) {
    CareerFrame f;
    f.now = now;
    f.player = {0, 0, 0};
    f.gang_names = {"La Verda Mano", "Montaj Lupoj", "La Ora Ankro"};
    f.crews = {{0, 0, {30, 0, 0}, "Verda", 6}, {1, 1, {400, 0, 0}, "Roka", 4}, {2, 2, {-600, 0, 0}, "Espera", 4}};
    f.rigs = {{"semi_1", {200, 0, 200}, "supply rig"}};
    f.towns = {{"Verda", {0, 0, 0}}, {"Roka", {400, 0, 0}}, {"Espera", {-600, 0, 0}}, {"Porto Luma", {0, 0, 900}}};
    return f;
}
// Run the clock until a job of `kind` is offered (other offers are turned down by failing them).
bool wait_for(Career& c, JobKind kind, double& now) {
    for (int i = 0; i < 400; ++i) {
        now += 1;
        c.update(world(now));
        if (c.job().kind == kind) return true;
        if (c.job().kind != JobKind::None) { now += 2000; c.update(world(now)); }   // let it lapse
    }
    return false;
}
}

int main() {
    // A gang: join, get jobs, earn respect and pay.
    {
        Career c;
        std::string why;
        check(c.join_gang(0, "La Verda Mano", why) && c.state().side == Side::Gang && !c.state().record && c.state().honourable,
              "join a gang: honourable, no record");
        check(!c.join_gang(0, "La Verda Mano", why), "already in");
        check(std::string(c.rank_name()) == "Novulo" && c.state().respect == 0, "start at the bottom");
        double now = 0;
        check(wait_for(c, JobKind::KillRivals, now), "a hit is offered");
        const auto hit = c.job();
        check(hit.gang != 0 && hit.needed >= 2 && hit.target.x != 0, "on a rival crew");
        const float before = c.state().respect;
        c.killed(0, false);                                   // one of their own
        check(c.state().side == Side::Civilian, "killing your own as a Novulo gets you thrown out");
        check(c.join_gang(0, "La Verda Mano", why), "back in");
        check(wait_for(c, JobKind::KillRivals, now), "another hit");
        const int target = c.job().gang;
        c.killed(target == 1 ? 2 : 1, false);                 // someone else: not this job
        for (int k = 0; k < c.job().needed; ++k) c.killed(target, false);
        c.update(world(now));
        check(c.job().kind == JobKind::None && c.state().respect > before && c.state().jobs_done == 1, "job done: respect");
        const int pay = c.take_pay();
        check(pay > 0 && c.take_pay() == 0, "and pay, once");

        // A drive-by only counts from a car.
        check(wait_for(c, JobKind::DriveBy, now), "a drive-by is offered");
        const int rivals = c.job().gang;
        c.killed(rivals, false);
        check(c.job().done == 0, "on foot does not count");
        c.killed(rivals, true); c.killed(rivals, true);
        c.update(world(now));
        check(c.job().kind == JobKind::None && c.state().jobs_done == 2, "from the car it does");

        // Rob the rig: take it, drive it home.
        check(wait_for(c, JobKind::RobRig, now), "a rig job is offered");
        auto f = world(now);
        f.driving = "semi_1"; f.player = {200, 0, 200};
        c.update(f);
        check(c.job().taken && std::abs(c.job().target.x - 30) < .1F, "at the wheel: now the hideout");
        f.player = {35, 0, 4};
        c.update(f);
        check(c.job().kind == JobKind::None && c.state().jobs_done == 3, "delivered");
        check(c.take_pay() >= 900, "the rig pays best");

        // Too slow fails, and costs respect.
        check(wait_for(c, JobKind::KillRivals, now), "another hit");
        const float had = c.state().respect;
        now += 1000;
        c.update(world(now));
        check(c.job().kind == JobKind::None && c.state().respect < had, "a lapsed job costs respect");

        // Still in a gang: not on the force.
        check(!c.join_police("car", why) && why.find("leave") != std::string::npos, "leave the gang first");
        // Honour: defending the town is a just fight; hurting the people of Verda ends it.
        const float r0 = c.state().respect;
        c.defended("Verda");
        check(c.state().defended == 1 && c.state().respect > r0 && c.state().honourable, "defending your town");
        c.crime(true);
        check(!c.state().honourable && c.state().record && c.state().side == Side::Gang, "a crime: no honour, a record for good");
        Career saved;
        check(saved.parse(c.serialise()) && !saved.state().honourable && saved.state().record && saved.state().defended == 1, "honour is saved");
        c.quit();
        check(!c.join_police("car", why) && why.find("record") != std::string::npos, "the record shuts the police door");
    }
    // The underworld's jobs: smuggling (a load, a tip-off, home), extortion (pay or fight), turf.
    {
        Career c;
        std::string why;
        c.join_gang(0, "La Verda Mano", why);
        const auto full = [](double now) {
            auto f = world(now);
            f.docks = {{"Porto Luma docks", {1800, 0, -400}}};
            f.ranches = {{2, "Ranĉo Sunleviĝo", "Villarreal", {730, 0, -180}, false}};
            f.turfs = {{0, "Verda", {0, 0, 0}, 0, 100}, {1, "Roka", {400, 0, 0}, 4, 35}, {2, "Espera", {-600, 0, 0}, 2, 90}};
            return f;
        };
        bool smuggle = false, extort = false, turf = false;
        double now = 0;
        for (int i = 0; i < 3000 && !(smuggle && extort && turf); ++i) {
            now += 1;
            auto f = full(now);
            c.update(f);
            const auto kind = c.job().kind;
            if (kind == JobKind::Smuggle && !smuggle) {
                smuggle = true;
                f.player = {1800, 0, -405};
                f.driving = "";
                c.update(f);
                check(!c.job().taken, "the load needs a car");
                f.driving = "van";
                c.update(f);
                check(c.job().taken && c.take_tipoff() && !c.take_tipoff() && std::abs(c.job().target.x - 30) < .1F, "loaded, the police tipped, now home");
                f.player = {32, 0, 2};
                c.update(f);
                Job done;
                check(c.job().kind == JobKind::None && c.take_finished(done) && done.kind == JobKind::Smuggle && c.take_pay() >= 1300, "delivered");
            } else if (kind == JobKind::Extort && !extort) {
                extort = true;
                check(c.job().ranch == 2, "the family's ranch");
                f.player = {735, 0, -170};
                c.update(f);
                check(c.take_extort_arrival() && !c.take_extort_arrival(), "the demand is made");
                c.extort_result(false, 7);
                check(c.job().needed == 2 && c.job().gang == 7, "they fight");
                c.killed(3, false);
                c.killed(7, false); c.killed(7, false);
                c.update(f);
                Job done;
                check(c.job().kind == JobKind::None && c.take_finished(done) && done.kind == JobKind::Extort && done.ranch == 2, "beaten, they pay");
            } else if (kind == JobKind::TakeTurf && !turf) {
                turf = true;
                check(c.job().turf == 1 && c.job().gang == 4, "the weakest rival town first");
                c.turf_taken(2, "Espera");
                check(c.job().kind == JobKind::TakeTurf, "another town falling is not this job");
                c.turf_taken(1, "Roka");
                Job done;
                check(c.job().kind == JobKind::None && c.take_finished(done) && done.kind == JobKind::TakeTurf, "Roka taken");
            } else if (kind != JobKind::None) {
                now += 2000; f.now = now; c.update(f);
            }
        }
        check(smuggle && extort && turf, "all three jobs come up");
        // A town the Policio cleared is nobody's to take; a load with no hideout left to go to is off.
        Career d;
        d.join_gang(0, "La Verda Mano", why);
        bool cleared_turf = false, lost_home = false;
        for (int i = 0; i < 6000 && !(cleared_turf && lost_home); ++i) {
            now += 1;
            auto f = full(now);
            f.turfs[1].cleared = true;
            d.update(f);
            const auto kind = d.job().kind;
            if (kind == JobKind::TakeTurf && !cleared_turf) {
                cleared_turf = true;
                check(d.job().turf == 2, "a cleared town is never the one to take");
            } else if (kind == JobKind::Smuggle && !lost_home) {
                lost_home = true;
                f.player = {1800, 0, -405};
                f.driving = "van";
                d.update(f);
                f.crews[0].cleared = true;   // the crew's hideout broken up meanwhile
                d.update(f);
                check(d.job().kind == JobKind::None, "no hideout to run it to: the job is off, not stuck");
            }
            if (d.job().kind != JobKind::None) { now += 2000; f.now = now; d.update(f); }
        }
        check(cleared_turf && lost_home, "both come up");
    }
    // The police: patrols in the car, raids, calls; going bad.
    {
        Career c;
        std::string why;
        check(c.join_police("police_1", why) && c.on_duty() && std::string(c.rank_name()) == "Kadeto", "sworn in");
        // The questline: report to the Chief, train, report back, then the Interceptor and calls.
        auto q = world(1);
        q.chief = {11, 0, -175}; q.chief_known = true; q.camp = {-400, 0, -60}; q.camp_known = true;
        c.update(q);
        check(c.job().kind == JobKind::Quest && std::abs(c.job().target.x - 11) < .1F && !c.serving(), "orders: report to the Chief");
        for (int i = 0; i < 30; ++i) { q.now += 1; c.update(q); }
        check(c.job().kind == JobKind::Quest, "no calls for a recruit");
        bool car = false;
        c.chief_talk(car);
        c.update(q);
        check(!car && c.police_stage() == PoliceStage::Training && std::abs(c.job().target.x + 400) < .1F, "sent to the camp");
        c.chief_talk(car);
        check(!car && c.police_stage() == PoliceStage::Training, "not until qualified");
        c.died();
        check(c.state().respect == 0 && c.on_duty(), "dying in training costs nothing");
        c.passed_training();
        c.update(q);
        check(c.police_stage() == PoliceStage::Graduated && std::abs(c.job().target.x - 11) < .1F, "qualified: back to the Chief");
        c.chief_talk(car);
        check(car && c.serving() && std::string(c.rank_name()) == "Patrolano", "the Interceptor, and a promotion");
        c.issue_car("interceptor_1");
        check(c.state().patrol_car == "interceptor_1", "the new car is the patrol car");
        double now = 0;
        check(wait_for(c, JobKind::Patrol, now), "a patrol call");
        auto f = world(now);
        for (int stop = 0; stop < 3; ++stop) {
            f.player = c.job().target;
            f.driving = "some_other_car";
            f.driving_police = false;
            c.update(f);
            check(c.job().done == stop, "a patrol needs a police car");
            if (stop == 1) { f.driving = "police_7"; f.driving_police = true; }   // any police car will do
            else f.driving = "interceptor_1";
            c.update(f);
        }
        check(c.job().kind == JobKind::None && c.state().jobs_done == 1, "patrol done");
        check(wait_for(c, JobKind::Raid, now), "a raid call");
        const int gang = c.job().gang;
        c.arrested(gang);                                    // one taken alive (USE)
        check(c.job().done == 1, "an arrest counts on a raid");
        for (int k = 1; k < c.job().needed; ++k) c.killed(gang, false);
        c.update(world(now));
        check(c.state().jobs_done == 2, "raid done");
        const int paid = c.take_pay();
        c.arrested(gang);
        check(c.take_pay() == 60 && paid > 0, "an arrest off the job pays a little");
        // Gang war calls when there is one.
        Career d;
        d.join_police("police_2", why);
        { bool car = false; d.chief_talk(car); d.passed_training(); d.chief_talk(car); }
        bool war_call = false;
        for (int i = 0; i < 400 && !war_call; ++i) {
            auto w = world(i);
            w.wars = {{1, 2, {400, 0, 0}, "Roka"}};
            d.update(w);
            war_call = d.job().kind == JobKind::BreakUpWar;
            if (d.job().kind != JobKind::None && !war_call) { w.now += 2000; d.update(w); i += 2000; }
        }
        check(war_call, "a gang war brings a call");
        {
            // The shooting stops before the officer gets there: the call points at a crew of theirs.
            auto w = world(d.job().deadline - 10);
            d.update(w);
            check(d.job().kind == JobKind::BreakUpWar && (std::abs(d.job().target.x - 400) < 1 || std::abs(d.job().target.x + 600) < 1),
                  "a war that's over: on to the nearest of either crew");
            d.killed(1, false); d.killed(2, false);
            d.update(w);
            check(d.job().kind == JobKind::None, "and two of them stopped ends it");
        }

        // A cop can always go bad: a serious crime ends it, and leaves a record.
        c.crime(false);
        check(c.on_duty(), "a minor matter is overlooked");
        c.crime(true);
        check(!c.on_duty() && c.state().record, "a crime on duty: fired, with a record");
        check(!c.join_police("police_1", why), "and never back");
        Career e;
        e.join_police("police_3", why);
        check(e.join_gang(1, "Montaj Lupoj", why) && e.state().side == Side::Gang && e.state().record, "or just join a gang (a cop gone bad is on file)");
    }
    // The police campaign: the towns first, Verda City last, then the gangs are gone.
    {
        Career c;
        std::string why;
        c.join_police("police_1", why);
        { bool car = false; c.chief_talk(car); c.passed_training(); c.chief_talk(car); }
        auto f = world(0);
        f.crews[0].capital = true;   // Verda's crew
        double now = 0;
        bool order = false;
        for (int i = 0; i < 400 && !order; ++i) {
            f.now = now += 1;
            c.update(f);
            order = c.job().kind == JobKind::CleanUp;
            if (c.job().kind != JobKind::None && !order) { f.now = now += 2000; c.update(f); }
        }
        check(order && c.job().crew != 0, "the Chief's orders: a town first, not Verda City");
        { bool car = false; check(c.chief_talk(car).find("start out there") != std::string::npos, "the Chief says the towns first"); }
        // Every one of them down: the crew is broken up. Their dead stay down meanwhile, and going
        // down yourself doesn't call it off.
        const int first = c.job().crew;
        check(c.besieging() == first, "the crew is under siege");
        c.died();
        check(c.job().kind == JobKind::CleanUp, "the Chief's orders stand when you go down");
        f.crews[static_cast<std::size_t>(first)].alive = 0;
        c.update(f);
        check(c.job().kind == JobKind::None && c.state().jobs_done >= 1, "a town cleaned up");
        Job done;
        check(c.take_finished(done) && done.kind == JobKind::CleanUp && done.crew == first, "the game hears which crew to break up");
        f.crews[static_cast<std::size_t>(first)].cleared = true;
        for (auto& crew : f.crews) if (!crew.capital) { crew.cleared = true; crew.alive = 0; }
        check(Career::capital_allowed(f), "the towns are clean");
        order = false;
        for (int i = 0; i < 400 && !order; ++i) {
            f.now = now += 1;
            c.update(f);
            order = c.job().kind == JobKind::CleanUp;
            if (c.job().kind != JobKind::None && !order) { f.now = now += 2000; c.update(f); }
        }
        check(order && c.job().crew == 0, "then Verda City");
        f.crews[0].alive = 0;
        c.update(f);
        f.crews[0].cleared = true;
        c.update(f);
        check(c.verda_clean() && c.state().respect >= 95, "Verda is clean: Komisaro");
        // An officer warned off the capital too early loses a little respect.
        Career d;
        d.join_police("police_2", why);
        { bool car = false; d.chief_talk(car); d.passed_training(); d.chief_talk(car); }
        const float r = d.state().respect;
        d.warned_off_capital(100);
        d.warned_off_capital(105);
        check(std::abs(d.state().respect - (r - 3)) < .01F, "warned off Verda City (once in a while)");
    }
    // The big hit: a Leŭtenanto gets the boss's order on the Chief; 500 000 and the island.
    {
        Career c;
        std::string why;
        c.join_gang(0, "La Verda Mano", why);
        auto st = c.state(); st.respect = 75; c.restore(st);
        auto f = world(0);
        f.chief = {11, 0, -185}; f.chief_known = true;
        f.crew_share = .5F;   // half the island: not yet
        bool early = false;
        double t = 0;
        for (int i = 0; i < 400; ++i) {
            f.now = t += 1;
            c.update(f);
            early |= c.job().kind == JobKind::HitChief;
            if (c.job().kind != JobKind::None) c.restore(st);
        }
        check(!early, "no hit on the Chief before the gang runs three quarters of Verda");
        f.crew_share = .8F;
        bool order = false;
        double now = t;
        for (int i = 0; i < 400 && !order; ++i) {
            f.now = now += 1;
            c.update(f);
            order = c.job().kind == JobKind::HitChief;
            if (c.job().kind != JobKind::None && !order) c.restore(st);   // turned down, at no cost
        }
        check(order && std::abs(c.job().target.z + 185) < .1F, "the boss orders the Chief dead");
        c.take_pay();
        c.chief_killed();
        check(c.take_pay() == Career::hit_pay && c.state().chief_dead && c.job().kind == JobKind::None, "the hit pays 500 000 kronoj");
        Job done;
        check(c.take_finished(done) && done.kind == JobKind::HitChief, "the game hears the Chief is dead");
        check(c.state().record && !c.state().honourable, "killing a cop is no gangster's quarrel");
        Career back;
        check(back.parse(c.serialise()) && back.state().chief_dead, "the Chief stays dead");
        back.quit();
        check(!back.join_police("car", why) && why.find("no head") != std::string::npos, "the Policio takes nobody now");
        // Below Leŭtenanto: never ordered.
        Career low;
        low.join_gang(1, "Montaj Lupoj", why);
        bool ordered = false;
        for (int i = 0; i < 300; ++i) { f.now = now += 1; low.update(f); ordered |= low.job().kind == JobKind::HitChief;
            if (low.job().kind != JobKind::None) { f.now = now += 2000; low.update(f); } }
        check(!ordered, "only a Leŭtenanto gets that order");
    }
    // Ranks and saving.
    {
        check(Career::rank_of(0) == 0 && Career::rank_of(15) == 1 && Career::rank_of(40) == 2 && Career::rank_of(100) == 4, "ranks");
        Career c;
        std::string why;
        c.join_gang(2, "La Ora Ankro", why);
        Career back;
        check(back.parse(c.serialise()) && back.state().side == Side::Gang && back.state().gang == 2 && back.state().honourable, "saved and loaded");
        check(back.parse("career 2 1 2 10 1 0 - 0") && back.state().record && back.state().honourable, "older saves keep their record");
        Career cop;
        cop.join_police("police_9", why);
        { bool car = false; cop.chief_talk(car); }
        check(back.parse(cop.serialise()) && back.on_duty() && back.state().patrol_car == "police_9" &&
              back.police_stage() == PoliceStage::Training, "an officer keeps the car and the training stage");
        check(back.parse("career 1 2 -1 30 0 4 police_7") && back.serving(), "an officer from before the questline is serving");
        check(!back.parse("career 1 7 0 0 0 0 -") && !back.parse("nonsense"), "bad saves refused");
    }
    if (failures) return 1;
    std::cout << "[PASS] Careers: gangs, police, respect, jobs, the one-way door\n";
    return 0;
}
