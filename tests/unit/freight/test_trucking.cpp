// Driving for Verda Transporto: signing on with the boss (never a serving officer), loads from the
// ship, the depot and the ranches, the right rig, loading and unloading, and pay by the trip.
#include "outland/game/freight/Trucking.hpp"
#include <cmath>
#include <iostream>
using namespace outland::game::freight;
namespace {
int failures = 0;
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
HaulWorld island() {
    HaulWorld w;
    w.depot = {-560, 0, -84};
    w.cargo_quay = {2214, -28, -481};
    w.cattle_quay = {2213, -28, -433};
    w.terminal = true;
    w.towns = {{"Verda", {-87, 0, -42}, -1}, {"Roka", {-1750, 0, 450}, -1}, {"Espera", {0, 0, -1800}, -1}};
    w.ranches = {{"Ranĉo Sunleviĝo", {700, 0, -240}, 0}, {"Ranĉo Kanoj", {-20, 0, 1430}, 1}};
    return w;
}
// Drive there in `rig` and sit still until the event comes (or give up).
TruckEvent sit(Trucking& t, Vector3 at, const std::string& rig, double& now, float seconds = 10) {
    TruckFrame f;
    f.dt = .1F; f.player = at; f.driving = rig; f.speed = 0;
    for (float s = 0; s < seconds; s += f.dt) {
        now += f.dt; f.now = now;
        const auto e = t.update(f);
        if (e != TruckEvent::None && e != TruckEvent::CallCattle) return e;
    }
    return TruckEvent::None;
}
}

int main() {
    const auto world = island();
    double now = 0;
    {
        Trucking t;
        check(t.talk(true, now, world).find("officer") != std::string::npos && !t.hired(), "the boss won't hire a serving officer");
        t.talk(true, now + 1, world);
        check(!t.hired(), "...not even asked twice");
    }
    // Every big rig on the island counts (they all wear the company's colours): the yard's rigs
    // for hire, the company drivers', the supply rig; the stock trailers for cattle.
    // (The game's ids, as VehicleSystem makes them from the depot's markers: all seven stand in one row.)
    for (const char* box : {"vehicle_marker_capital_verda_vehicle_spawn_semi_depot_2", "vehicle_marker_capital_verda_vehicle_spawn_semi_haul_1",
                            "vehicle_marker_capital_verda_vehicle_spawn_semi_haul_3", "vehicle_marker_ranch_bovbazaro_vehicle_spawn_semi_freight_1"})
        check(Trucking::company_rig(box) && !Trucking::stock_rig(box), std::string("a box-trailer rig: ") + box);
    for (const char* stock : {"vehicle_marker_capital_verda_vehicle_spawn_semi_depot_cattle_1", "vehicle_marker_ranch_bovbazaro_vehicle_spawn_semi_cattle_1"})
        check(Trucking::stock_rig(stock), std::string("a stock-trailer rig: ") + stock);
    check(!Trucking::company_rig("vehicle_spawn_truck_capital") && !Trucking::company_rig("vehicle_spawn_hatchback_capital"), "a truck or a car isn't a rig");
    // Diesel: on the company's account it fills free; otherwise it costs tank_price a tank, in
    // whole kronoj, and stops when the cash runs out.
    {
        using outland::game::freight::Diesel;
        float fuel = 0, owed = 0;
        std::int64_t money = 1000;
        for (int i = 0; i < 100; ++i) Diesel::fill(fuel, .1F, true, money, owed);
        check(fuel > .999F && money == 1000, "on the company's account: full, and free");
        fuel = 0;
        std::int64_t paid = 0;
        for (int i = 0; i < 100; ++i) paid += Diesel::fill(fuel, .1F, false, money, owed);
        check(fuel > .999F && paid >= Diesel::tank_price - 1 && paid <= Diesel::tank_price && money == 1000 - paid, "paying: a tank costs tank_price");
        fuel = 0; money = 75; owed = 0;
        for (int i = 0; i < 100; ++i) Diesel::fill(fuel, .1F, false, money, owed);
        check(money == 0 && std::abs(fuel - .25F) < .01F, "the cash runs out: a quarter tank");
        check(Diesel::fill(fuel, .1F, false, money, owed) == 0, "...and no more");
        // Long-range tanks hold more: slower to fill, dearer to fill.
        fuel = 0; money = 10000; owed = 0;
        std::int64_t big = 0;
        for (int i = 0; i < 100; ++i) big += Diesel::fill(fuel, .1F, false, money, owed, 2);
        check(fuel > .6F && fuel < .7F, "double tanks: 10 s fills five eighths");
        for (int i = 0; i < 100; ++i) big += Diesel::fill(fuel, .1F, false, money, owed, 2);
        check(fuel > .999F && big >= 2 * Diesel::tank_price - 1 && big <= 2 * Diesel::tank_price, "double tanks cost double");
    }
    Trucking t;
    t.seed(7);
    t.talk(false, now, world);
    check(!t.hired(), "the first word is an offer");
    t.talk(false, now + 2, world);
    check(t.hired() && t.job().stage == HaulStage::Pickup && t.job().kind != HaulKind::None, "USE again: signed on with a first load");
    // Run loads until each kind has come up, and every one is done right through.
    int done[5]{}, paid_total = 0, trips = 0;
    for (int round = 0; round < 40; ++round) {
        const auto job = t.job();
        const bool cattle = job.kind == HaulKind::Cattle;
        const std::string rig = cattle ? "vehicle_spawn_semi_depot_cattle_1" : "vehicle_spawn_semi_depot_2";
        check(job.pay >= Trucking::base_pay, "a load pays at least the base");
        // The wrong rig (and on foot, and moving) loads nothing.
        check(sit(t, job.from, cattle ? "vehicle_spawn_semi_depot_2" : "vehicle_spawn_semi_depot_cattle_1", now) == TruckEvent::None, "the wrong rig loads nothing");
        check(sit(t, job.from, "", now) == TruckEvent::None, "on foot, nothing");
        check(sit(t, job.from, "vehicle_spawn_hatchback_capital", now) == TruckEvent::None, "a car isn't a rig");
        {
            TruckFrame f; f.dt = .1F; f.player = job.from; f.driving = rig; f.speed = 8;
            for (int i = 0; i < 100; ++i) check(t.update(f) == TruckEvent::None, "rolling past doesn't load");
        }
        if (cattle) {
            check(sit(t, job.from, rig, now) == TruckEvent::LoadCattle, "at the ranch's chute: the cattle are loaded");
            t.cattle_loaded(0);
            check(t.job().stage == HaulStage::Pickup, "none in the corral yet: wait");
            check(sit(t, job.from, rig, now) == TruckEvent::LoadCattle, "...and try again");
            t.cattle_loaded(4);
            check(t.job().stage == HaulStage::Deliver && t.job().head == 4, "four head aboard");
        } else {
            check(sit(t, job.from, rig, now) == TruckEvent::Loaded && t.job().stage == HaulStage::Deliver, "loaded at the pickup");
        }
        check(sit(t, Vector3{job.from.x + 300, 0, job.from.z}, rig, now) == TruckEvent::None, "nothing happens on the road");
        const auto drop = sit(t, job.to, rig, now);
        check(drop == (cattle ? TruckEvent::ShipCattle : TruckEvent::Delivered) && t.job().stage == HaulStage::Report, "unloaded at the drop");
        check(t.target().x == world.depot.x && t.target().z == world.depot.z, "then back to the depot");
        const int owed = t.job().pay;
        if (cattle) check(owed >= Trucking::base_pay + 4 * Trucking::head_pay, "cattle pay by the head too");
        check(t.take_pay() == 0, "nothing paid until the boss pays");
        t.talk(false, now += 30, world);
        const int paid = t.take_pay();
        check(paid == owed, "the boss pays for the trip");
        paid_total += paid;
        ++trips;
        ++done[static_cast<int>(job.kind)];
        check(t.job().stage == HaulStage::Pickup, "and has the next load ready");
    }
    check(done[1] > 0 && done[2] > 0 && done[3] > 0 && done[4] > 0, "imports, exports, deliveries and cattle hauls all come up");
    check(t.trips() == trips && t.earned() == paid_total, "trips and earnings counted");
    // Giving up a load: USE twice.
    t.talk(false, now += 30, world);
    t.talk(false, now += 1, world);
    check(t.job().stage == HaulStage::None, "a load given up");
    t.talk(false, now += 30, world);
    check(t.job().stage == HaulStage::Pickup, "...and a new one when asked");
    // Saved and restored.
    Trucking back;
    check(back.parse(t.serialise()) && back.hired() && back.trips() == t.trips() && back.earned() == t.earned(), "saved and restored");
    // Without the cargo terminal: deliveries only.
    {
        auto plain = island();
        plain.terminal = false;
        Trucking d;
        d.talk(false, 0, plain);
        d.talk(false, 1, plain);
        check(d.job().kind == HaulKind::Delivery, "without the terminal, deliveries from the depot");
    }
    if (failures) { std::cerr << failures << " failures\n"; return 1; }
    std::cout << trips << " trips, " << paid_total << " kr - trucking ok\n";
    return 0;
}
