#pragma once

#include <cstddef>

namespace outland::creator {

enum class CreatorTool {
    Place,
    Delete,
    Move,
    Rotate
};

struct CreatorState {
    // Creator Mode is an internal development tool.
    bool enabled{false};

    // Minecraft-style free-flight while building Verda.
    bool flying{true};
    bool noclip{true};

    // Current editing tool.
    CreatorTool tool{CreatorTool::Place};

    // Selected entry in the Creator asset catalog.
    std::size_t selected_asset{0};

    // Rotation applied to the placement ghost / next asset.
    float placement_yaw{0.0F};

    // Distance from the camera used when positioning the ghost.
    float placement_distance{8.0F};

    // Placement helpers.
    bool snap_to_ground{true};
    float placement_height{0.0F};
    float grid_step{0.0F};
    bool show_collision{false};
    bool show_grid{false};

    // Editor history requests.
    bool request_undo{false};
    bool request_redo{false};

    void rotate(float degrees) {
        placement_yaw += degrees;

        while (placement_yaw >= 360.0F) {
            placement_yaw -= 360.0F;
        }

        while (placement_yaw < 0.0F) {
            placement_yaw += 360.0F;
        }
    }

    void reset_tool_requests() {
        request_undo = false;
        request_redo = false;
    }
};

} // namespace outland::creator
