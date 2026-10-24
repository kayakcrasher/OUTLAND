#include "outland/world/MotorTrade.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include <cmath>
#include <string_view>

namespace outland::world::motor {

Vector3 Site::to_world(Vector3 l) const {
    const float a = yaw * DEG2RAD;
    return {origin.x + l.x * std::cos(a) + l.z * std::sin(a), origin.y + l.y, origin.z - l.x * std::sin(a) + l.z * std::cos(a)};
}

Vector3 Site::to_local(Vector3 w) const {
    const float a = yaw * DEG2RAD, dx = w.x - origin.x, dz = w.z - origin.z;
    return {dx * std::cos(a) - dz * std::sin(a), w.y - origin.y, dx * std::sin(a) + dz * std::cos(a)};
}

std::vector<Site> sites(const VerdaRegion& region) {
    std::vector<Site> out;
    for (const auto& town : region.settlements())
        for (const auto& asset : town.assets) {
            Kind kind;
            if (asset.model_path == gas_model) kind = Kind::Gas;
            else if (asset.model_path == mechanic_model) kind = Kind::Mechanic;
            else if (asset.model_path == car_lot_model) kind = Kind::CarLot;
            else if (asset.model_path == depot_model) kind = Kind::Depot;
            else continue;
            // The world kit placed the model's bounds centre (and its base); back to its own origin.
            Site site{kind, asset.id, asset.position, asset.rotation_y};
            if (const auto* mesh = physics::MeshCollisionLibrary::get(asset.model_path)) {
                const float cx = (mesh->source_min.x + mesh->source_max.x) * .5F, cz = (mesh->source_min.z + mesh->source_max.z) * .5F;
                const float a = asset.rotation_y * DEG2RAD;
                site.origin.x -= cx * std::cos(a) + cz * std::sin(a);
                site.origin.z -= -cx * std::sin(a) + cz * std::cos(a);
                site.origin.y -= mesh->source_min.y;
            }
            out.push_back(std::move(site));
        }
    return out;
}

bool at_pump(const Site& gas, Vector3 car) {
    const Vector3 p = gas.to_local(car);
    if (p.z < gas_pumps_z0 - 3 || p.z > gas_pumps_z1 + 3.3F) return false;   // anywhere under the canopy, beside an island
    for (const float x : gas_islands) if (std::abs(p.x - x) < 3.4F) return true;
    return false;
}

int bay_at(const Site& shop, Vector3 car) {
    const Vector3 p = shop.to_local(car);
    if (shop.kind == Kind::Depot) {
        if (p.z < depot_bay_z0 || p.z > depot_bay_z1) return -1;
        for (std::size_t i = 0; i < depot_bays.size(); ++i)
            if (std::abs(p.x - depot_bays[i]) < depot_bay_half) return static_cast<int>(i);
        return -1;
    }
    if (p.z < mechanic_bay_z0 || p.z > mechanic_bay_z1) return -1;
    for (std::size_t i = 0; i < mechanic_bays.size(); ++i)
        if (std::abs(p.x - mechanic_bays[i]) < mechanic_bay_half) return static_cast<int>(i);
    return -1;
}

} // namespace outland::world::motor
