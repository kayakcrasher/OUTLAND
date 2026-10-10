#pragma once

#include <raylib.h>

#include "outland/world/BuildingPurpose.hpp"
#include <string>

namespace outland::world {

enum class BuildingStyle {
    RuralHouse,
    TwoStoryHouse,
    Shop,
    Garage,
    Warehouse
};

struct Building {
    std::string id;

    BuildingStyle style{
        BuildingStyle::RuralHouse
    };

    Vector3 position{
        0.0F,
        0.0F,
        0.0F
    };

    Vector3 size{
        8.0F,
        4.0F,
        8.0F
    };

    float rotation_y{0.0F};

    Color wall_color{
        210,
        195,
        165,
        255
    };

    Color roof_color{
        90,
        70,
        55,
        255
    };

    bool enterable{true};
    BuildingPurpose purpose{BuildingPurpose::Auto}; // authored in the Creator; Auto = designated by island life
};

}
