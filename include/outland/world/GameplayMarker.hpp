#pragma once

#include <raylib.h>

#include "outland/world/BuildingPurpose.hpp"
#include <string>

namespace outland::world {

enum class GameplayMarkerType {
    LootSpawn,
    ZombieSpawn,
    NpcSpawn,
    VehicleSpawn,
    // Placed just inside a building's front door, facing out: gives that building (or a building
    // assembled from Creator parts, which has no single object) its purpose for island life.
    BuildingPurpose
};

struct GameplayMarker {
    std::string id;

    GameplayMarkerType type{
        GameplayMarkerType::LootSpawn
    };

    Vector3 position{
        0.0F,
        0.0F,
        0.0F
    };

    Vector3 size{
        0.5F,
        0.5F,
        0.5F
    };

    float rotation_y{0.0F};

    bool enabled{true};
    world::BuildingPurpose purpose{world::BuildingPurpose::Auto}; // BuildingPurpose markers only
};

} // namespace outland::world
