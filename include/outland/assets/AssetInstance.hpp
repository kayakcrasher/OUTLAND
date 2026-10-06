#pragma once

#include <raylib.h>

#include <string>

namespace outland::assets {

struct AssetInstance {
    std::string asset_id;

    Vector3 position{
        0.0F,
        0.0F,
        0.0F
    };

    Vector3 scale{
        1.0F,
        1.0F,
        1.0F
    };

    float rotation_y{0.0F};

    bool enabled{true};
};

} // namespace outland::assets
