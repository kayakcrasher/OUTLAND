#pragma once

#include <string>

namespace outland::assets {

enum class AssetCategory {
    Building,
    Vehicle,
    Vegetation,
    Prop,
    Infrastructure
};

enum class CollisionType {
    None,
    Box,
    Compound,
    Mesh
};

struct AssetDefinition {
    std::string id;
    std::string display_name;

    AssetCategory category{AssetCategory::Prop};
    CollisionType collision{CollisionType::Box};

    std::string model_path;
    std::string lod1_path;
    std::string lod2_path;

    float render_distance{300.0F};
    float lod1_distance{80.0F};
    float lod2_distance{180.0F};

    bool casts_shadow{true};
    bool receives_shadow{true};
    bool enterable{false};
};

} // namespace outland::assets
