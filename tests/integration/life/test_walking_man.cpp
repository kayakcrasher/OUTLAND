// Paul Acosta, "the walking man": his trailer and car at Ruldomaro, the car left outside Verda
// City's limits, poker at the casino, the bar, home by car or stumbling drunk on foot; a pistol
// drawn only in self defence; back home the morning after he dies.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/game/life/WalkingMan.hpp"
#include "outland/game/mafia/Mafio.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/Zones.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/roads/RoadGraph.hpp"
#include <raymath.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>

using namespace outland;
using game::life::PaulDoing;
using game::life::WalkingMan;

namespace {
int failures = 0;
void check(bool condition, const std::string& message) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
}

int main() {
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    if (!creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map")) { std::cerr << "no map\n"; return 1; }
    world::physics::MeshCollisionLibrary::preload(region);
    std::string error;
    game::vehicles::VehicleRegistry types;
    if (!types.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error)) { std::cerr << error << '\n'; return 1; }
    game::vehicles::VehicleSystem cars(types);
    cars.reconcile(region);
    game::life::LifeSimulation life;
    life.build(region, nullptr);

    game::ai::BattleRoyaleBots bots;
    game::ai::BotDrivers drivers;
    drivers.reserve(std::string(WalkingMan::car_marker));
    game::mafia::Mafio mafio;
    mafio.start(life.places(), region, bots, 11);
    const auto* casino = mafio.casino();
    if (!casino) { std::cerr << "no casino\n"; return 1; }

    WalkingMan paul;
    auto clock = game::life::WorldClock::at(1, 8, 0);
    check(paul.start(life.places(), region, casino->door, casino->table, bots, cars, 7, clock), "Paul Acosta moves in");
    mafio.set_outsiders([&](int a, int b, double now) { return paul.hostile_to(a, b, now); });
    const auto& plan = paul.plan();
    std::cout << "home " << plan.home.x << "," << plan.home.z << "  car " << plan.car << " at " << plan.car_home.x << "," << plan.car_home.z
              << "\nparks at " << plan.parking.x << "," << plan.parking.z << "  casino seat " << plan.table.x << "," << plan.table.z
              << "  bar " << plan.bar.x << "," << plan.bar.z << '\n';

    // Where things are.
    check(!plan.car.empty() && cars.find(plan.car) >= 0, "his car is on the island");
    check(flat(plan.home, plan.car_home) < 30, "his trailer is by his car");
    check(world::zones::zone_at(plan.home) != world::zones::ZoneId::Verdkerno, "he lives out of town");
    check(world::zones::zone_at(plan.parking) != world::zones::ZoneId::Verdkerno, "he parks outside the city limits");
    check(flat(plan.parking, plan.casino_door) > 250 && flat(plan.parking, plan.car_home) > 200, "...between town and home, a walk from the casino");
    world::roads::RoadGraph roads(region);
    check(roads.nearest(plan.parking).distance < 14, "on the shoulder of a road");
    check(flat(plan.table, casino->table) < 2, "his seat is at the poker table");
    check(flat(plan.bar_door, casino->door) > 8, "the bar is another place");
    check(bots.find(paul.bot()) && bots.find(paul.bot())->weapon() == game::combat::WeaponId::Pistol, "he carries a pistol");
    check(paul.doing() == PaulDoing::Home && flat(paul.position(bots), plan.home) < 3, "at 8 he is home");

    // Calm: nobody's enemy, and not the gangs' either.
    check(!mafio.hostile(paul.bot(), 0, 0) && !mafio.hostile(0, paul.bot(), 0), "he leaves the player alone");
    check(!mafio.hostile(paul.bot(), casino->members.front(), 0), "and the Green Hand");
    if (drivers.dispatch(bots, cars, region, {casino->members.front()}, plan.car_home, plan.parking, 40))
        check(drivers.ride_of(casino->members.front())->vehicle != plan.car, "nobody else takes his car");
    drivers.clear(bots, cars);
    bots.find(casino->members.front())->set_position(casino->door);
    cars.place(region, plan.car, plan.car_home, plan.car_home_yaw);

    // A day with nobody watching (the player far away): one frame is 0.5 s, the island's clock 30x.
    game::life::WalkingManFrame frame;
    frame.player_feet = {5000, 0, 5000};
    double now = 0;
    std::set<PaulDoing> seen;
    std::vector<PaulDoing> order;
    const auto run = [&](float hours, const std::function<void()>& each = {}) {
        const int frames = static_cast<int>(hours * 3600 / 30 / .5F);
        for (int i = 0; i < frames; ++i) {
            frame.dt = .5F;
            now += .5;
            clock.advance(.5F, 30);
            frame.now = now;
            frame.clock = clock;
            paul.update(frame, bots, drivers, cars, region);
            if (order.empty() || order.back() != paul.doing()) order.push_back(paul.doing());
            seen.insert(paul.doing());
            if (each) each();
        }
    };
    // Find the next drunk and sober nights (seeded per day).
    int drunk_day = -1, sober_day = -1;
    for (int d = 1; d < 30; ++d) {
        if (paul.drunk_on(d) && drunk_day < 0) drunk_day = d;
        if (!paul.drunk_on(d) && sober_day < 0) sober_day = d;
    }
    std::cout << "drunk on day " << drunk_day << ", sober on day " << sober_day << '\n';
    check(drunk_day > 0 && sober_day > 0, "some nights drunk, some not");

    run(4); // to noon
    check(paul.doing() == PaulDoing::Gambling && flat(paul.position(bots), plan.table) < 1.5F, "by noon he is at the poker table (" + std::string(game::life::paul_doing_name(paul.doing())) + ")");
    check(flat(cars.vehicles()[static_cast<std::size_t>(cars.find(plan.car))].position, plan.parking) < 4, "his car waits outside the city limits");
    check(seen.contains(PaulDoing::WalkingIn), "he walked in from the car");
    run(6.5F); // 18:30
    check(paul.doing() == PaulDoing::Drinking && flat(paul.position(bots), plan.bar) < 1.5F, "in the evening he is at the bar");
    check(seen.contains(PaulDoing::ToBar), "he walked to the bar");

    // The night: day 1's choice.
    const bool drunk_tonight = paul.drunk_on(1);
    std::vector<Vector3> trail;
    float off_line = 0;
    run(13.25F, [&] {
        if (paul.doing() == PaulDoing::Stumbling) trail.push_back(paul.position(bots));
    });
    std::cout << "day 1 (" << (drunk_tonight ? "drunk" : "sober") << "):";
    for (const auto d : order) std::cout << ' ' << game::life::paul_doing_name(d) << ',';
    std::cout << '\n';
    if (drunk_tonight) {
        check(trail.size() > 100, "drunk, he stumbles home on foot");
        // Slow, weaving: compare the walk with straight steps.
        float walked = 0;
        for (std::size_t i = 1; i < trail.size(); ++i) walked += flat(trail[i - 1], trail[i]);
        const float seconds = static_cast<float>(trail.size()) * .5F;
        check(walked / seconds < WalkingMan::stumble_speed * 1.15F, "slowly");
        for (std::size_t i = 10; i + 10 < trail.size(); i += 10) {
            const Vector3 a = trail[i - 10], b = trail[i + 10], p = trail[i];
            const Vector3 d = Vector3Normalize({b.x - a.x, 0, b.z - a.z});
            off_line = std::max(off_line, std::abs((p.x - a.x) * -d.z + (p.z - a.z) * d.x));
        }
        check(off_line > .4F, "weaving");
        check(flat(cars.vehicles()[static_cast<std::size_t>(cars.find(plan.car))].position, plan.parking) < 4, "the car stays out overnight");
    } else {
        check(seen.contains(PaulDoing::ToCar), "sober, he walks to his car and drives home");
        check(flat(cars.vehicles()[static_cast<std::size_t>(cars.find(plan.car))].position, plan.car_home) < 4, "the car is home");
    }
    check(paul.doing() == PaulDoing::Home || paul.doing() == PaulDoing::Stumbling, "home by morning");

    // The player sleeps the day away: he is where the clock says.
    clock = game::life::WorldClock::at(clock.day(), 19, 0);
    run(.1F);
    check(paul.doing() == PaulDoing::Drinking && flat(paul.position(bots), plan.bar) < 1.5F, "after a jump in time he is at the bar");

    // Self defence. A gun held on him close up: a warning, then he draws.
    const Vector3 at = paul.position(bots);
    Vector3 away = Vector3Subtract(plan.bar_door, plan.bar);
    away.y = 0;
    away = Vector3Normalize(away);
    frame.player_feet = Vector3Add(at, Vector3Scale(away, 7));
    frame.aim_origin = {frame.player_feet.x, at.y + 1.5F, frame.player_feet.z};
    frame.aim_direction = Vector3Normalize(Vector3Subtract({at.x, at.y + 1.2F, at.z}, frame.aim_origin));
    frame.player_aiming = true;
    run(1.2F * 30 / 3600); // 1.2 real seconds
    check(!paul.angry(now) && paul.message().find("easy") != std::string::npos, "he warns you first");
    run(2.0F * 30 / 3600);
    check(paul.angry(now) && mafio.hostile(paul.bot(), 0, now), "keep aiming and he draws on you");
    check(paul.doing() == PaulDoing::Defending && !bots.asleep(paul.bot()), "his combat brain takes over");
    check(!mafio.hostile(paul.bot(), casino->members.front(), now), "his quarrel is with you alone");
    frame.player_aiming = false;
    frame.player_feet = {5000, 0, 5000};
    run((WalkingMan::angry_seconds + 2) * 30 / 3600);
    check(!paul.angry(now) && paul.doing() != PaulDoing::Defending && bots.asleep(paul.bot()), "it blows over");
    // Shooting near him is as good as hitting him.
    frame.player_feet = Vector3Add(paul.position(bots), {20, 0, 0});
    frame.aim_origin = Vector3Add(frame.player_feet, {0, 1.5F, 0});
    frame.aim_direction = Vector3Normalize(Vector3Subtract(Vector3Add(paul.position(bots), {0, 2.5F, 1}), frame.aim_origin));
    frame.player_fired = true;
    run(.5F * 30 / 3600);
    frame.player_fired = false;
    check(paul.angry(now), "a shot past his head and he fights");
    // Hurt him: the game's hit calls provoke().
    run((WalkingMan::angry_seconds + 2) * 30 / 3600);
    paul.provoke(paul.bot(), now);
    check(paul.angry(now), "hurt him and he fights");

    // Killed: home again next morning.
    frame.player_feet = {5000, 0, 5000};
    bots.find(paul.bot())->set_health(0);
    run(.05F);
    check(paul.doing() == PaulDoing::Dead && !mafio.hostile(paul.bot(), 0, now), "dead");
    run(16);
    check(bots.find(paul.bot())->alive() && paul.doing() != PaulDoing::Dead, "back the next morning");
    std::cout << "after death: " << game::life::paul_doing_name(paul.doing()) << " at " << clock.label() << '\n';

    // A drunk night, followed on foot (the player walks a few metres behind, so he goes at his own
    // pace): slow, weaving, stopping now and then, all the way to the trailer.
    while (!paul.drunk_on(clock.day())) run(24);
    run(std::max(0.0F, 22.5F - clock.hour()));
    check(paul.doing() == PaulDoing::Stumbling, "a drunk night: he stumbles home (" + std::string(game::life::paul_doing_name(paul.doing())) + ")");
    check(flat(cars.vehicles()[static_cast<std::size_t>(cars.find(plan.car))].position, plan.parking) < 4, "and leaves the car outside town");
    trail.clear();
    int stops = 0;
    bool was_still = false;
    run(1.5F, [&] {
        const Vector3 p = paul.position(bots);
        frame.player_feet = Vector3Add(p, {6, 0, 0});
        if (paul.doing() != PaulDoing::Stumbling) return;
        const bool still = !trail.empty() && flat(trail.back(), p) < .01F;
        if (still && !was_still) ++stops;
        was_still = still;
        trail.push_back(p);
    });
    float walked = 0;
    off_line = 0;
    for (std::size_t i = 1; i < trail.size(); ++i) walked += flat(trail[i - 1], trail[i]);
    for (std::size_t i = 10; i + 10 < trail.size(); i += 5) {
        const Vector3 a = trail[i - 10], b = trail[i + 10], p = trail[i];
        if (flat(a, b) < 5) continue;
        const Vector3 d = Vector3Normalize({b.x - a.x, 0, b.z - a.z});
        off_line = std::max(off_line, std::abs((p.x - a.x) * -d.z + (p.z - a.z) * d.x));
    }
    std::cout << "drunk walk: " << walked << " m in " << trail.size() * .5F << " s, " << stops << " stops, weaving up to " << off_line << " m\n";
    check(trail.size() > 300 && walked / (static_cast<float>(trail.size()) * .5F) < WalkingMan::stumble_speed * 1.2F, "slowly");
    check(off_line > .5F, "weaving");
    check(stops >= 2, "stopping to get his bearings");
    frame.player_feet = {5000, 0, 5000};
    run(4);
    check(paul.doing() == PaulDoing::Home && flat(paul.position(bots), plan.home) < 2, "he makes it home (" + std::string(game::life::paul_doing_name(paul.doing())) + " at " + clock.label() + ")");

    if (failures) { std::cerr << failures << " failures\n"; return 1; }
    std::cout << "walking man ok\n";
    return 0;
}
