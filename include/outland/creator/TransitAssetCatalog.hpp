#pragma once
// Verda Transit / Roadside Kit (Elbolilloduro "Bus Stop", CC0 models): bus shelters, stop signs,
// tall street lamps and a concrete pad. Textures are embedded PNG. Footprints are width (X),
// depth (Z) and height in metres; the shelters face +Z.
#include "outland/creator/CreatorAssetRegistry.hpp"
#include <array>
namespace outland::creator {
inline void register_transit_assets(CreatorAssetRegistry& registry) {
    struct Piece { const char* id; const char* name; CreatorAssetCategory category; float width, depth, height; };
    static constexpr std::array<Piece, 8> pieces{{
        {"bus_stop", "Bus Shelter (transit)", CreatorAssetCategory::Prop, 4.17F, 1.36F, 3.19F},
        {"bus_stop_alt", "Bus Shelter, wide (transit)", CreatorAssetCategory::Prop, 4.15F, 1.96F, 3.52F},
        {"bus_stop_sign", "Bus Stop Sign", CreatorAssetCategory::Prop, .5F, .07F, 2.64F},
        {"bus_stop_sign_01", "Bus Stop Sign 2", CreatorAssetCategory::Prop, .59F, .07F, 2.85F},
        {"bus_stop_sign_02", "Bus Stop Sign 3", CreatorAssetCategory::Prop, .67F, .08F, 2.88F},
        {"lamppost", "Street Lamp, double (transit)", CreatorAssetCategory::Prop, 1.05F, 3.47F, 10.52F},
        {"lamppost_01", "Street Lamp (transit)", CreatorAssetCategory::Prop, .43F, 3.47F, 10.52F},
        {"concrete", "Concrete Kerb Pad", CreatorAssetCategory::Road, 2.0F, .81F, .06F},
    }};
    for (const auto& p : pieces)
        (void)registry.add({.id = std::string("transit_") + p.id, .name = p.name, .category = p.category,
            .model_path = std::string("assets/verda/transit/") + p.id + ".glb", .thumbnail_path = "",
            .footprint = {p.width, p.depth, p.height}, .placement = {}, .default_scale = 1,
            .tags = {"verda", "transit", "bus stop", "bus", "haltejo"}});
}
}
