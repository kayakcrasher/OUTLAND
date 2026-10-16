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
// Ambulances: anyone hurt or killed in the street (a resident or an officer) is collected by the
// nearest free ambulance. Two paramedics go to them, they are loaded, and the ambulance drives
// back to the hospital - lights and siren if the patient is alive. The wounded are treated and
// walk out of the hospital door later (game::life); the dead go to the morgue.
//
// Officers and paramedics on foot are NpcSystem residents with ids from crew_base up (not island
// life's residents).
namespace outland::game::services {

enum class UnitKind : std::uint8_t { Police, Ambulance };
enum class UnitState : std::uint8_t { Parked, Responding, Pursuit, OnScene, Returning, Out };
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
    [[nodiscard]] int count(UnitKind kind, UnitState state) const;
    [[nodiscard]] const ServicesEvents& events() const { return events_; }
    // The player on the force (game::career): their patrol car leaves the dispatch, and its
    // lights run while `siren` (on a call) and they are at the wheel. "" hands it back.
    void assign_player_car(const std::string& vehicle) { player_car_ = vehicle; }
    void set_player_siren(bool siren) { player_siren_ = siren; }
    [[nodiscard]] const std::string& player_car() const { return player_car_; }
    // The police station (its patrol car's parking place) within `radius` of `feet`, or null.
    [[nodiscard]] const Unit* station_near(Vector3 feet, float radius) const;

private:
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
    std::string character(bool police);
    float random();

    std::vector<Unit> units_;
    world::roads::RoadGraph roads_;
    const characters::CharacterRegistry* registry_{nullptr};
    Vector3 hospital_door_{};
    bool hospital_found_{false};
    int next_crew_{0};
    std::vector<int> claimed_;   // patients an ambulance is on its way to
    ServicesEvents events_;
    std::string player_car_;
    bool player_siren_{false};
    std::uint64_t rng_{0x6a09e667f3bcc909ULL};
};

// The light bars: flashing red and blue (police) or red and amber (ambulance) over the roof of
// every unit running with its siren, drawn inside BeginMode3D.
// One police car's bar running (the player's car with its lights on): bar lenses at (±width, y, z)
// in the car's frame, and the grille flashers.
void draw_light_bar(Vector3 position, float yaw, float y, float z, float width, int seed, Vector3 camera, float time);
void draw_light_bars(const EmergencyServices& services, const vehicles::VehicleSystem& cars, Vector3 camera, float time);

} // namespace outland::game::services
