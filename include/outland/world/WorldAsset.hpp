#pragma once

#include <raylib.h>

#include "outland/world/BuildingPurpose.hpp"
#include <string>
#include <array>

namespace outland::world {

enum class AssetType {
    House,
    Shop,
    Garage,
    Warehouse,
    Wall,
    Road,
    Tree,
    Rock,
    UtilityPole,
    Sign
};

struct VehiclePlacementState {
    std::string definition, marker;
    Vector3 home{};
    float home_yaw{0};
    float health{100}, engine{100}, fuel{1};
    std::array<float,4> tires{{100,100,100,100}};
    bool enabled{true}, destroyed{false};
};
struct WorldAsset {
    std::string id;
    AssetType type{AssetType::House};

    // Optional render model used by data-driven world assets.
    // Empty keeps the existing procedural rendering path.
    std::string model_path;

    Vector3 position{0.0F, 0.0F, 0.0F};
    Vector3 size{1.0F, 1.0F, 1.0F};

    float rotation_y{0.0F};

    Color primary_color{WHITE};
    Color secondary_color{DARKGRAY};

    bool collision{true};
    VehiclePlacementState vehicle{};
    BuildingPurpose purpose{BuildingPurpose::Auto}; // authored use when this model is a building
};

// Model assets big enough to be a building on their own (a whole house, shop, tower or shed) can
// carry a purpose; walls, slabs and props cannot - tag part-built buildings with a purpose marker.
inline bool asset_can_have_purpose(const WorldAsset& asset) {
    if(asset.model_path.empty() || !asset.vehicle.definition.empty()) return false;
    if(asset.model_path.find("/city/main_street/")!=std::string::npos || asset.model_path.find("/city/towers/")!=std::string::npos) return true;
    return asset.size.x*asset.size.z>=24.0F && asset.size.x>=3.5F && asset.size.z>=3.5F && asset.size.y>=2.5F;
}

}
