#pragma once
#include "outland/world/roads/RoadGraph.hpp"
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::characters { class NpcSystem; class CharacterRegistry; }
namespace outland::game::vehicles { class VehicleSystem; }

// Linio 1 · Insula Rondo: the island bus line. Its buses (vehicle markers "vehicle_spawn_bus_...")
// run a loop through every town's stop (the Transit kit shelters): Verda, Espera, Porto Luma,
// Suda Haveno, Sovaĝejo, Roka and back, each with a driver at the wheel. They keep to the lane,
// slow for towns and corners, wait at each stop, and stop for anyone or anything in the way.
// Buses are moved kinematically along the road (VehicleSystem::glide), so they never wedge on a
// corner; a bus that is wrecked or taken by the player leaves the line.
namespace outland::game::transit {

struct BusStop {
    std::string asset;   // the shelter
    std::string town;    // the nearest town's name
    Vector3 shelter{};
    Vector3 kerb{};      // where the bus pulls up (on the road)
    float at{0};         // metres along the loop
};

struct Bus {
    std::string vehicle;     // VehicleSystem id
    std::string driver;      // character id at the wheel
    float progress{0};       // metres along the loop (the bus's centre)
    float speed{0};          // m/s
    float dwell{0};          // seconds left at a stop
    int next_stop{0};
    bool running{false};
    bool blocked{false};
    Vector3 position{};
    float yaw{0};
};

struct BusLineConfig {
    float cruise{14};        // m/s on the open road (50 km/h)
    float town_speed{9};     // within 250 m of a stop
    float corner_speed{5};
    float accel{1.2F}, brake{2.6F};
    float dwell{12};         // seconds at each stop
    float front_axle{3.5F}, rear_axle{-3.0F}; // metres from the bus's centre
};

class BusLine {
public:
    void configure(BusLineConfig config) { config_ = config; }
    const BusLineConfig& config() const { return config_; }
    // Finds the stops and the buses and lays out the loop. False if there is no line to run.
    bool build(const world::VerdaRegion& region, const vehicles::VehicleSystem& vehicles,
               const characters::CharacterRegistry* characters, std::uint64_t seed);
    // Puts the buses on the road, spaced evenly round the loop, and runs them.
    void start(world::VerdaRegion& region, vehicles::VehicleSystem& vehicles);
    // Takes the buses off the line and parks them back at the depot.
    void stop(world::VerdaRegion& region, vehicles::VehicleSystem& vehicles);
    void update(float dt, world::VerdaRegion& region, vehicles::VehicleSystem& vehicles, Vector3 player,
                const characters::NpcSystem* npcs);

    const std::vector<BusStop>& stops() const { return stops_; }
    const std::vector<Bus>& buses() const { return buses_; }
    const world::roads::Route& loop() const { return loop_; }
    bool running() const { return running_; }
    int turnarounds() const { return turnarounds_; } // stops where the line turns round a loop

private:
    void pose(Bus& bus, Vector3& centre, float& yaw, float& steering) const;
    float wrap(float distance) const;
    BusLineConfig config_{};
    std::vector<BusStop> stops_;
    std::vector<Bus> buses_;
    world::roads::Route loop_;
    bool running_{false};
    int turnarounds_{0};
};

}
