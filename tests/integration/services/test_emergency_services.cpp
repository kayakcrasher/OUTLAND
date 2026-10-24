// The Policio and Hospitalo de Verda: a patrol car at every station and three ambulances; cars
// go out for a wanted player (one per star), pull up and their officers get out, armed from two
// stars, and go home when the search is off; an ambulance collects a wounded resident and brings
// them to the hospital, where island life keeps them until they are discharged.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/game/services/EmergencyServices.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include <raymath.h>
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace outland;
using game::services::EmergencyServices;
using game::services::UnitKind;
using game::services::UnitState;

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
    check(types.find("police") && types.find("ambulance"), "police car and ambulance types");
    game::vehicles::VehicleSystem cars(types);
    cars.reconcile(region);
    game::life::LifeSimulation life;
    life.build(region, nullptr);
    for (const auto& car : life.island().cars)
        check(car.vehicle.find("_police_") == std::string::npos && car.vehicle.find("_ambulance_") == std::string::npos, "residents don't drive the services' cars");
    characters::NpcSystem npcs;

    // Every town has a station (not the small trailer hamlets); the capital two patrol cars; the
    // hospital three ambulances.
    int stations = 0;
    for (const auto& owner : region.settlements())
        for (const auto& a : owner.assets) stations += a.model_path.ends_with("civic/police_station.glb");
    for (const auto& town : region.settlements()) {
        if (town.id.starts_with("ranch_")) continue; // the Vakeroj's ranches are not towns
        bool station = false;
        for (const auto& owner : region.settlements())
            for (const auto& a : owner.assets)
                station |= a.model_path.ends_with("civic/police_station.glb") &&
                           std::hypot(a.position.x - town.center.x, a.position.z - town.center.z) < (town.id.starts_with("hamlet_") ? 150 : 450);
        if (town.id.starts_with("hamlet_")) check(!station, "no police station in the trailer hamlet " + town.name);
        else check(station, "a police station in " + town.name);
    }
    EmergencyServices services;
    const int units = services.start(cars, region, nullptr);
    int police = 0, ambulances = 0;
    for (const auto& u : services.units()) (u.kind == UnitKind::Police ? police : ambulances)++;
    std::cout << stations << " stations, " << police << " patrol cars, " << ambulances << " ambulances\n";
    check(units == police + ambulances && police == stations + 1 && ambulances == 3, "a car per station (two in Verda City) and three ambulances");
    check(services.has_hospital(), "Hospitalo de Verda");
    const Vector3 hospital = services.hospital_door();

    const auto step = [&](game::services::ServicesFrame frame, float seconds, const std::function<bool()>& until = {}) {
        for (float t = 0; t < seconds; t += .05F) {
            frame.dt = .05F;
            services.update(frame, cars, region, npcs, &life);
            cars.update(.05F, {}, region, frame.player, false);
            characters::NpcContext context;
            context.player_position = frame.player;
            context.player_alive = frame.player_alive;
            npcs.update(.05F, context, region);
            if (until && until()) return t;
        }
        return seconds;
    };

    // Two stars, on foot outside the hospital: two patrol cars come with lights and siren.
    game::services::ServicesFrame frame;
    frame.player = hospital;
    frame.last_known = hospital;
    frame.wanted = 2;
    frame.sight = [](Vector3, Vector3) { return true; };
    const auto responding = [&] {
        int n = 0;
        for (const auto& u : services.units())
            if (u.kind == UnitKind::Police && (u.state == UnitState::Responding || u.state == UnitState::Pursuit || u.state == UnitState::OnScene)) ++n;
        return n;
    };
    step(frame, .1F);
    check(responding() == 2, "one car per star");
    for (const auto& u : services.units()) if (u.kind == UnitKind::Police && u.state == UnitState::Responding) check(u.siren, "lights and siren");
    const float arrived = step(frame, 120, [&] { return services.count(UnitKind::Police, UnitState::OnScene) > 0; });
    std::cout << "the first patrol car on scene after " << arrived << " s\n";
    check(services.count(UnitKind::Police, UnitState::OnScene) > 0, "a patrol car pulls up");
    step(frame, 1);
    const auto officers = services.officers_on_foot(npcs);
    check(officers.size() >= 2, "its officers get out");
    bool armed = true;
    for (const auto& u : services.units())
        if (u.state == UnitState::OnScene)
            for (const int id : u.crew) armed &= npcs.actors()[static_cast<std::size_t>(npcs.find_resident(id))].armed;
    check(armed && EmergencyServices::crew_member(services.units().front().crew.empty() ? EmergencyServices::crew_base : services.units().front().crew.front()), "armed at two stars");

    // The player drives off: the officers get back in and the car gives chase.
    frame.player_driving = true;
    frame.player = Vector3Add(hospital, {60, 0, 0});
    step(frame, 15, [&] { return services.count(UnitKind::Police, UnitState::Pursuit) > 0; });
    check(services.count(UnitKind::Police, UnitState::Pursuit) > 0, "a pursuit when the suspect drives away");

    // The search is called off: everyone home.
    frame.wanted = 0;
    frame.player_driving = false;
    frame.player = {0, 0, 9000};
    step(frame, 60, [&] { return responding() == 0; });
    check(responding() == 0 && services.officers_on_foot(npcs).empty(), "the search is off: cars go home, officers with them");

    // A wounded resident in the street: an ambulance comes and takes them to hospital.
    int patient = -1;
    for (const auto& r : life.residents()) if (r.alive && !r.physical) { patient = r.id; break; }
    check(patient >= 0, "a resident to hurt");
    characters::ResidentSpawn spawn;
    spawn.resident = patient;
    spawn.position = Vector3Add(hospital, {-30, 0, -25});
    spawn.anchor = spawn.position;
    spawn.health = 30;
    npcs.spawn_resident(spawn);
    frame.player = Vector3Add(spawn.position, {10, 0, 0});
    step(frame, .1F);
    const auto* sent = [&]() -> const game::services::Unit* {
        for (const auto& u : services.units()) if (u.kind == UnitKind::Ambulance && u.patient == patient) return &u;
        return nullptr;
    }();
    check(sent && sent->siren, "an ambulance on its way, lights and siren");
    bool admitted = false;
    const float loaded = step(frame, 150, [&] {
        for (const int id : services.events().hospitalised) admitted |= id == patient;
        return admitted;
    });
    std::cout << "the patient was in the ambulance after " << loaded << " s\n";
    check(admitted && npcs.find_resident(patient) < 0, "loaded into the ambulance");
    check(life.in_hospital(life.residents()[static_cast<std::size_t>(patient)]), "admitted to hospital");
    check(Vector3Distance(life.position(life.residents()[static_cast<std::size_t>(patient)]), hospital) < 1, "at the hospital");
    step(frame, 90, [&] { return services.count(UnitKind::Ambulance, UnitState::Parked) == 3; });
    for (const auto& u : services.units())
        if (u.kind == UnitKind::Ambulance && u.state != UnitState::Parked) {
            const auto& car = cars.vehicles()[static_cast<std::size_t>(cars.find(u.vehicle))];
            std::cout << u.vehicle << ": " << game::services::unit_state_name(u.state) << " at " << car.position.x << "," << car.position.z
                      << " home " << u.home.x << "," << u.home.z << " route " << u.progress << "/" << u.route.length << " attempts " << u.attempts
                      << " crew " << u.crew.size() << '\n';
        }
    check(services.count(UnitKind::Ambulance, UnitState::Parked) == 3, "the ambulance is back at the hospital");
    // A hit and run by somebody else: the nearest free unit goes after the car, catches it up,
    // pulls it over and makes the arrest.
    {
        const auto& station = *std::find_if(services.units().begin(), services.units().end(), [](const auto& u) { return u.kind == UnitKind::Police; });
        std::string suspect;
        float best = 1e9F;
        for (const auto& car : cars.vehicles()) {
            if (services.unit(car.id) || car.id.find("_police_") != std::string::npos) continue;
            const float d = std::hypot(car.position.x - station.home.x, car.position.z - station.home.z);
            if (d > 40 && d < best) { best = d; suspect = car.id; }
        }
        check(!suspect.empty() && best < 600, "a car near a station to drive off in");
        game::vehicles::VehicleInput off;
        off.throttle = .35F;
        cars.set_autopilot(suspect, off);   // drives on after the hit
        frame.player = {0, 0, 9000};
        frame.wanted = 0;
        const Vector3 scene = cars.vehicles()[static_cast<std::size_t>(cars.find(suspect))].position;
        check(services.pursue(suspect, scene, cars) && services.pursued(suspect), "the police go after the hit-and-run driver");
        check(!services.pursue(suspect, scene, cars), "...one unit for one car");
        bool caught = false;
        const float took = step(frame, 120, [&] {
            for (const auto& id : services.events().arrested) caught |= id == suspect;
            return caught;
        });
        std::cout << "the hit-and-run driver (" << best << " m from the station) caught after " << took << " s\n";
        check(caught, "pulled over and arrested");
        check(cars.held(suspect) || !services.pursued(suspect), "the car was pulled over");
        step(frame, 30, [&] { return !services.pursued(suspect); });
        check(!services.pursued(suspect) && !cars.held(suspect), "the unit goes back on duty and lets the car go");
        cars.clear_autopilot(suspect);
    }
    std::cout << "emergency services tests passed\n";
    return 0;
}
