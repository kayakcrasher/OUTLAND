// La Mafio: a crew of the county's gang in every town's taken-over building; they leave the
// player alone until shot at, then the whole gang shoots back; the dead respawn at the hideout;
// hit squads drive onto a rival gang's turf and fight it out.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/life/LifeSimulation.hpp"
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
    check(crews == static_cast<int>(towns), "a crew in every town (not on the Vakeros' ranches)");
    std::set<int> gangs;
    for (const auto& crew : mafio.crews()) {
        gangs.insert(crew.gang);
        std::cout << "  " << crew.town << ": " << game::mafia::gangs[static_cast<std::size_t>(crew.gang)].meaning << " in "
                  << crew.building << " (" << game::life::place_name(crew.kind) << "), " << crew.members.size() << " members, door "
                  << crew.door.x << "," << crew.door.z << " inside " << crew.interior.x << "," << crew.interior.z << '\n';
        check(crew.gang == game::mafia::gang_of_zone(world::zones::zone_at(region.settlements()[static_cast<std::size_t>(crew.settlement)].center)),
              "each town's crew is its county's gang");
        check(crew.members.size() >= 3, "a crew of three or more");
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
