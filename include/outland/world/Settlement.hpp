#pragma once

#include "outland/world/Building.hpp"
#include "outland/world/GameplayMarker.hpp"
#include "outland/world/Road.hpp"
#include "outland/world/WorldAsset.hpp"

#include <raylib.h>

#include <string>
#include <vector>

namespace outland::world {

enum class SettlementState {
    Peaceful,
    Abandoned,
    Hostile,
    Contested,
    Infected,
    Overrun
};

struct Settlement {
    std::string id;
    std::string name;

    Vector3 center{
        0.0F,
        0.0F,
        0.0F
    };

    SettlementState state{
        SettlementState::Abandoned
    };

    int survivors{0};
    int hostiles{0};
    int infected{0};

    std::vector<Building> buildings;
    std::vector<Road> roads;
    std::vector<WorldAsset> assets;
    std::vector<GameplayMarker> gameplay_markers;
};

}
