#pragma once
// Verda residential houses: one detached two-storey house with a front porch and garage, taken
// from the residential scene (assets/verda/residential/houses/source) by tools/extract_house.py
// and painted in four schemes. Footprint is width (X), depth (Z) and height in metres; the front
// porch faces -Z.
#include "outland/creator/CreatorAssetRegistry.hpp"
#include <array>
namespace outland::creator {
inline void register_residential_assets(CreatorAssetRegistry& registry) {
    struct Piece { const char* id; const char* name; };
    static constexpr std::array<Piece, 4> pieces{{
        {"white", "House, white clapboard"}, {"blue", "House, blue clapboard"},
        {"sage", "House, sage clapboard"}, {"cream", "House, cream clapboard"},
    }};
    for (const auto& p : pieces)
        (void)registry.add({.id = std::string("house_") + p.id, .name = p.name, .category = CreatorAssetCategory::Building,
            .model_path = std::string("assets/verda/residential/houses/house_") + p.id + ".glb", .thumbnail_path = "",
            .footprint = {14.42F, 20.17F, 9.03F}, .placement = {}, .default_scale = 1,
            .tags = {"verda", "residential", "house", "home", "suburb", "domo"}});
}
}
