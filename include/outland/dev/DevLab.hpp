#pragma once

#include <raylib.h>

namespace outland::dev {

enum class DevLocation {
    TrainingGround,
    Espera,
    Forest,
    OpenField,
    BuildingTest,
    VehicleTest,
    ZombieTest
};

class DevLab {
public:
    void update();

    void draw_overlay(
        Vector3 player_position,
        float yaw,
        float pitch,
        bool grounded
    ) const;

    [[nodiscard]]
    Vector3 spawn_position() const;

    [[nodiscard]]
    bool teleport_requested() const;

    void clear_teleport();

private:
    DevLocation location_{
        DevLocation::TrainingGround
    };

    bool teleport_requested_{
        false
    };
};

} // namespace outland::dev
