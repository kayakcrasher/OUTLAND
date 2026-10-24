#pragma once
#include "outland/world/roads/RoadGraph.hpp"
#include <raylib.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::characters { class NpcSystem; class CharacterRegistry; }
namespace outland::game::vehicles { class VehicleSystem; }
namespace outland::game::transit { class BusLine; }
namespace outland::game::ranch { class Ranches; }

// Verda's big rigs: 18-wheelers (the "semi" tractor) with articulated trailers.
//   The supply rig (vehicle_spawn_semi_freight_..., parked at the stockyard of Porto Luma) hauls a
//   53 ft dry van round the island on the bus line's loop and pulls up in every town on the line
//   to deliver (Verda, Espera, Porto Luma, Suda Haveno, Roka).
//   The cattle rig (vehicle_spawn_semi_cattle_...) hauls a livestock "pot" trailer from ranch to
//   ranch: at each ranch's gate the family has a few head waiting in the corral and they go up the
//   chute; then on to the cattle quay at Porto Luma's cargo terminal, where they come off and are
//   shipped - and each ranch is paid for its cattle (ranch::Ranches::ship_cattle). (Without the
//   cargo terminal, to the stockyard, Bovbazaro de Porto Luma, where they come off into the pens.)
//   Verda Transporto's own drivers (vehicle_spawn_semi_haul_..., at the depot in Verda City's
//   industrial quarter): a container shuttle between the depot and the cargo terminal's cranes, a
//   second supply run on the bus loop (starting halfway round), and a run from the terminal to Roka
//   and back to the depot.
// Like the buses they drive kinematically (VehicleSystem::glide) along the road; the trailer
// follows the fifth wheel (its axles trail the kingpin) wherever the tractor goes, the player's
// hands included. A rig that is wrecked or taken leaves its run.
namespace outland::game::freight {

// Trailer geometry (tools/generate_ranch_models.py).
inline constexpr float trailer_length = 16;
inline constexpr float kingpin_to_centre = 7.0F;      // kingpin 1 m behind the nose
inline constexpr float kingpin_to_axles = 13.825F;    // to the middle of the rear tandem
inline constexpr float fifth_wheel = -2.495F;         // the tractor's fifth wheel from its centre (z)
inline constexpr float fifth_wheel_height = 1.18F;
inline constexpr const char* box_trailer = "assets/verda/vehicles/runtime/trailer_box.glb";
inline constexpr const char* cattle_trailer = "assets/verda/vehicles/runtime/trailer_cattle.glb";

// What a stop is (FreightStop::ranch when not a ranch's index).
inline constexpr int stop_town = -1, stop_stockyard = -2, stop_cattle_quay = -3, stop_cargo_quay = -4, stop_depot = -5;

struct FreightStop {
    std::string name;     // town or ranch
    Vector3 point{};      // on the road
    float at{0};          // metres along the loop where the rig stops (its centre)
    int ranch{stop_town}; // a ranch's index, or what else it is (stop_*)
};

struct Rig {
    std::string vehicle;          // the tractor (VehicleSystem id)
    std::string driver;           // character at the wheel
    std::string trailer;          // model path
    bool cattle{false};
    world::roads::Route loop;
    std::vector<FreightStop> stops;
    float progress{0}, speed{0}, dwell{0};
    float waited{0};              // seconds held up behind something
    float passing{0};             // seconds left edging past a stopped car
    int next_stop{0};
    bool running{false}, blocked{false}, called{false};
    int head{0};                  // cattle aboard
    std::vector<int> aboard;      // ...by ranch
    int first_stop{0};            // where the run starts
    int deliveries{0};
    Vector3 position{};
    float yaw{0};
};

struct TrailerPose {
    std::string vehicle;          // the tractor
    std::string model;
    Vector3 centre{};             // on the ground under the trailer's middle
    float yaw{0}, pitch{0};       // degrees
};

struct RigDriver { std::string vehicle, character; bool hat{false}; };

class Freight {
public:
    static constexpr float cruise = 15, town_speed = 9, corner_speed = 4.5F, accel = .9F, brake = 2.2F;
    static constexpr float town_dwell = 25, ranch_dwell = 28, stockyard_dwell = 30;

    // Finds the rigs; the supply run follows the bus loop, the cattle run is laid between the
    // ranches' gates and the stockyard. False if there is nothing to run.
    bool build(const world::VerdaRegion& region, const vehicles::VehicleSystem& cars, const transit::BusLine& buses,
               const ranch::Ranches* ranches, const characters::CharacterRegistry* characters, std::uint64_t seed);
    void start(world::VerdaRegion& region, vehicles::VehicleSystem& cars);
    void stop(world::VerdaRegion& region, vehicles::VehicleSystem& cars);
    void update(float dt, world::VerdaRegion& region, vehicles::VehicleSystem& cars, Vector3 player,
                const characters::NpcSystem* npcs, ranch::Ranches* ranches);

    // Every semi's trailer, wherever the tractor is (parked, on a run or driven by the player).
    void follow_trailers(const world::VerdaRegion& region, const vehicles::VehicleSystem& cars);
    [[nodiscard]] std::vector<TrailerPose> trailers() const;
    // Dropping and hitching (the player, at the wheel of a rig, stopped): a dropped trailer stays
    // where it was left; back the tractor's fifth wheel up under its nose (within hitch_reach,
    // lined up within hitch_angle) to hitch it again. Returns what happened, for the screen.
    static constexpr float hitch_reach = 3.5F, hitch_angle = 35;
    std::string drop_or_hitch(const world::VerdaRegion& region, const vehicles::VehicleSystem& cars, const std::string& vehicle);
    [[nodiscard]] bool hitched(const std::string& vehicle) const;
    // Where a rig's dropped trailer stands (its nose), if it has one.
    [[nodiscard]] bool dropped_at(const std::string& vehicle, Vector3& kingpin) const;
    [[nodiscard]] std::vector<RigDriver> drivers() const;
    [[nodiscard]] const std::vector<Rig>& rigs() const { return rigs_; }
    [[nodiscard]] bool running() const { return running_; }
    [[nodiscard]] int reversals() const { return reversals_; }
    [[nodiscard]] const std::string& message() const { return message_; }
    [[nodiscard]] float message_time() const { return message_time_; }

private:
    void pose(const Rig& rig, float progress, Vector3& centre, float& yaw, float& steering) const;
    float wrap(const Rig& rig, float distance) const;
    void say(std::string text) { message_ = std::move(text); message_time_ = 5; }

    std::vector<Rig> rigs_;
    struct Hitch { std::string model; Vector3 kingpin{}, axles{}; bool placed{false}, dropped{false}; };
    std::map<std::string, Hitch> hitches_;   // per semi
    bool running_{false};
    int reversals_{0};
    std::string message_;
    float message_time_{0};
};

} // namespace outland::game::freight
