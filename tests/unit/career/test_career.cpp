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
        check(c.join_gang(0, "La Verda Mano", why) && c.state().side == Side::Gang && c.state().record, "join a gang (a record from now on)");
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

        // The record shuts the police door.
        check(!c.join_police("car", why) && why.find("record") != std::string::npos, "no Krimuloj on the force");
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
            c.update(f);
            check(c.job().done == stop, "a patrol needs the patrol car");
            f.driving = "interceptor_1";
            c.update(f);
        }
        check(c.job().kind == JobKind::None && c.state().jobs_done == 1, "patrol done");
        check(wait_for(c, JobKind::Raid, now), "a raid call");
        const int gang = c.job().gang;
        for (int k = 0; k < c.job().needed; ++k) c.killed(gang, false);
        c.update(world(now));
        check(c.state().jobs_done == 2, "raid done");
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

        // A cop can always go bad: a serious crime ends it, and leaves a record.
        c.crime(false);
        check(c.on_duty(), "a minor matter is overlooked");
        c.crime(true);
        check(!c.on_duty() && c.state().record, "a crime on duty: fired, with a record");
        check(!c.join_police("police_1", why), "and never back");
        Career e;
        e.join_police("police_3", why);
        check(e.join_gang(1, "Montaj Lupoj", why) && e.state().side == Side::Gang && e.state().record, "or just join a gang");
    }
    // Ranks and saving.
    {
        check(Career::rank_of(0) == 0 && Career::rank_of(15) == 1 && Career::rank_of(40) == 2 && Career::rank_of(100) == 4, "ranks");
        Career c;
        std::string why;
        c.join_gang(2, "La Ora Ankro", why);
        Career back;
        check(back.parse(c.serialise()) && back.state().side == Side::Gang && back.state().gang == 2 && back.state().record, "saved and loaded");
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
