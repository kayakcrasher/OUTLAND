// The Vakeroj: five ranches with a family of four (hats, shotguns) and a herd each, a side-by-side
// running the farm roads; the cattle truck taking cattle from the corrals to the cattle quay (shipped, the ranches paid); the
// supply rig delivering round the towns; trailers that follow their tractors.
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/freight/Freight.hpp"
#include "outland/game/ranch/Ranches.hpp"
#include "outland/game/transit/BusLine.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>

using namespace outland;
using game::ranch::CowState;
namespace rr = world::ranches;

namespace {
int failures = 0;
void check(bool condition, const std::string& message) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
int alive(const game::ranch::RanchState& r) {
    return static_cast<int>(std::count_if(r.herd.begin(), r.herd.end(), [](const auto& c) { return c.state != CowState::Dead && c.state != CowState::Gone; }));
}
}

int main() {
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    if (!creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map")) { std::cerr << "no map\n"; return 1; }
    world::physics::MeshCollisionLibrary::preload(region);
    std::string error;
    game::vehicles::VehicleRegistry types;
    if (!types.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error)) { std::cerr << error << '\n'; return 1; }
    characters::CharacterRegistry people;
    if (!people.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/characters/character_manifest.tsv", error)) { std::cerr << error << '\n'; return 1; }
    game::vehicles::VehicleSystem cars(types);
    cars.reconcile(region);

    // The ranches: one in each county, level, off the towns, each with its track joined to a highway.
    int ranch_settlements = 0;
    std::set<world::zones::ZoneId> counties;
    for (const auto& spec : rr::ranches) {
        const rr::Frame f(spec.gate, spec.up_track);
        counties.insert(world::zones::zone_at(f.at(0, 64)));
        check(world::zones::zone_at(f.at(0, 64)) == spec.county, std::string(spec.name) + " is in its county");
        float lo = 1e9F, hi = -1e9F;
        for (float u = rr::footprint.u0; u <= rr::footprint.u1; u += 8)
            for (float v = rr::footprint.v0; v <= rr::footprint.v1; v += 8) {
                const Vector3 p = f.at(u, v);
                const float h = world::terrain::TerrainHeight::sample(p.x, p.z);
                lo = std::min(lo, h);
                hi = std::max(hi, h);
            }
        check(hi - lo < 1.5F, std::string(spec.name) + " stands on level ground (" + std::to_string(hi - lo) + " m)");
        for (const auto& town : region.settlements()) {
            if (town.id == spec.id) ++ranch_settlements;
            else if (!town.id.starts_with("ranch_")) check(flat(town.center, f.at(0, 64)) > 380, std::string(spec.name) + " is out of " + town.name);
        }
    }
    check(ranch_settlements == 5 && counties.size() == 5, "five ranches, one in each county");
    {
        const world::roads::RoadGraph graph(region);
        for (const auto& spec : rr::ranches) {
            const rr::Frame f(spec.gate, spec.up_track);
            const auto route = graph.route({0, 0, 0}, f.at(rr::yard));
            check(route.complete && route.length < 3000, std::string("a road from Verda City to ") + std::string(spec.name));
        }
    }
    int sxs = 0, semis = 0;
    for (const auto& car : cars.vehicles()) {
        sxs += car.id.find("vehicle_spawn_sxs_") != std::string::npos && car.id.find("_forsale_") == std::string::npos;   // (not the car lot's)
        semis += car.id.find("vehicle_spawn_semi_") != std::string::npos;
    }
    // The stockyard's two, and Verda Transporto's: four for hire and three of its own drivers'.
    check(sxs == 5 && semis == 9, "a side-by-side on every ranch, nine big rigs (" + std::to_string(semis) + ")");
    const auto* sxs_type = types.find("sxs");
    const auto* semi_type = types.find("semi");
    check(sxs_type && semi_type && sxs_type->length < 3.5F && semi_type->length > 7, "the side-by-side and the tractor are vehicle types");

    // The families and herds.
    game::ai::BattleRoyaleBots bots;
    game::ranch::Ranches ranches;
    auto clock = game::life::WorldClock::at(1, 9, 30);
    check(ranches.start(region, bots, cars, &people, 5, clock) == 5, "the game finds the five ranches");
    check(ranches.vakeros().size() == 20 && ranches.stockyard().found, "four Vakeroj on each, and the stockyard");
    check(ranches.cattle_quay().found, "the cattle quay at Porto Luma's cargo terminal");
    std::set<std::string> names;
    for (const auto& v : ranches.vakeros()) {
        const auto* bot = bots.find(v.bot);
        check(bot && bot->weapon() == game::combat::WeaponId::Shotgun, v.name + " carries a shotgun");
        check(!v.character.empty() && (v.female == (v.character.find("female") != std::string::npos)), v.name + " has a body");
        names.insert(v.name);
    }
    check(names.size() == 20, "every Vakero has a name");
    for (const auto& r : ranches.ranches()) {
        check(static_cast<int>(r.herd.size()) == game::ranch::Ranches::herd_size && !r.sxs.empty(), r.settlement + ": a herd and a side-by-side");
        for (const auto& cow : r.herd) check(r.frame.inside(cow.position, rr::pasture), r.settlement + ": cattle in the pasture");
    }

    // A morning at the east ranch, the player standing in the yard.
    const int east = 2;
    auto& ranch = const_cast<game::ranch::RanchState&>(ranches.ranches()[east]);
    game::ranch::RanchFrame frame;
    frame.player_feet = ranch.frame.at(rr::yard.x + 5, rr::yard.y - 10);
    frame.player_feet.y = world::terrain::TerrainHeight::sample(frame.player_feet.x, frame.player_feet.z);
    double now = 0;
    const auto run = [&](float seconds, const std::function<void()>& each = {}) {
        for (float t = 0; t < seconds; t += .1F) {
            frame.dt = .1F;
            now += .1;
            frame.now = now;
            clock.advance(.1F, 30);
            frame.clock = clock;
            ranches.update(frame, bots, cars, region);
            if (each) each();
        }
    };
    Vector3 sxs_start = cars.vehicles()[static_cast<std::size_t>(cars.find(ranch.sxs))].position;
    float sxs_travel = 0, worst_stray = 0;
    run(60, [&] {
        for (const auto& cow : ranch.herd)
            if (cow.state == CowState::Grazing || cow.state == CowState::Walking) {
                const auto l = ranch.frame.local(cow.position);
                worst_stray = std::max({worst_stray, rr::pasture.u0 - l.x, l.x - rr::pasture.u1, rr::pasture.v0 - l.y, l.y - rr::pasture.v1});
            }
        sxs_travel = std::max(sxs_travel, flat(cars.vehicles()[static_cast<std::size_t>(cars.find(ranch.sxs))].position, sxs_start));
    });
    check(worst_stray < .5F, "the herd stays in the pasture (" + std::to_string(worst_stray) + ")");
    check(ranch.sxs_running && sxs_travel > 40, "the son drives the side-by-side round the farm (" + std::to_string(sxs_travel) + " m)");
    const Vector3 at = cars.vehicles()[static_cast<std::size_t>(cars.find(ranch.sxs))].position;
    check(ranch.frame.inside(at, {rr::footprint.u0, rr::footprint.u1, 0, rr::footprint.v1 + 6}), "on the farm's own roads");
    for (const auto& k : ranch.family) {
        const auto& v = ranches.vakeros()[static_cast<std::size_t>(k)];
        check(!bots.asleep(v.bot), v.name + " is up and working");
    }

    // Calm: nobody's enemy. Shoot one of the family: the family, and only it, comes for you.
    check(!ranches.hostile_to(0, ranches.vakeros()[0].bot, now), "Vakeroj leave the player alone");
    ranches.provoke(ranches.vakeros()[static_cast<std::size_t>(ranch.family[0])].bot, now);
    check(ranches.hostile_to(0, ranches.vakeros()[static_cast<std::size_t>(ranch.family[3])].bot, now) &&
          !ranches.hostile_to(0, ranches.vakeros()[0].bot, now), "hurt one and that family shoots back");
    check(!ranches.hostile_to(ranches.vakeros()[static_cast<std::size_t>(ranch.family[0])].bot, ranches.vakeros()[0].bot, now), "not at their neighbours");
    ranches.forgive();
    // Shoot a steer: the same.
    {
        auto& cow = ranch.herd[4];
        frame.aim_origin = Vector3Add(frame.player_feet, {0, 1.5F, 0});
        frame.aim_direction = Vector3Normalize(Vector3Subtract(Vector3Add(cow.position, {0, 1.0F, 0}), frame.aim_origin));
        frame.player_fired = true;
        run(.1F);
        frame.player_fired = false;
        check(ranch.herd[4].state == CowState::Dead && ranches.angry(east, now), "shoot their cattle and the family comes for you");
        ranches.forgive();
    }

    // The cattle truck sends word: three head go to the corral; up the chute they go.
    const int before = alive(ranch);
    ranches.call_cattle(east, 3);
    run(90);
    check(ranches.in_corral(east) == 3, "three head brought into the corral (" + std::to_string(ranches.in_corral(east)) + ")");
    for (const auto& cow : ranch.herd)
        if (cow.state == CowState::InCorral) check(ranch.frame.inside(cow.position, rr::corral), "penned in the corral");
    const int loaded = ranches.load_cattle(east, 4);
    float highest = -1e9F;
    run(40, [&] {
        for (const auto& cow : ranch.herd)
            if (cow.state == CowState::Loading)
                highest = std::max(highest, cow.position.y - world::terrain::TerrainHeight::sample(cow.position.x, cow.position.z));
    });
    check(loaded == 3 && alive(ranch) == before - 3, "up the chute and into the trailer: " + std::to_string(loaded));
    check(highest > 1.0F, "they climb the ramp (" + std::to_string(highest) + " m)");
    ranches.unload_cattle(loaded);
    run(30);
    int penned = 0;
    const auto& yard = ranches.stockyard();
    for (const auto& cow : yard.pen) penned += yard.frame.inside(cow.position, rr::stockyard_pens[0]) || yard.frame.inside(cow.position, rr::stockyard_pens[1]);
    check(penned == static_cast<int>(yard.pen.size()) && yard.pen.size() >= 3 + game::ranch::Ranches::stockyard_size / 2,
          "and off into the stockyard's holding pens (" + std::to_string(penned) + " of " + std::to_string(yard.pen.size()) + ")");
    check(world::zones::zone_at(yard.frame.at(0, 34)) == world::zones::ZoneId::Lumamaro, "the stockyard is at Porto Luma");
    // Calves in the morning.
    clock = game::life::WorldClock::at(clock.day() + 1, 6, 10);
    run(1);
    check(alive(ranch) == game::ranch::Ranches::herd_size, "calves born overnight bring the herd back up");

    // Night: the family is home and the side-by-side parked.
    clock = game::life::WorldClock::at(clock.day(), 22, 0);
    run(30);
    check(!ranch.sxs_running, "the side-by-side is parked at night");

    // Away from the ranch they sleep (and cost nothing).
    frame.player_feet = {5000, 0, 5000};
    run(1);
    for (const auto& k : ranch.family) check(bots.asleep(ranches.vakeros()[static_cast<std::size_t>(k)].bot), "asleep out of sight");

    // The big rigs.
    game::transit::BusLine buses;
    check(buses.build(region, cars, &people, 1), "the bus loop");
    game::freight::Freight freight;
    check(freight.build(region, cars, buses, &ranches, &people, 3), "the rigs");
    check(freight.rigs().size() == 5, "five rigs on the road (" + std::to_string(freight.rigs().size()) + ")");
    const game::freight::Rig* supply = nullptr;
    const game::freight::Rig* cattle = nullptr;
    const game::freight::Rig* shuttle = nullptr;
    for (const auto& rig : freight.rigs()) {
        if (rig.cattle) cattle = &rig;
        else if (rig.vehicle.find("semi_freight_") != std::string::npos) supply = &rig;
        else if (rig.vehicle.ends_with("semi_haul_1")) shuttle = &rig;
    }
    check(supply && supply->stops.size() >= 5, "the supply rig calls at the towns");
    check(cattle && cattle->stops.size() == 6 && std::any_of(cattle->stops.begin(), cattle->stops.end(),
          [](const game::freight::FreightStop& s) { return s.ranch == game::freight::stop_cattle_quay; }),
          "the cattle rig calls at every ranch and the cattle quay");
    check(shuttle && shuttle->stops.size() == 2 && std::any_of(shuttle->stops.begin(), shuttle->stops.end(),
          [](const game::freight::FreightStop& s) { return s.ranch == game::freight::stop_cargo_quay; }),
          "Verda Transporto's shuttle runs between the depot and the ship");
    if (supply) { std::cout << "supply run: " << supply->loop.length / 1000 << " km:"; for (const auto& s : supply->stops) std::cout << ' ' << s.name; std::cout << '\n'; }
    if (cattle) { std::cout << "cattle run: " << cattle->loop.length / 1000 << " km, " << freight.reversals() << " reversals:"; for (const auto& s : cattle->stops) std::cout << ' ' << s.name; std::cout << '\n'; }
    freight.start(region, cars);
    freight.follow_trailers(region, cars);
    check(freight.trailers().size() == 9, "every tractor has its trailer");
    int unloaded = 0, loads = 0, deliveries = 0, calls = 0;
    float worst_fold = 0;
    const int start_shipped = ranches.cattle_quay().shipped;
    frame.player_feet = {5000, 0, 5000};
    for (int i = 0; i < 35 * 60 * 10; ++i) {   // 35 minutes
        clock = game::life::WorldClock::at(2, 10, 0);
        frame.clock = clock;
        now += .1;
        frame.now = now;
        frame.dt = .1F;
        ranches.update(frame, bots, cars, region);
        freight.update(.1F, region, cars, frame.player_feet, nullptr, &ranches);
        freight.follow_trailers(region, cars);
        if (i % 50 == 0)
            for (const auto& t : freight.trailers()) {
                const int v = cars.find(t.vehicle);
                if (v >= 0) {
                    const float fold = std::abs(std::remainder(t.yaw - cars.vehicles()[static_cast<std::size_t>(v)].yaw, 360.0F));
                    worst_fold = std::max(worst_fold, fold);
                }
            }
        if (cattle) loads = std::max(loads, cattle->head);
        if (supply) deliveries = supply->deliveries;
        if (shuttle) calls = shuttle->deliveries;
    }
    unloaded = ranches.cattle_quay().shipped - start_shipped;
    int earned = 0;
    for (const auto& r : ranches.ranches()) earned += r.earned;
    std::cout << "in 35 minutes: " << deliveries << " deliveries, most aboard " << loads << ", " << unloaded << " shipped from the cattle quay ("
              << earned << " kr to the ranches), " << calls << " shuttle calls, trailer fold up to " << worst_fold << " deg\n";
    check(deliveries >= 2, "the supply rig delivers to towns");
    check(loads > 0, "the cattle truck loads cattle at the ranches");
    check(unloaded > 0, "and takes them to the cattle quay to be shipped");
    check(earned == unloaded * game::ranch::Ranches::cattle_price, "the ranches are paid for their cattle");
    check(calls >= 2, "the shuttle calls at the depot and the cargo terminal");
    check(worst_fold < 95, "trailers never fold past a right angle");
    for (const auto& t : freight.trailers()) check(std::isfinite(t.centre.x) && std::isfinite(t.yaw), "trailer pose");

    // Dropping a trailer and hitching it again: it stays where it was dropped; the tractor has to
    // back its fifth wheel under the trailer's nose.
    {
        freight.stop(region, cars);
        freight.follow_trailers(region, cars);
        const auto poses = freight.trailers();
        check(!poses.empty(), "a trailer to drop");
        const std::string id = poses.front().vehicle;
        const Vector3 centre = poses.front().centre;
        const auto* tractor = cars.asset(region, id);
        const Vector3 home = tractor->position;
        const float yaw = tractor->rotation_y;
        check(freight.drop_or_hitch(region, cars, id).find("dropped") != std::string::npos && !freight.hitched(id), "trailer dropped");
        const Vector3 away{home.x + std::sin(yaw * DEG2RAD) * 12, home.y, home.z + std::cos(yaw * DEG2RAD) * 12};
        check(cars.place(region, id, away, yaw), "the tractor pulls away");
        freight.follow_trailers(region, cars);
        bool stayed = false;
        for (const auto& t : freight.trailers()) if (t.vehicle == id) stayed = Vector3Distance(t.centre, centre) < .01F;
        check(stayed, "the dropped trailer stays put");
        check(freight.drop_or_hitch(region, cars, id).find("Back") != std::string::npos && !freight.hitched(id), "too far to hitch");
        check(cars.place(region, id, home, yaw), "backed up under it");
        check(freight.drop_or_hitch(region, cars, id) == "Trailer hitched" && freight.hitched(id), "hitched again");
    }

    if (failures) { std::cerr << failures << " failures\n"; return 1; }
    std::cout << "ranches ok\n";
    return 0;
}
