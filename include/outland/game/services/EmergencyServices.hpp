#pragma once
#include "outland/world/roads/RoadGraph.hpp"
#include <raylib.h>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::characters { class NpcSystem; class CharacterRegistry; }
namespace outland::game::vehicles { class VehicleSystem; }
namespace outland::game::life { class LifeSimulation; }

// Verda's emergency services in Explore: the Policio's patrol cars (one at every town's police
// station, two in Verda City) and Hospitalo de Verda's three ambulances.
//
// Police: while the player is wanted (game::law::Justice) the nearest patrol cars - one per star,
// three at most - drive out with lights and siren to where the player was last seen. A car that
// sees the player driving gives chase, straight at them; once the player is on foot nearby it
// pulls up and its two officers get out: unarmed at one star (they come to arrest), armed from
// two (they shoot). When the player drives off they get back in and the chase goes on. When the
// search is called off the cars go home to their stations.
//
// Ambulances: three at Hospitalo de Verda and one at every town's Kliniko. Anyone hurt or killed in
// the street (a resident or an officer) is collected by the nearest free ambulance. It drives up
// (straight at them for the last stretch if the roads run out), two paramedics go to them - they
// don't run from gunfire - they are loaded, the crew get back in, and the ambulance drives back
// to its hospital or clinic, lights and siren if the patient is alive. The wounded are treated
// and walk out of that door later (game::life); the dead go to the morgue. A unit that can't find
// its way home tries again, and is only put back at its base once the player is out of sight.
//
// The Ŝosea Patrolo (game::services::HighwayPatrol): tan cruisers the game adds one per outpost.
// They drive the ring road's beat round and round; one that finds a wanted player close by joins
// the chase and radios the Policio for backup (events().backup); guard() sends the nearest to a
// ranch in trouble. Two troopers stand at each outpost, and turn on a dangerous player near it.
//
// Hit and run (game::law::HitAndRun): somebody else's car that hits a pedestrian and drives on is
// chased too (pursue()). The nearest free patrol car or cruiser rides out with its lights, catches
// the car up, pulls it over (VehicleSystem::hold) and its officers make the arrest. A wanted player
// comes first: the units on the player stay on the player.
//
// Officers and paramedics on foot are NpcSystem residents with ids from crew_base up (not island
// life's residents).
namespace outland::game::services {

enum class UnitKind : std::uint8_t { Police, Ambulance, Highway };
enum class UnitState : std::uint8_t { Parked, Responding, Pursuit, OnScene, Returning, Out, Patrol };
const char* unit_state_name(UnitState state);

struct Unit {
    UnitKind kind{UnitKind::Police};
    std::string vehicle;
    Vector3 home{};
    float home_yaw{0};
    UnitState state{UnitState::Parked};
    bool siren{false};
    world::roads::Route route;
    float progress{0}, stuck{0}, reverse{0}, replan{0}, scene{0}, unseen{0};
    int attempts{0};
    Vector3 target{};
    std::vector<int> crew;     // NpcSystem resident ids on foot
    int patient{-1};           // NpcSystem resident id being collected (ambulance)
    bool loaded{false}, patient_alive{false};
    int waypoint{-1};          // Ŝosea Patrolo: where on the beat
    std::string suspect;       // the car of a hit-and-run driver this unit is after ("" none)
    float gap{0}, stalled{0};  // ...the closest it has come in the chase, and how long since it closed in
    double call_until{-1};     // called to a scene (a riot): there till then (ServicesFrame::now)
    Vector3 call_at{};
    Vector3 base{};            // ambulance: the door of its hospital or clinic, where patients go
    float approach{0};         // ambulance: time spent driving straight at a patient off the roads
    int retries{0};            // going home: fresh routes tried
};

// Where the hurt are treated: Hospitalo de Verda and the towns' clinics.
struct MedicalSite {
    Vector3 door{};
    std::string name;
    bool hospital{false};
};

struct ServicesFrame {
    float dt{0};
    double now{0};
    Vector3 player{}, player_velocity{};   // feet
    bool player_alive{true}, player_driving{false};
    int wanted{0};
    Vector3 last_known{};
    std::function<bool(Vector3, Vector3)> sight; // eye to eye, true if clear
};

struct ServicesEvents {
    std::vector<int> hospitalised;  // island life residents brought in alive
    std::vector<int> morgue;        // island life residents brought in dead
    std::string message;
    bool backup{false};             // a Ŝosea Patrolo car found the wanted player and radioed for backup
    std::vector<std::string> arrested; // the hit-and-run drivers' cars stopped and their drivers taken in
};

class EmergencyServices {
public:
    static constexpr int crew_base = 900000;
    static constexpr float treatment_minutes = 90; // island minutes in hospital

    // Finds the patrol cars and ambulances (vehicle ids with "_police_" / "_ambulance_") and the
    // hospital (the city kit's civic/hospital.glb). Returns the number of units.
    int start(vehicles::VehicleSystem& cars, world::VerdaRegion& region, const characters::CharacterRegistry* registry);
    void clear(vehicles::VehicleSystem& cars, characters::NpcSystem& npcs);
    void update(const ServicesFrame& frame, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                characters::NpcSystem& npcs, life::LifeSimulation* life);

    [[nodiscard]] bool active() const { return !units_.empty(); }
    [[nodiscard]] const std::vector<Unit>& units() const { return units_; }
    [[nodiscard]] const Unit* unit(std::string_view vehicle) const;
    [[nodiscard]] bool siren(std::string_view vehicle) const;
    [[nodiscard]] static bool crew_member(int resident) { return resident >= crew_base; }
    // Officers on foot from the patrol cars (feet), for Justice.
    [[nodiscard]] std::vector<Vector3> officers_on_foot(const characters::NpcSystem& npcs) const;
    [[nodiscard]] bool has_hospital() const { return hospital_found_; }
    [[nodiscard]] Vector3 hospital_door() const { return hospital_door_; }
    // The hospital and every clinic, and the nearest of them to `feet` (null if there are none).
    [[nodiscard]] const std::vector<MedicalSite>& medical_sites() const { return sites_; }
    [[nodiscard]] const MedicalSite* nearest_medical(Vector3 feet) const;
    [[nodiscard]] int count(UnitKind kind, UnitState state) const;
    [[nodiscard]] const ServicesEvents& events() const { return events_; }
    // The player on the force (game::career): their patrol car leaves the dispatch, and its
    // lights run while `siren` (on a call) and they are at the wheel. "" hands it back.
    void assign_player_car(const std::string& vehicle) { player_car_ = vehicle; }
    void set_player_siren(bool siren) { player_siren_ = siren; }
    [[nodiscard]] const std::string& player_car() const { return player_car_; }
    // The Ŝosea Patrolo: the ring road's beat, a cruiser (an outpost's), its two troopers at the
    // outpost, and a call to a ranch in trouble (the nearest cruiser rides out with its lights).
    void set_beat(std::vector<Vector3> beat) { beat_ = std::move(beat); }
    void add_highway(const std::string& vehicle, Vector3 home, float yaw);
    void post_troopers(Vector3 at, float yaw, characters::NpcSystem& npcs);
    void guard(Vector3 where, vehicles::VehicleSystem& cars);
    [[nodiscard]] bool trooper(int resident) const;
    static constexpr const char* trooper_characters[2]{"character_19_police", "character_20_police"};
    // The police station (its patrol car's parking place) within `radius` of `feet`, or null.
    [[nodiscard]] const Unit* station_near(Vector3 feet, float radius) const;
    // A hit-and-run driver (their car's vehicle id), last seen at `where`: the nearest free patrol
    // car or cruiser goes after them. False if nobody is free (or the car is already wanted).
    bool pursue(const std::string& suspect, Vector3 where, vehicles::VehicleSystem& cars);
    [[nodiscard]] bool pursued(std::string_view suspect) const;
    static constexpr float suspect_reach = 1600;   // how far a free unit rides out for a hit and run
    // A call to a scene (a riot at a ballpark): up to `cars` of the nearest free patrol cars ride
    // out with their lights, their officers get out and stand guard there until `until`
    // (ServicesFrame::now), then go home. Returns how many went.
    int respond(Vector3 where, int cars, double until, vehicles::VehicleSystem& cars_system);

private:
    void after_suspect(Unit& u, const ServicesFrame& frame, vehicles::VehicleSystem& cars, world::VerdaRegion& region, characters::NpcSystem& npcs);
    void drop_suspect(Unit& u, vehicles::VehicleSystem& cars);
    void at_call(Unit& u, const ServicesFrame& frame, vehicles::VehicleSystem& cars, world::VerdaRegion& region, characters::NpcSystem& npcs);
    void highway(Unit& u, const ServicesFrame& frame, vehicles::VehicleSystem& cars, world::VerdaRegion& region, characters::NpcSystem& npcs);
    void police(Unit& u, const ServicesFrame& frame, vehicles::VehicleSystem& cars, world::VerdaRegion& region, characters::NpcSystem& npcs);
    void ambulance(Unit& u, const ServicesFrame& frame, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                   characters::NpcSystem& npcs, life::LifeSimulation* life);
    void dispatch(Unit& u, Vector3 target, bool siren, vehicles::VehicleSystem& cars);
    void go_home(Unit& u, vehicles::VehicleSystem& cars);
    bool drive(Unit& u, vehicles::VehicleSystem& cars, float dt);          // true when the route is done
    void chase(Unit& u, vehicles::VehicleSystem& cars, Vector3 target, Vector3 velocity, float dt);
    void halt(Unit& u, vehicles::VehicleSystem& cars);
    void crew_out(Unit& u, int count, bool police, bool armed, vehicles::VehicleSystem& cars, world::VerdaRegion& region, characters::NpcSystem& npcs);
    bool crew_back(Unit& u, vehicles::VehicleSystem& cars, characters::NpcSystem& npcs, float dt, bool hurry_home);
    // Returning: parked at home, or put there once the player is far; true once parked.
    bool homeward(Unit& u, const ServicesFrame& frame, vehicles::VehicleSystem& cars, world::VerdaRegion& region, float far);
    std::string character(bool police);
    float random();

    std::vector<Unit> units_;
    world::roads::RoadGraph roads_;
    const characters::CharacterRegistry* registry_{nullptr};
    Vector3 hospital_door_{};
    bool hospital_found_{false};
    std::vector<MedicalSite> sites_;
    int next_crew_{0};
    std::vector<int> claimed_;   // patients an ambulance is on its way to
    ServicesEvents events_;
    std::string player_car_;
    bool player_siren_{false};
    struct Trooper { int resident{-1}; Vector3 post{}; float yaw{0}; };
    std::vector<Vector3> beat_;
    std::vector<Trooper> troopers_;
    double backup_at_{-1e9};
    std::uint64_t rng_{0x6a09e667f3bcc909ULL};
};

// The light bars: flashing red and blue (police) or red and amber (ambulance) over the roof of
// every unit running with its siren, drawn inside BeginMode3D.
// One police car's bar running (the player's car with its lights on): bar lenses at (±width, y, z)
// in the car's frame, and the grille flashers.
void draw_light_bar(Vector3 position, float yaw, float y, float z, float width, int seed, Vector3 camera, float time);
void draw_light_bars(const EmergencyServices& services, const vehicles::VehicleSystem& cars, Vector3 camera, float time);

} // namespace outland::game::services
