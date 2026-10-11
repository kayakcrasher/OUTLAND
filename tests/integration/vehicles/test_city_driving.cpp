// Every street lane in Verda City, its suburbs and Porto Luma can be driven by a car: nothing
// static stands in a lane (street furniture, kerb pads, ground plates, building pieces).
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include <raymath.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
using namespace outland;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}
int main() {
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    std::string error;
    game::vehicles::VehicleRegistry types;
    check(types.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error), error);
    const auto* car = types.find("hatchback");
    check(car != nullptr, "hatchback type");
    game::vehicles::VehicleSystem vehicles(types); // no parked cars spawned: static world only
    vehicles.set_training_structure(false);
    int samples = 0, blocked = 0;
    std::map<std::string, int> culprits;
    for (const auto& town : region.settlements()) {
        if (town.id != "capital_verda" && town.id != "port_luma") continue;
        for (const auto& road : town.roads) {
            const Vector3 d = Vector3Subtract(road.end, road.start);
            const float length = std::hypot(d.x, d.z);
            if (length < 1 || std::max(std::abs(road.start.x - town.center.x), std::abs(road.end.x - town.center.x)) > 520 ||
                std::max(std::abs(road.start.z - town.center.z), std::abs(road.end.z - town.center.z)) > 520) continue;
            const Vector3 u{d.x / length, 0, d.z / length}, side{-u.z, 0, u.x};
            for (const int direction : {1, -1}) {
                const float yaw = std::atan2(u.x * direction, u.z * direction) * RAD2DEG;
                const float lane = road.width * .25F * direction;
                for (float t = 8; t < length - 8; t += 2) {
                    const Vector3 p{road.start.x + u.x * t + side.x * lane, 0, road.start.z + u.z * t + side.z * lane};
                    ++samples;
                    if (!vehicles.blocked(region, p, yaw, *car, 1)) continue;
                    ++blocked;
                    for (const auto& s : region.settlements()) for (const auto& a : s.assets)
                        if (a.collision && std::hypot(a.position.x - p.x, a.position.z - p.z) < 6 + std::max(a.size.x, a.size.z) * .5F &&
                            world::physics::WorldCollision::blocked(p, region, 2.5F, ""))
                            ++culprits[a.model_path.empty() ? "procedural " + a.id.substr(0, 20) : a.model_path];
                    if (blocked <= 12) std::cerr << "  blocked at " << p.x << ", " << p.z << " on " << town.id << " road "
                                                 << road.start.x << "," << road.start.z << " -> " << road.end.x << "," << road.end.z << '\n';
                }
            }
        }
    }
    for (const auto& [model, n] : culprits) std::cerr << "  near: " << model << " x" << n << '\n';
    std::cout << samples << " lane samples, " << blocked << " blocked\n";
    check(samples > 2000, "city streets sampled");
    check(blocked == 0, "every city lane is drivable: " + std::to_string(blocked) + " blocked");

    // Pickup trucks: a real-size vehicle type, parked where the work is, and they drive.
    const auto* truck = types.find("truck");
    check(truck && truck->length > 5.3F && truck->width > 1.9F && truck->wheel_radius >= .4F, "the pickup is a vehicle type");
    game::vehicles::VehicleSystem fleet(types);
    fleet.set_training_structure(false);
    fleet.reconcile(region);
    int trucks = 0, cars = 0;
    for (std::size_t i = 0; i < fleet.vehicles().size(); ++i) {
        const auto* a = fleet.asset(region, fleet.vehicles()[i].id);
        check(a && std::abs(a->size.x - fleet.definition(*a)->width) < .01F, "every vehicle at its real size");
        trucks += a->vehicle.definition == "truck";
        cars += a->vehicle.definition == "hatchback";
    }
    check(trucks >= 5 && cars >= 80, "trucks and cars parked: " + std::to_string(trucks) + "/" + std::to_string(cars));
    int index = -1;
    for (std::size_t i = 0; i < fleet.vehicles().size(); ++i)
        if (fleet.vehicles()[i].id.find("truck_port_luma_1") != std::string::npos) index = static_cast<int>(i);
    check(index >= 0, "a truck on the Porto Luma quay");
    auto* pickup = fleet.asset(region, fleet.vehicles()[static_cast<std::size_t>(index)].id);
    const Vector3 start = pickup->position;
    check(fleet.enter(region, index, Vector3Add(start, {1.5F, 0, 0})), "climb into the pickup");
    for (int i = 0; i < 150; ++i) fleet.update(.02F, {1, 0}, region, pickup->position);
    const float moved = Vector3Distance(start, pickup->position);
    std::cout << "pickup drove " << moved << " m in 3 s\n";
    check(moved > 15 && pickup->vehicle.health == 100, "the pickup drives off down the quay");
}
