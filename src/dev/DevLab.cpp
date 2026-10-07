#include "outland/dev/DevLab.hpp"

#include "outland/world/terrain/TerrainHeight.hpp"

#include <cstdio>

namespace outland::dev {

namespace {

Vector3 grounded(
    const float x,
    const float z
) {
    return {
        x,
        world::terrain::TerrainHeight::sample(
            x,
            z
        ) + 1.0F,
        z
    };
}

}

void DevLab::update() {
    if (IsKeyPressed(KEY_ONE)) {
        location_ =
            DevLocation::TrainingGround;

        teleport_requested_ = true;
    }

    if (IsKeyPressed(KEY_TWO)) {
        location_ =
            DevLocation::Espera;

        teleport_requested_ = true;
    }

    if (IsKeyPressed(KEY_THREE)) {
        location_ =
            DevLocation::Forest;

        teleport_requested_ = true;
    }

    if (IsKeyPressed(KEY_FOUR)) {
        location_ =
            DevLocation::OpenField;

        teleport_requested_ = true;
    }

    if (IsKeyPressed(KEY_FIVE)) {
        location_ =
            DevLocation::BuildingTest;

        teleport_requested_ = true;
    }

    if (IsKeyPressed(KEY_SIX)) {
        location_ =
            DevLocation::VehicleTest;

        teleport_requested_ = true;
    }

    if (IsKeyPressed(KEY_SEVEN)) {
        location_ =
            DevLocation::ZombieTest;

        teleport_requested_ = true;
    }
}

Vector3 DevLab::spawn_position() const {
    switch (location_) {
    case DevLocation::TrainingGround:
        return grounded(
            0.0F,
            8.0F
        );

    case DevLocation::Espera:
        return grounded(
            0.0F,
            -60.0F
        );

    case DevLocation::Forest:
        return grounded(
            120.0F,
            -150.0F
        );

    case DevLocation::OpenField:
        return grounded(
            -140.0F,
            -90.0F
        );

    case DevLocation::BuildingTest:
        return grounded(
            20.0F,
            -65.0F
        );

    case DevLocation::VehicleTest:
        return grounded(
            70.0F,
            30.0F
        );

    case DevLocation::ZombieTest:
        return grounded(
            -80.0F,
            -160.0F
        );
    }

    return grounded(
        0.0F,
        8.0F
    );
}

bool DevLab::teleport_requested() const {
    return teleport_requested_;
}

void DevLab::clear_teleport() {
    teleport_requested_ = false;
}

void DevLab::draw_overlay(
    const Vector3 player_position,
    const float yaw,
    const float pitch,
    const bool grounded_state
) const {
    DrawRectangle(
        8,
        70,
        330,
        170,
        Fade(
            BLACK,
            0.58F
        )
    );

    DrawText(
        "OUTLAND DEV LAB",
        18,
        80,
        20,
        YELLOW
    );

    char buffer[160];

    std::snprintf(
        buffer,
        sizeof(buffer),
        "XYZ: %.2f  %.2f  %.2f",
        player_position.x,
        player_position.y,
        player_position.z
    );

    DrawText(
        buffer,
        18,
        108,
        18,
        RAYWHITE
    );

    std::snprintf(
        buffer,
        sizeof(buffer),
        "YAW %.2f   PITCH %.2f",
        yaw,
        pitch
    );

    DrawText(
        buffer,
        18,
        132,
        18,
        RAYWHITE
    );

    DrawText(
        grounded_state
            ? "GROUND: YES"
            : "GROUND: NO",
        18,
        156,
        18,
        grounded_state
            ? GREEN
            : RED
    );

    DrawText(
        "1 TRAIN  2 ESPERA  3 FOREST  4 FIELD",
        18,
        182,
        14,
        LIGHTGRAY
    );

    DrawText(
        "5 BUILD  6 VEHICLE  7 ZOMBIE",
        18,
        202,
        14,
        LIGHTGRAY
    );
    DrawText("T RESET TARGETS  /  UNLIMITED AMMO",18,220,14,YELLOW);
}

} // namespace outland::dev
