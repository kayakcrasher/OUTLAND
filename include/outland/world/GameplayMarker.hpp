#pragma once

#include <raylib.h>

#include <string>

namespace outland::world {

enum class GameplayMarkerType {
    LootSpawn,
    ZombieSpawn,
    NpcSpawn,
    VehicleSpawn
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
};

} // namespace outland::world
