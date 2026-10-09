#pragma once

#include <raylib.h>

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
};

}
