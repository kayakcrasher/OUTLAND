#pragma once

#include <raylib.h>

#include <string>

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
};

}
