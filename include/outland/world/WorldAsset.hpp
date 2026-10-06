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

    Vector3 position{0.0F, 0.0F, 0.0F};
    Vector3 size{1.0F, 1.0F, 1.0F};

    float rotation_y{0.0F};

    Color primary_color{WHITE};
    Color secondary_color{DARKGRAY};

    bool collision{true};
};

}
