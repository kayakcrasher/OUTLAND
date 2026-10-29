// The Policio and Hospitalo de Verda: a patrol car at every station and three ambulances; cars
// go out for a wanted player (one per star), pull up and their officers get out, armed from two
// stars, and go home when the search is off; an ambulance collects a wounded resident and brings
// them to the hospital, where island life keeps them until they are discharged.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/game/law/JailTerms.hpp"
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
    int clinics = 0;
    for (const auto& owner : region.settlements())
        for (const auto& a : owner.assets) clinics += a.model_path.ends_with("civic/clinic.glb");
    std::cout << clinics << " clinics\n";
    check(units == police + ambulances && police == stations + 1 && ambulances == 3 + clinics, "a car per station (two in Verda City), three ambulances and one per clinic");
    check(services.has_hospital(), "Hospitalo de Verda");
    // A clinic in every town but the capital (it has the hospital), each a place to be treated.
    for (const auto& town : region.settlements()) {
        if (town.id.starts_with("ranch_") || town.id.starts_with("hamlet_") || town.id == "capital_verda") continue;
        const auto* site = services.nearest_medical(town.center);
        check(site && !site->hospital && std::hypot(site->door.x - town.center.x, site->door.z - town.center.z) < 300, "a clinic in " + town.name);
    }
    check(services.medical_sites().size() == static_cast<std::size_t>(clinics + 1), "the hospital and every clinic");
    for (const auto& u : services.units())
        if (u.kind == UnitKind::Ambulance) check(std::hypot(u.base.x - u.home.x, u.base.z - u.home.z) < 40, "each ambulance brings patients to its own clinic or the hospital");
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
    // Driving home with the player looking on: never parked in the street and left there - it is
    // back at its base, or still on its way.
    const auto parked_away = [&] {
        for (const auto& u : services.units())
            if (u.kind == UnitKind::Ambulance && u.state == UnitState::Parked) {
                const auto& car = cars.vehicles()[static_cast<std::size_t>(cars.find(u.vehicle))];
                if (std::hypot(car.position.x - u.home.x, car.position.z - u.home.z) > 8) return true;
            }
        return false;
    };
    bool stranded = false;
    step(frame, 90, [&] { stranded |= parked_away(); return services.count(UnitKind::Ambulance, UnitState::Parked) == ambulances; });
    for (const auto& u : services.units())
        if (u.kind == UnitKind::Ambulance && u.state != UnitState::Parked) {
            const auto& car = cars.vehicles()[static_cast<std::size_t>(cars.find(u.vehicle))];
            std::cout << u.vehicle << ": still " << game::services::unit_state_name(u.state) << " at " << car.position.x << "," << car.position.z
                      << " home " << u.home.x << "," << u.home.z << '\n';
        }
    check(!stranded, "an ambulance is never left parked in the street");
    // The player walks off: every ambulance is home.
    frame.player = {0, 0, 9000};
    step(frame, 2, [&] { return services.count(UnitKind::Ambulance, UnitState::Parked) == ambulances; });
    check(services.count(UnitKind::Ambulance, UnitState::Parked) == ambulances && !parked_away(), "the ambulance is back at the hospital");
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
    // Prison: a week inside for anyone booked in. Each either joins a gang or gets shanked (to the
    // hospital, and out from there); the rest walk out of the gate when the week is up.
    {
        using game::law::JailTerms;
        JailTerms terms;
        terms.seed(11);
        const double now = life.clock().minutes;
        int joined = 0, shanked = 0, released = 0;
        for (int k = 0; k < 40; ++k) terms.book(100 + k, -1, now);
        terms.book(500, -1, now, true);   // a gangster: with his gang already
        check(terms.update(now + JailTerms::decide_min - 1).empty(), "nothing happens on the first day");
        for (const auto& e : terms.update(now + JailTerms::decide_max)) {
            if (e.fate == JailTerms::Fate::Joined) { ++joined; check(e.term.gang >= 0 && e.term.gang < JailTerms::gangs, "joined a real gang"); }
            if (e.fate == JailTerms::Fate::Shanked) ++shanked;
            check(e.term.inmate != 500, "the gangster doesn't have to choose");
        }
        check(joined + shanked == 40 && joined > 12 && shanked > 6, "everyone chooses: " + std::to_string(joined) + " joined, " + std::to_string(shanked) + " shanked");
        check(terms.terms().size() == static_cast<std::size_t>(joined + 1), "the shanked are gone from the yard");
        check(terms.update(now + JailTerms::week - 1).empty(), "the rest are still inside before the week is up");
        terms.moved(100, 900);
        for (const auto& e : terms.update(now + JailTerms::week)) released += e.fate == JailTerms::Fate::Released;
        check(released == joined + 1 && terms.terms().empty(), "out of the gate after a week");

        // Island life: a resident put away is off the street until let out at the gate.
        int convict = -1;
        for (const auto& r : life.residents()) if (r.alive && !r.physical && !life.in_hospital(r)) { convict = r.id; break; }
        check(convict >= 0, "a resident to put away");
        const Vector3 gate{1234, 0, 567};
        check(life.imprison(convict, gate, life.clock().minutes + JailTerms::week), "imprisoned");
        const auto& r = life.residents()[static_cast<std::size_t>(convict)];
        check(life.in_jail(r) && Vector3Distance(life.position(r), gate) < .1F, "in prison");
        life.set_free(convict);
        for (int k = 0; k < 400 && r.jail_until >= 0; ++k) life.update(.05F, {0, 0, 9000}, npcs);
        check(!life.in_jail(r) && r.jail_until < 0, "let out");
        check(Vector3Distance(life.position(r), gate) < 30, "out of the gate");
        // Shanked inside: the hospital has them, and lets them out (not back to prison).
        check(life.imprison(convict, gate, life.clock().minutes + JailTerms::week) && life.hospitalise(convict, hospital, 90), "shanked: to hospital");
        check(life.in_hospital(r) && !life.in_jail(r), "in hospital, not prison");
    }
    std::cout << "emergency services tests passed\n";
    return 0;
}
