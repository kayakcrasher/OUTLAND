// The Policio's crime calls (game::career::Callouts over game::mafia's heists) on the real island:
// a call goes out with the gang and the getaway car; the suspects wait at the scene, run when an
// officer comes, give up once stopped; cuffed, they walk with the officer and climb into the
// police car; the call is done when both are in; the prison books them. And the prison's snitch,
// and a career's new save fields.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/career/Callouts.hpp"
#include "outland/game/law/PrisonYard.hpp"
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

using namespace outland;
using game::career::Career;
using game::career::JobKind;
using game::mafia::HeistStage;

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
}

int main() {
    // The little things first.
    check(game::career::Callouts::make_plate(7) == game::career::Callouts::make_plate(7) &&
          game::career::Callouts::make_plate(7).starts_with("VRD-") && game::career::Callouts::make_plate(7).size() == 8, "plates");
    check(std::string(game::career::Callouts::shirts(4)).find("rust") != std::string::npos, "the Iron Dogs wear rust red");

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
    game::mafia::Mafio mafio;
    check(mafio.start(life.places(), region, bots, 5) > 0, "the gangs are out");
    mafio.set_next_war(1e9);
    game::ai::BotWorld world;
    world.environment.line_of_sight = [](Vector3, Vector3) { return true; };
    world.environment.move = [&](Vector3 from, Vector3 to) {
        const float terrain = world::terrain::TerrainHeight::sample(to.x, to.z);
        const float probe = std::max(from.y, terrain) + .6F;
        Vector3 out{to.x, world::physics::WorldCollision::ground_height({to.x, probe, to.z}, probe, region), to.z};
        if (world::physics::WorldCollision::body_blocked(out, out.y, region, .3F)) return from;
        return out;
    };

    // A serving officer, due a call.
    Career career;
    std::string why;
    check(career.join_police("police_car", why), why);
    bool issue = false;
    career.chief_talk(issue);
    career.passed_training();
    career.chief_talk(issue);
    check(career.serving(), "serving");
    double now = 0;
    game::career::CareerFrame cf;
    cf.now = now;
    career.update(cf);
    now = 10;
    check(career.due_call(now), "due the next call");

    // The shops in Roka (a robbery there), an officer 300 m off.
    std::vector<game::career::CallScene> scenes;
    for (const auto& place : life.places()) {
        if (place.kind != game::life::PlaceKind::Shop) continue;
        const auto& town = region.settlements()[static_cast<std::size_t>(place.settlement)];
        if (town.id != "west_roka") continue;
        Vector3 out = Vector3Subtract(place.door, place.interior);
        out.y = 0;
        if (Vector3Length(out) < .1F) continue;
        scenes.push_back({place.door, Vector3Normalize(out), "the shop", town.name, true});
    }
    check(!scenes.empty(), "Roka has shops");
    // The shop furthest from any gang's door, so the getaway has a drive home.
    std::stable_sort(scenes.begin(), scenes.end(), [&](const auto& a, const auto& b) {
        const auto clear = [&](Vector3 p) {
            float best = 1e9F;
            for (const auto& crew : mafio.crews()) best = std::min(best, flat(p, crew.door));
            return best;
        };
        return clear(a.door) > clear(b.door);
    });
    game::career::Callouts calls;
    game::career::Callouts::Frame frame;
    frame.now = now;
    frame.officer = Vector3Add(scenes.front().door, {300, 0, 0});
    frame.officer.y = world::terrain::TerrainHeight::sample(frame.officer.x, frame.officer.z);
    frame.alive = true;
    std::vector<game::career::CallScene> one{scenes.front()};
    check(calls.dispatch(frame, one, career, mafio, bots, cars, region, 99), "a call goes out");
    std::cout << "CALL: " << career.job().detail << '\n';
    check(career.job().kind == JobKind::Callout && career.job().needed == 2 && career.job().title == "ROBBERY", "a robbery, two suspects");
    check(career.state().callouts == 1 && calls.line() != nullptr && calls.take_partner_wanted(), "the first call comes with a partner and a lesson");
    check(career.job().detail.find(calls.plate()) != std::string::npos && career.job().detail.find(calls.look()) != std::string::npos,
          "the radio gives the car and its plate");
    check(mafio.heists().size() == 1 && mafio.heists().front().stage == HeistStage::Waiting && cars.find(calls.car()) >= 0, "they wait by the car");
    const auto suspects = mafio.heists().front().squad;
    check(suspects.size() == 2, "two of them");
    for (const int id : suspects) check(flat(bots.find(id)->position(), scenes.front().door) < 6, "at the scene");

    Vector3 player = frame.officer;
    const auto step = [&](float seconds) {
        for (float t = 0; t < seconds; t += .05F) {
            game::mafia::MafioFrame mf;
            mf.dt = .05F; mf.now = now; mf.player_feet = player; mf.player_alive = true;
            mafio.update(mf, bots, world, drivers, cars, region);
            cars.update(.05F, {}, region, player, false);
            frame.dt = .05F; frame.now = now; frame.officer = player;
            std::string said;
            calls.update(frame, career, mafio, bots, cars, region, said);
            now += .05;
        }
    };
    step(3);
    check(mafio.heists().front().stage == HeistStage::Waiting && career.job().kind == JobKind::Callout, "far off, nothing happens");
    // Close by: they run for it (or, with no road home, stand).
    player = Vector3Add(scenes.front().door, Vector3Scale(scenes.front().out, 60));
    player.y = world::terrain::TerrainHeight::sample(player.x, player.z);
    step(1);
    const auto stage = mafio.heists().front().stage;
    check(stage == HeistStage::Fleeing || stage == HeistStage::Stopped, "seen: they move");
    if (stage == HeistStage::Fleeing) {
        step(4);
        // Stopped: the car pulled over.
        drivers.halt(suspects.front(), bots, cars, region);
        step(.2F);
    }
    check(mafio.heists().front().stage == HeistStage::Stopped, "stopped");
    // Face to face: hands up, or a run; caught, a runner gives up too.
    for (int k = 0; k < 40 && mafio.heists().front().surrendered.size() < 2; ++k) {
        const auto& h = mafio.heists().front();
        const int target = !h.running.empty() ? h.running.front() : suspects.front();
        const Vector3 at = bots.find(target)->position();
        player = Vector3Add(at, {1.5F, 0, 0});
        step(.5F);
    }
    check(mafio.heists().front().surrendered.size() == 2, "both gave up");
    for (const int id : suspects) check(mafio.held(id) && !mafio.hostile(0, id, now), "hands up: they hold still and nobody shoots");

    // Cuffed, both walk with the officer.
    for (const int id : suspects) {
        player = Vector3Add(bots.find(id)->position(), {1, 0, 0});
        const int cuff = calls.to_cuff(player, mafio, bots);
        check(cuff >= 0, "in reach to cuff");
        check(calls.cuff(cuff, mafio.gang_of(cuff), mafio, bots), "cuffed");
    }
    check(calls.escorts().size() == 2, "two prisoners");
    const Vector3 start = player;
    player = Vector3Add(player, {12, 0, 0});
    player.y = world::terrain::TerrainHeight::sample(player.x, player.z);
    step(6);
    for (const auto& e : calls.escorts()) check(flat(bots.find(e.bot)->position(), player) < 6, "they keep up with the officer");
    (void)start;
    // Into the police car: the call is done, they are in custody.
    check(cars.spawn(region, player, 0, "police"), "a police car");
    frame.police_car = cars.last_spawned();
    frame.car = cars.vehicles()[static_cast<std::size_t>(cars.find(frame.police_car))].position;
    const int before = career.state().jobs_done;
    step(.2F);
    check(calls.escorts().empty() && career.custody() == 2, "both in the back of the car");
    check(career.job().kind == JobKind::None && career.state().jobs_done == before + 1, "the call is done");
    for (const int id : suspects) check(bots.hidden(id), "out of sight in the car");
    check(career.deliver() == 2 && career.custody() == 0, "booked in at the prison");
    check(career.take_pay() > 0, "paid");

    // The prison: a snitch in the yard, who can be stopped and taken away.
    game::law::PrisonYard yard;
    yard.start({0, 0, 0}, 0, 3, {});
    const int snitch = yard.mark_snitch();
    check(snitch >= 0 && yard.find(snitch) && !yard.find(snitch)->guard && !yard.find(snitch)->gang, "a law breaker who talked");
    check(yard.mark_snitch() == snitch, "the same snitch until he's gone");
    yard.pause(snitch, 5);
    const Vector3 was = yard.find(snitch)->local;
    yard.update(1);
    check(Vector3Distance(was, yard.find(snitch)->local) < 1e-4F, "he stops for the new man");
    const int inmates = yard.inmates();
    check(yard.remove(snitch) && yard.inmates() == inmates - 1 && yard.snitch() < 0, "gone");

    // The career keeps the new fields.
    Career inmate;
    inmate.join_gang(1, "Montaj Lupoj", why);
    inmate.jailed();
    check(inmate.inmate_number() >= 10000 && inmate.inmate_number() < 100000, "a five-digit prison number");
    const float respect = inmate.state().respect;
    inmate.shanked_snitch();
    check(inmate.state().respect > respect && inmate.state().record, "put in work: respect, and a record");
    inmate.inside_told();
    Career back;
    check(back.parse(inmate.serialise()) && back.inmate_number() == inmate.inmate_number() && back.state().inside_told, "saved: the number");
    check(back.parse(career.serialise()) && back.state().callouts == 1, "saved: the calls answered");
    career.prison_toured();
    check(back.parse(career.serialise()) && back.state().prison_toured, "saved: the warden's tour");
    check(back.parse("career 5 1 2 10 1 0 - 3 1 0 0 4") && back.state().callouts == 0 && back.inmate_number() == 0, "older saves load");

    std::cout << "[PASS] Callouts: the call, the chase, the arrest, the ride in; the snitch\n";
    return 0;
}
