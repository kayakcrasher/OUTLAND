#include "outland/game/economy/Penthouse.hpp"
#include <algorithm>
#include <cmath>

namespace outland::game::economy {

bool Penthouse::locate(const world::VerdaRegion& region) {
    found_ = false;
    for (const auto& town : region.settlements())
        for (const auto& asset : town.assets)
            if (asset.model_path == model) {
                origin_ = asset.position;
                yaw_ = asset.rotation_y;
                found_ = true;
                return true;
            }
    return false;
}

// The same placement the world kit and the renderer use: a model's +X turns toward (cos, -sin).
Vector3 Penthouse::to_world(Vector3 local) const {
    const float a = yaw_ * DEG2RAD;
    return {origin_.x + local.x * std::cos(a) + local.z * std::sin(a), origin_.y + local.y,
            origin_.z - local.x * std::sin(a) + local.z * std::cos(a)};
}

Vector3 Penthouse::to_local(Vector3 world) const {
    const float a = yaw_ * DEG2RAD, dx = world.x - origin_.x, dz = world.z - origin_.z;
    return {dx * std::cos(a) - dz * std::sin(a), world.y - origin_.y, dx * std::sin(a) + dz * std::cos(a)};
}

bool Penthouse::inside(Vector3 feet) const {
    if (!found_) return false;
    const auto p = to_local(feet);
    return std::abs(p.x) < half_x && std::abs(p.z) < half_z && p.y > floor_y - .6F && p.y < floor_y + 2.5F;
}

bool Penthouse::near_bed(Vector3 feet, float reach) const {
    if (!inside(feet)) return false;
    const auto p = to_local(feet);
    return std::hypot(p.x - bed.x, p.z - bed.z) < reach;
}

bool Penthouse::owned(const EconomyState& wallet) {
    return std::find(wallet.properties.begin(), wallet.properties.end(), property_id) != wallet.properties.end();
}

std::string Penthouse::buy(EconomyState& wallet) {
    if (owned(wallet)) return "The penthouse is already yours";
    if (wallet.money < price) return "The penthouse costs " + std::to_string(price) + " kronoj - you have " + std::to_string(wallet.money);
    wallet.money -= price;
    wallet.properties.emplace_back(property_id);
    return "Bought the Stela Turo penthouse! Welcome home.";
}

} // namespace outland::game::economy
