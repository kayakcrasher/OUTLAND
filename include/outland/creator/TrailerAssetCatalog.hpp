#pragma once
// Trailer Park pack (El Bolillo Duro, CC0): six single-wide mobile homes, about 6 x 15 m.
// The long sides run along local Z; the front door and porch steps are on the +X side.
// Textures are embedded PNG (tools/convert_glb_jpeg_to_png.py) so every raylib build draws them.
#include "outland/creator/CreatorAssetRegistry.hpp"
#include <array>
namespace outland::creator {
inline void register_trailer_assets(CreatorAssetRegistry& registry) {
    struct Trailer { const char* id; const char* name; float width, depth, height; };
    static constexpr std::array<Trailer, 6> trailers{{
        {"trailer_01", "Trailer Home 1 (white, porch)", 6.21F, 15.11F, 5.70F},
        {"trailer_02", "Trailer Home 2 (white, railed deck)", 6.35F, 15.11F, 5.85F},
        {"trailer_03", "Trailer Home 3 (wood siding, deck)", 6.36F, 15.12F, 5.64F},
        {"trailer_04", "Trailer Home 4 (covered porch)", 6.46F, 15.25F, 5.65F},
        {"trailer_05", "Trailer Home 5 (wood siding)", 5.51F, 15.11F, 5.72F},
        {"trailer_06", "Trailer Home 6 (trimmed)", 5.67F, 15.03F, 5.17F},
    }};
    for (const auto& t : trailers)
        (void)registry.add({.id = std::string("residential_") + t.id, .name = t.name, .category = CreatorAssetCategory::Building,
            .model_path = std::string("assets/verda/residential/trailers/") + t.id + ".glb", .thumbnail_path = "",
            .footprint = {t.width, t.depth, t.height}, .placement = {}, .default_scale = 1,
            .tags = {"verda", "residential", "trailer", "mobile home", "trailer park", "home"}});
}
}
