#pragma once

#ifdef OUTLAND_DEV_TOOLS

#include "outland/creator/CreatorAssetRegistry.hpp"

namespace outland::creator {

inline void register_industrial_assets(
    CreatorAssetRegistry& registry
) {
    (void)registry.add({
        .id = "industrial_barrel",
        .name = "Industrial Barrel",
        .category = CreatorAssetCategory::Prop,
        .model_path = "assets/verda/industrial/shacks/Barrel.glb",
        .thumbnail_path = "",
        .footprint = {0.8000F, 0.6928F, 0.9000F},
        .placement = {},
        .default_scale = 1.0F,
        .tags = {"industrial", "shacks", "prop"}
    });

    (void)registry.add({
        .id = "industrial_chainlink_fence",
        .name = "Chainlink Fence",
        .category = CreatorAssetCategory::BuildingPart,
        .model_path = "assets/verda/industrial/shacks/Chainlink Fence.glb",
        .thumbnail_path = "",
        .footprint = {2.4608F, 0.1200F, 2.5000F},
        .placement = {},
        .default_scale = 1.0F,
        .tags = {"industrial", "shacks", "fence"}
    });

    (void)registry.add({
        .id = "industrial_concrete_barricade",
        .name = "Concrete Barricade",
        .category = CreatorAssetCategory::Prop,
        .model_path = "assets/verda/industrial/shacks/Concrete Barricade.glb",
        .thumbnail_path = "",
        .footprint = {2.0000F, 0.5000F, 1.0000F},
        .placement = {},
        .default_scale = 1.0F,
        .tags = {"industrial", "shacks", "barrier"}
    });

    (void)registry.add({
        .id = "industrial_concrete_ramps",
        .name = "Concrete Ramps",
        .category = CreatorAssetCategory::BuildingPart,
        .model_path = "assets/verda/industrial/shacks/Concrete Ramps.glb",
        .thumbnail_path = "",
        .footprint = {13.0000F, 12.0000F, 2.0000F},
        .placement = {},
        .default_scale = 1.0F,
        .tags = {"industrial", "shacks", "structure"}
    });

    (void)registry.add({
        .id = "industrial_handrail",
        .name = "Handrail",
        .category = CreatorAssetCategory::BuildingPart,
        .model_path = "assets/verda/industrial/shacks/Handrail.glb",
        .thumbnail_path = "",
        .footprint = {1.0250F, 0.0500F, 0.7500F},
        .placement = {},
        .default_scale = 1.0F,
        .tags = {"industrial", "shacks", "railing"}
    });

    (void)registry.add({
        .id = "industrial_large_shed_a",
        .name = "Large Shed A",
        .category = CreatorAssetCategory::BuildingPart,
        .model_path = "assets/verda/industrial/shacks/Large Shed A.glb",
        .thumbnail_path = "",
        .footprint = {8.8000F, 14.3220F, 6.2010F},
        .placement = {},
        .default_scale = 1.0F,
        .tags = {"industrial", "shacks", "shed"}
    });

    (void)registry.add({
        .id = "industrial_large_shed_b",
        .name = "Large Shed B",
        .category = CreatorAssetCategory::BuildingPart,
        .model_path = "assets/verda/industrial/shacks/Large Shed B.glb",
        .thumbnail_path = "",
        .footprint = {8.8000F, 14.3220F, 5.5010F},
        .placement = {},
        .default_scale = 1.0F,
        .tags = {"industrial", "shacks", "shed"}
    });

    (void)registry.add({
        .id = "industrial_leanto_a",
        .name = "Lean-to A",
        .category = CreatorAssetCategory::BuildingPart,
        .model_path = "assets/verda/industrial/shacks/Leanto A.glb",
        .thumbnail_path = "",
        .footprint = {3.5704F, 2.1247F, 3.0621F},
        .placement = {},
        .default_scale = 1.0F,
        .tags = {"industrial", "shacks", "shelter"}
    });

    (void)registry.add({
        .id = "industrial_leanto_b",
        .name = "Lean-to B",
        .category = CreatorAssetCategory::BuildingPart,
        .model_path = "assets/verda/industrial/shacks/Leanto B.glb",
        .thumbnail_path = "",
        .footprint = {3.2852F, 6.2493F, 3.0016F},
        .placement = {},
        .default_scale = 1.0F,
        .tags = {"industrial", "shacks", "shelter"}
    });

    (void)registry.add({
        .id = "industrial_metal_gantry_floor",
        .name = "Metal Gantry Floor",
        .category = CreatorAssetCategory::BuildingPart,
        .model_path = "assets/verda/industrial/shacks/Metal Gantry Floor.glb",
        .thumbnail_path = "",
        .footprint = {1.0000F, 1.0000F, 0.0500F},
        .placement = {},
        .default_scale = 1.0F,
        .tags = {"industrial", "shacks", "gantry"}
    });

    (void)registry.add({
        .id = "industrial_pallet",
        .name = "Wood Pallet",
        .category = CreatorAssetCategory::Prop,
        .model_path = "assets/verda/industrial/shacks/Pallet.glb",
        .thumbnail_path = "",
        .footprint = {0.9000F, 1.2000F, 0.1000F},
        .placement = {},
        .default_scale = 1.0F,
        .tags = {"industrial", "shacks", "prop"}
    });

    (void)registry.add({
        .id = "industrial_shack_a",
        .name = "Shack A",
        .category = CreatorAssetCategory::BuildingPart,
        .model_path = "assets/verda/industrial/shacks/Shack A.glb",
        .thumbnail_path = "",
        .footprint = {3.5704F, 6.2493F, 3.0621F},
        .placement = {},
        .default_scale = 1.0F,
        .tags = {"industrial", "shacks", "shack"}
    });

    (void)registry.add({
        .id = "industrial_stairs",
        .name = "Industrial Stairs",
        .category = CreatorAssetCategory::BuildingPart,
        .model_path = "assets/verda/industrial/shacks/Stairs.glb",
        .thumbnail_path = "",
        .footprint = {1.2000F, 0.9750F, 1.0000F},
        .placement = {},
        .default_scale = 1.0F,
        .tags = {"industrial", "shacks", "stairs"}
    });
}

} // namespace outland::creator

#endif
