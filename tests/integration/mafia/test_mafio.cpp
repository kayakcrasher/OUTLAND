// La Mafio: a crew of the county's gang in every town's taken-over building; they leave the
// player alone until shot at, then the whole gang shoots back; the dead respawn at the hideout;
// hit squads drive onto a rival gang's turf and fight it out.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/game/career/Backup.hpp"
#include "outland/game/career/Crew.hpp"
#include "outland/game/mafia/Mafio.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>

using namespace outland;
using game::mafia::Mafio;

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}

int main() {
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    world::physics::MeshCollisionLibrary::preload(region);
    std::string error;
    game::vehicles::VehicleRegistry types;
    check(types.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error), error);
    game::vehicles::VehicleSystem cars(types);
    cars.reconcile(region);
    game::life::LifeSimulation life;
    life.build(region, nullptr);

    game::ai::BattleRoyaleBots bots;
    game::ai::BotDrivers drivers;
    Mafio mafio;
    const int crews = mafio.start(life.places(), region, bots, 11);
    std::cout << crews << " crews, " << bots.bots().size() << " Krimuloj in " << region.settlements().size() << " towns\n";
    const auto towns = std::count_if(region.settlements().begin(), region.settlements().end(), [](const auto& t) { return !t.id.starts_with("ranch_"); });
    check(crews == static_cast<int>(towns), "a crew in every town (not on the Vakeroj's ranches)");
    std::set<int> gangs;
    for (const auto& crew : mafio.crews()) {
        gangs.insert(crew.gang);
        std::cout << "  " << crew.town << ": " << game::mafia::gangs[static_cast<std::size_t>(crew.gang)].meaning << " in "
                  << crew.building << " (" << game::life::place_name(crew.kind) << "), " << crew.members.size() << " members, door "
                  << crew.door.x << "," << crew.door.z << " inside " << crew.interior.x << "," << crew.interior.z << '\n';
        check(crew.gang == game::mafia::gang_of_zone(world::zones::zone_at(region.settlements()[static_cast<std::size_t>(crew.settlement)].center)),
              "each town's crew is its county's gang");
        check(crew.members.size() >= 3, "a crew of three or more");
        // Well away from the police: no hideout on a station's doorstep.
        float police = 1e9F;
        for (const auto& place : life.places())
            if (place.kind == game::life::PlaceKind::Police) police = std::min(police, std::hypot(place.door.x - crew.door.x, place.door.z - crew.door.z));
        std::cout << "    " << static_cast<int>(police) << " m from the nearest police station\n";
        check(police > 100, "the " + crew.town + " hideout is well away from the police station");
        for (const int id : crew.members) {
            const auto* bot = bots.find(id);
            check(bot && bot->alive() && Vector3Distance(bot->position(), crew.interior) < 30, "members stand at the hideout");
            check(mafio.gang_of(id) == crew.gang, "members know their gang");
        }
    }
    check(gangs.size() == 5, "five gangs, one for each county");
    const auto* casino = mafio.casino();
    check(casino && casino->town == "Verda" && casino->gang == 0 && casino->members.size() == 8, "the Green Hand's casino crew in Verda City");
    check(bots.find(casino->members.front())->level() == game::ai::BotLevel::Hard, "Verda City's gang is the hardest");
    check(game::mafia::gangs[3].level == game::ai::BotLevel::Easy && game::mafia::gangs[1].level == game::ai::BotLevel::Medium, "the counties are softer");

    // Walking on the real island; clear sight and bullets that hit bodies (no wall traces here).
    game::ai::BotWorld world;
    world.environment.line_of_sight = [](Vector3, Vector3) { return true; };
    world.environment.move = [&](Vector3 from, Vector3 to) {
        const float terrain = world::terrain::TerrainHeight::sample(to.x, to.z);
        const float probe = std::max(from.y, terrain) + .6F;
        Vector3 out{to.x, world::physics::WorldCollision::ground_height({to.x, probe, to.z}, probe, region), to.z};
        if (world::physics::WorldCollision::body_blocked(out, out.y, region, .3F)) return from;
        return out;
    };

    mafio.set_next_war(1e9); // no wars of their own until the hit squad below
    // The player walks up to the Verda City crew: nobody shoots.
    const Vector3 player = Vector3Add(casino->door, Vector3Scale(Vector3Normalize(Vector3Subtract(casino->door, casino->interior)), 7));
    double now = 0;
    float hurt = 0;
    const auto step = [&](float seconds, bool fired, Vector3 aim) {
        for (float t = 0; t < seconds; t += .05F) {
            game::mafia::MafioFrame frame;
            frame.dt = .05F; frame.now = now; frame.player_feet = player; frame.player_alive = true;
            frame.player_fired = fired && t < .06F;
            frame.aim_origin = {player.x, player.y + 1.5F, player.z};
            frame.aim_direction = Vector3Subtract(aim, frame.aim_origin);
            mafio.update(frame, bots, world, drivers, cars, region);
            cars.update(.05F, {}, region, player, false);
            hurt += bots.events().player_damage;
            now += .05;
        }
    };
    step(20, false, {});
    check(hurt == 0, "the Krimuloj leave a peaceful player alone");
    check(!mafio.hostile(0, casino->members.front(), now), "not hostile");
    // Far crews sleep.
    int asleep = 0;
    for (const auto& m : mafio.members()) asleep += bots.asleep(m.bot) ? 1 : 0;
    check(asleep > 0 && !bots.asleep(casino->members.front()), "crews far away sleep, the near one is awake");

    // The player fires at a door guard: the whole gang shoots back.
    const auto* guard = bots.find(casino->members.front());
    step(.1F, true, {guard->position().x, guard->position().y + 1.2F, guard->position().z});
    check(mafio.provoked(0, now) && mafio.hostile(0, casino->members[3], now), "shooting at one turns the gang");
    check(!mafio.provoked(2, now), "only that gang");
    step(15, false, {});
    std::cout << "self defence: the player took " << hurt << " damage in 15 s\n";
    check(hurt > 30, "they shoot back");
    // Gangs never fight themselves; rivals only at war.
    check(!mafio.hostile(casino->members[0], casino->members[1], now), "same gang, never");
    int rival = -1;
    for (const auto& m : mafio.members()) if (mafio.gang_of(m.bot) != 0) { rival = m.bot; break; }
    check(!mafio.hostile(casino->members[0], rival, now), "rivals at peace");
    mafio.forgive();
    check(!mafio.provoked(0, now), "the player dies: forgiven");

    // A Krimulo killed comes back at the hideout later.
    int victim = -1;
    for (const int id : casino->members) if (bots.find(id)->alive()) victim = id;
    check(victim > 0, "the crew stands (their rounds pass their own)");
    for (const int id : casino->members) check(bots.find(id)->alive(), "no Krimulo shot by his own crew");
    check(bots.damage_bot(victim, 500, player, now), "killed");
    step(1, false, {});
    check(!bots.find(victim)->alive(), "dead for now");
    step(Mafio::respawn_seconds + 2, false, {});
    const auto* back = bots.find(victim);
    if (!back->alive()) step(95, false, {});
    check(bots.find(victim)->alive() && bots.find(victim)->health() >= 99, "respawned");
    check(Vector3Distance(bots.find(victim)->position(), mafio.members()[static_cast<std::size_t>(victim - 1)].post) < 6, "at the hideout");
    // Under siege (the Policio's CLEAN UP): the dead stay down until it is over.
    int casino_crew = -1;
    for (std::size_t c = 0; c < mafio.crews().size(); ++c) if (mafio.crews()[c].casino) casino_crew = static_cast<int>(c);
    game::mafia::MafioFrame siege;
    siege.now = now; siege.player_feet = player;
    mafio.besiege(casino_crew, bots, siege);
    check(mafio.besieged() == casino_crew && bots.damage_bot(victim, 500, player, now), "besieged, and one down");
    step(Mafio::respawn_seconds + 100, false, {});
    check(!bots.find(victim)->alive(), "under siege, the dead stay down");
    mafio.besiege(-1, bots, siege);
    step(Mafio::respawn_seconds + 100, false, {});
    check(bots.find(victim)->alive(), "the siege lifted, they come back");

    // An officer's backup: two officers, out of sight until called; they come and stand by the
    // player, fight a gang at war with the officer (not one at peace), and go home.
    game::career::Backup backup;
    backup.start(bots, casino->door, 77);
    bool off_duty = backup.ids().size() == 2;
    for (const int id : backup.ids()) off_duty = off_duty && !bots.find(id)->alive() && bots.hidden(id);
    check(off_duty, "two officers, off duty and out of sight");
    mafio.set_outsiders([&](int a, int b, double at) {
        if (!backup.officer(a) && !backup.officer(b)) return false;
        const int gang = mafio.gang_of(backup.officer(a) ? b : a);
        return gang >= 0 && mafio.provoked(gang, at);
    });
    Vector3 station{};
    for (const auto& town : region.settlements())
        for (const auto& marker : town.gameplay_markers)
            if (marker.id.find("vehicle_spawn_police_capital_verda_1") != std::string::npos) station = marker.position;
    const std::string said = backup.call(player, now, station, 0, bots, drivers, cars, region);
    std::cout << "backup: " << said << '\n';
    bool called = backup.active();
    for (const int id : backup.ids()) called = called && bots.find(id)->alive() && !bots.hidden(id);
    check(called, "the call is answered: two officers on their way");
    check(backup.call(player, now, station, 0, bots, drivers, cars, region).find("way") != std::string::npos ||
          backup.phase() == game::career::BackupPhase::OnScene, "one squad at a time");
    for (float t = 0; t < 200 && backup.phase() != game::career::BackupPhase::OnScene; t += .05F) {
        game::mafia::MafioFrame frame;
        frame.dt = .05F; frame.now = now; frame.player_feet = player; frame.player_alive = true;
        mafio.update(frame, bots, world, drivers, cars, region);
        cars.update(.05F, {}, region, player, false);
        backup.update(player, now, true, false, bots, drivers, cars, region);
        now += .05;
    }
    for (float t = 0; t < 10; t += .05F) {   // a moment to walk over
        game::mafia::MafioFrame frame;
        frame.dt = .05F; frame.now = now; frame.player_feet = player; frame.player_alive = true;
        mafio.update(frame, bots, world, drivers, cars, region);
        backup.update(player, now, true, false, bots, drivers, cars, region);
        now += .05;
    }
    const auto* officer = bots.find(backup.ids().front());
    std::cout << "backup on the scene " << (backup.phase() == game::career::BackupPhase::OnScene) << ", "
              << Vector3Distance(officer->position(), player) << " m from the player\n";
    check(backup.phase() == game::career::BackupPhase::OnScene && Vector3Distance(officer->position(), player) < 45, "backup arrives at the player's side");
    check(!mafio.hostile(officer->id(), casino->members.front(), now), "a gang at peace is left alone");
    mafio.provoke(casino->members.front(), now);
    check(mafio.hostile(officer->id(), casino->members.front(), now), "a gang at war with the officer is fought");
    mafio.forgive();
    backup.dismiss(bots, now);
    check(!backup.active() && bots.hidden(officer->id()) && backup.call(player, now, station, 0, bots, drivers, cars, region).find("no units") != std::string::npos,
          "home again, and dispatch needs a while before the next");

    // The player's own crew: a gang member recruits his gang's foot soldiers (never the boss), up
    // to four; they follow the player, fight whoever turns on the player, and one of them drives
    // the player across the island. An officer gets a partner instead.
    {
        const int own = casino->gang;
        mafio.set_player_gang(own);
        game::career::Crew crew;
        crew.start(bots, casino->door, 91);
        check(!mafio.recruitable(casino->members.front(), bots), "the boss can't be recruited");
        std::vector<int> soldiers;
        for (const auto& c : mafio.crews()) {
            if (c.gang != own) continue;
            for (const int id : c.members) if (mafio.recruitable(id, bots)) soldiers.push_back(id);
        }
        check(soldiers.size() >= 4, "foot soldiers to recruit: " + std::to_string(soldiers.size()));
        int joined = 0;
        for (const int id : soldiers) if (crew.recruit(id)) { mafio.lend(id, true, bots, player); ++joined; }
        check(joined == game::career::Crew::max_gang && crew.size() == 4 && !crew.recruit(casino->members.back()), "four join the crew, no more");
        check(!mafio.recruitable(crew.members().front(), bots), "a recruit is taken");
        // He follows: the player walks off; the crew keeps up (catching up when left far behind).
        const Vector3 walk = Vector3Add(casino->door, {0, 0, 160});
        std::string said;
        for (float t = 0; t < 8; t += .05F) {
            game::mafia::MafioFrame frame;
            frame.dt = .05F; frame.now = now; frame.player_feet = walk; frame.player_alive = true;
            mafio.update(frame, bots, world, drivers, cars, region);
            game::career::Crew::Frame cf; cf.player = walk; cf.alive = true;
            crew.update(cf, bots, region, said);
            now += .05;
        }
        float worst = 0;
        for (const int id : crew.members()) worst = std::max(worst, std::hypot(bots.find(id)->position().x - walk.x, bots.find(id)->position().z - walk.z));
        std::cout << "crew: furthest " << worst << " m from the player\n";
        check(worst < 40, "the crew keeps up with the player");
        // A rival gang turns on the player: the crew fights them; the player's own gang never.
        int rival = -1;
        for (const auto& c : mafio.crews()) if (c.gang != own) { rival = c.members.front(); break; }
        check(!mafio.hostile(crew.members().front(), rival, now), "at peace with the rival gang");
        mafio.provoke(rival, now);
        check(mafio.hostile(crew.members().front(), rival, now), "the crew fights a gang that turned on the player");
        mafio.forgive();
        // The player at a car's passenger door: the nearest recruit drives to a goal far off.
        int car_index = -1;
        float nearest = 1e9F;
        for (std::size_t i = 0; i < cars.vehicles().size(); ++i) {
            const auto& v = cars.vehicles()[i];
            const auto* asset = cars.asset(region, v.id);
            if (!asset || asset->vehicle.definition != "hatchback" || drivers.driver_of(v.id) >= 0) continue;
            const float d = std::hypot(v.position.x - walk.x, v.position.z - walk.z);
            if (d < nearest) { nearest = d; car_index = static_cast<int>(i); }
        }
        check(car_index >= 0, "a car to ride in");
        if (car_index >= 0) {
            const auto car = cars.vehicles()[static_cast<std::size_t>(car_index)];
            for (const int id : crew.members()) bots.find(id)->set_position(Vector3Add(car.position, {3, 0, 0}));
            const Vector3 goal = Vector3Add(car.position, {0, 0, -400});
            const int driver = crew.ride(car.id, car.position, goal, bots, drivers, cars, region);
            check(driver >= 0 && crew.riding(drivers), "a recruit takes the wheel");
            const Vector3 start = car.position;
            for (float t = 0; t < 120 && crew.riding(drivers); t += .05F) {
                const auto here = cars.vehicles()[static_cast<std::size_t>(cars.find(car.id))].position;
                game::mafia::MafioFrame frame;
                frame.dt = .05F; frame.now = now; frame.player_feet = here; frame.player_alive = true; frame.player_in_vehicle = true;
                mafio.update(frame, bots, world, drivers, cars, region);
                cars.update(.05F, {}, region, here, false);
                game::career::Crew::Frame cf; cf.player = here; cf.alive = true; cf.riding = true;
                crew.update(cf, bots, region, said);
                now += .05;
            }
            const auto end = cars.vehicles()[static_cast<std::size_t>(cars.find(car.id))].position;
            std::cout << "ride: " << std::hypot(end.x - start.x, end.z - start.z) << " m driven, " << std::hypot(end.x - goal.x, end.z - goal.z)
                      << " m from the goal, still riding " << crew.riding(drivers) << '\n';
            check(std::hypot(end.x - start.x, end.z - start.z) > 100, "the crew drives the player off");
            crew.end_ride(bots, drivers, cars, region);
            check(!crew.riding(drivers) && crew.ride_car().empty(), "out of the car");
        }
        // Out of the gang: everyone goes home.
        for (const int id : crew.dismiss(bots)) mafio.lend(id, false, bots, player);
        check(crew.size() == 0 && mafio.recruitable(soldiers.front(), bots), "dismissed, back with the gang");
        mafio.set_player_gang(-1);
        // An officer's partner from the desk.
        check(crew.assign_partner(Vector3Add(casino->door, {2, 0, 0}), 0, bots, now) && crew.partner() >= 0 && bots.find(crew.partner())->alive(),
              "the desk gives the officer a partner");
        check(!crew.assign_partner(casino->door, 0, bots, now), "one partner");
        crew.dismiss_partner(bots);
        check(crew.partner() < 0 && bots.hidden(crew.partner_bot()), "the partner goes off duty with the badge");
    }

    // A hit squad: a rival crew drives onto another gang's turf and they fight.
    bool sent = false;
    int target = -1;
    for (std::size_t t = 0; t < mafio.crews().size() && !sent; ++t)
        for (std::size_t f = 0; f < mafio.crews().size() && !sent; ++f) {
            if (mafio.crews()[f].gang == mafio.crews()[t].gang) continue;
            if (Vector3Distance(mafio.crews()[f].door, mafio.crews()[t].door) > 1500) continue;
            if (mafio.send_hit_squad(static_cast<int>(f), static_cast<int>(t), bots, drivers, cars, region, now)) {
                sent = true; target = static_cast<int>(t);
                std::cout << "hit squad: " << mafio.crews()[f].town << " -> " << mafio.crews()[t].town << '\n';
            }
        }
    check(sent && mafio.wars().size() == 1, "a hit squad sets off by car");
    const auto war = mafio.wars().front();
    check(war.squad.size() >= 2 && bots.mounted(war.squad[0]) && bots.mounted(war.squad[1]), "the squad rides together");
    check(mafio.hostile(war.squad[0], mafio.crews()[static_cast<std::size_t>(target)].members[0], now), "at war with the target gang");
    // Watch from beside the target's hideout until the squad is there and the shooting starts.
    const Vector3 watch = Vector3Add(mafio.crews()[static_cast<std::size_t>(target)].door, {0, 0, 0});
    int shots = 0;
    bool arrived = false;
    for (float t = 0; t < 240 && !(arrived && shots > 10); t += .05F) {
        game::mafia::MafioFrame frame;
        frame.dt = .05F; frame.now = now; frame.player_feet = {watch.x + 40, watch.y, watch.z + 40}; frame.player_alive = true;
        mafio.update(frame, bots, world, drivers, cars, region);
        cars.update(.05F, {}, region, frame.player_feet, false);
        shots += bots.events().shots;
        if (std::getenv("MAFIO_TRACE") && static_cast<int>(t * 20) % 40 == 0)
            for (const auto& ride : drivers.rides())
                if (ride.targeted) {
                    const auto& c = cars.vehicles()[static_cast<std::size_t>(cars.find(ride.vehicle))];
                    std::cout << "t " << t << " " << ride.vehicle << " active " << ride.active << " at " << c.position.x << "," << c.position.z
                              << " speed " << c.speed << " progress " << ride.progress << "/" << ride.route.length << " attempts " << ride.attempts << '\n';
                }
        if (!mafio.wars().empty()) arrived = mafio.wars().front().arrived;
        now += .05;
    }
    std::cout << "the squad arrived: " << arrived << ", shots fired: " << shots << '\n';
    for (const int id : war.squad) {
        const auto* bot = bots.find(id);
        std::cout << "  squad member " << id << " at " << bot->position().x << "," << bot->position().z << " alive " << bot->alive()
                  << " " << game::ai::bot_intent_name(bot->intent()) << ", " << Vector3Distance(bot->position(), watch) << " m from the target\n";
    }
    check(arrived, "the squad gets there by road");
    check(shots > 10, "and they fight it out");
    std::cout << "mafio tests passed\n";
    return 0;
}
