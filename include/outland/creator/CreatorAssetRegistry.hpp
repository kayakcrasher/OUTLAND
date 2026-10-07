#pragma once

#ifdef OUTLAND_DEV_TOOLS

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace outland::creator {

// ============================================================
// CREATOR ASSET CATEGORIES
// ============================================================

enum class CreatorAssetCategory {
    Building,
    BuildingPart,
    Nature,
    Road,
    Prop,
    Vehicle,
    Gameplay
};

[[nodiscard]]
constexpr std::string_view category_name(
    const CreatorAssetCategory category
) {
    switch (category) {
        case CreatorAssetCategory::Building:
            return "BUILDINGS";

        case CreatorAssetCategory::BuildingPart:
            return "PARTS";

        case CreatorAssetCategory::Nature:
            return "NATURE";

        case CreatorAssetCategory::Road:
            return "ROADS";

        case CreatorAssetCategory::Prop:
            return "PROPS";

        case CreatorAssetCategory::Vehicle:
            return "VEHICLES";

        case CreatorAssetCategory::Gameplay:
            return "GAMEPLAY";
    }

    return "UNKNOWN";
}

// ============================================================
// PLACEMENT METADATA
// ============================================================

struct CreatorFootprint {
    float width{1.0F};
    float depth{1.0F};
    float height{1.0F};
};

struct CreatorPlacementRules {
    bool placeable{true};
    bool rotatable{true};

    bool snap_to_ground{true};
    bool align_to_surface{false};

    bool allow_overlap{false};

    float ground_offset{0.0F};

    // Rotation step used by Creator's rotate buttons.
    float rotation_step{15.0F};
};

// ============================================================
// CREATOR ASSET
// ============================================================

struct CreatorAssetDefinition {
    std::string id;
    std::string name;

    CreatorAssetCategory category{
        CreatorAssetCategory::Prop
    };

    // Production asset location.
    //
    // Empty is legal for procedural OUTLAND assets such as
    // the current VerdanArchitecture buildings.
    std::string model_path;

    // Optional image used by the future asset drawer.
    std::string thumbnail_path;

    CreatorFootprint footprint{};

    CreatorPlacementRules placement{};

    float default_scale{1.0F};

    // Search/filter metadata.
    std::vector<std::string> tags;
};

// ============================================================
// ASSET REGISTRY
//
// Single source of truth for everything Creator can place.
//
// Later:
// assets/catalog/*.json
//          ↓
// CreatorAssetRegistry
//          ↓
// Inventory / Hotbar / Preview / Placement
// ============================================================

class CreatorAssetRegistry {
public:
    CreatorAssetRegistry() {
        register_builtin_assets();
    }

    [[nodiscard]]
    const std::vector<CreatorAssetDefinition>&
    assets() const {
        return assets_;
    }

    [[nodiscard]]
    std::size_t size() const {
        return assets_.size();
    }

    [[nodiscard]]
    bool empty() const {
        return assets_.empty();
    }

    [[nodiscard]]
    const CreatorAssetDefinition*
    find(
        const std::string_view id
    ) const {
        for (const auto& asset : assets_) {
            if (asset.id == id) {
                return &asset;
            }
        }

        return nullptr;
    }

    [[nodiscard]]
    std::vector<const CreatorAssetDefinition*>
    category(
        const CreatorAssetCategory wanted
    ) const {
        std::vector<const CreatorAssetDefinition*> result;

        for (const auto& asset : assets_) {
            if (asset.category == wanted) {
                result.push_back(&asset);
            }
        }

        return result;
    }

    [[nodiscard]]
    bool add(
        CreatorAssetDefinition asset
    ) {
        if (asset.id.empty() || asset.name.empty()) {
            return false;
        }

        if (find(asset.id) != nullptr) {
            return false;
        }

        assets_.push_back(
            std::move(asset)
        );

        return true;
    }

private:
    std::vector<CreatorAssetDefinition> assets_;

    void register_builtin_assets() {

        // ====================================================
        // CURRENT OUTLAND BUILDINGS
        // ====================================================

        add({
            .id = "rural_house",
            .name = "Rural House",
            .category = CreatorAssetCategory::Building,
            .model_path = "",
            .thumbnail_path = "",
            .footprint = {8.0F, 7.0F, 4.0F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "house",
                "rural",
                "verda",
                "residential"
            }
        });

        add({
            .id = "two_story_house",
            .name = "Two-Story House",
            .category = CreatorAssetCategory::Building,
            .model_path = "",
            .thumbnail_path = "",
            .footprint = {9.0F, 9.0F, 7.0F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "house",
                "two-story",
                "verda",
                "residential"
            }
        });

        add({
            .id = "shop",
            .name = "Shop",
            .category = CreatorAssetCategory::Building,
            .model_path = "",
            .thumbnail_path = "",
            .footprint = {11.0F, 8.0F, 4.5F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "shop",
                "commercial",
                "verda"
            }
        });

        add({
            .id = "garage",
            .name = "Garage",
            .category = CreatorAssetCategory::Building,
            .model_path = "",
            .thumbnail_path = "",
            .footprint = {12.0F, 10.0F, 5.0F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "garage",
                "utility",
                "verda"
            }
        });

        // ====================================================
        // CURRENT OUTLAND NATURE
        // ====================================================

        add({
            .id = "oak_tree",
            .name = "Oak Tree",
            .category = CreatorAssetCategory::Nature,
            .model_path =
                "assets/verda/starter/tree_oak_lod0.obj",
            .thumbnail_path = "",
            .footprint = {4.0F, 4.0F, 9.0F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "tree",
                "oak",
                "forest"
            }
        });

        add({
            .id = "tall_tree",
            .name = "Tall Tree",
            .category = CreatorAssetCategory::Nature,
            .model_path =
                "assets/verda/starter/tree_tall_lod0.obj",
            .thumbnail_path = "",
            .footprint = {3.0F, 3.0F, 12.0F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "tree",
                "tall",
                "forest"
            }
        });

        add({
            .id = "round_bush",
            .name = "Round Bush",
            .category = CreatorAssetCategory::Nature,
            .model_path =
                "assets/verda/starter/bush_round_lod0.obj",
            .thumbnail_path = "",
            .footprint = {2.0F, 2.0F, 1.5F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "bush",
                "foliage"
            }
        });

        add({
            .id = "scrub_bush",
            .name = "Scrub Bush",
            .category = CreatorAssetCategory::Nature,
            .model_path =
                "assets/verda/starter/bush_scrub_lod0.obj",
            .thumbnail_path = "",
            .footprint = {2.5F, 2.5F, 1.2F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "bush",
                "scrub",
                "foliage"
            }
        });

        // ====================================================
        // PROPS
        // ====================================================

        add({
            .id = "rock",
            .name = "Rock",
            .category = CreatorAssetCategory::Nature,
            .model_path = "",
            .thumbnail_path = "",
            .footprint = {2.0F, 2.0F, 1.5F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "rock",
                "stone",
                "terrain"
            }
        });

        add({
            .id = "wood_fence",
            .name = "Wood Fence",
            .category = CreatorAssetCategory::Prop,
            .model_path = "",
            .thumbnail_path = "",
            .footprint = {3.0F, 0.4F, 1.4F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "fence",
                "wood",
                "barrier"
            }
        });

        // ====================================================
        // CREATOR GAMEPLAY MARKERS
        // ====================================================

        add({
            .id = "loot_spawn",
            .name = "Loot Spawn",
            .category = CreatorAssetCategory::Gameplay,
            .model_path = "",
            .thumbnail_path = "",
            .footprint = {0.5F, 0.5F, 0.5F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "loot",
                "spawn",
                "gameplay"
            }
        });

        add({
            .id = "zombie_spawn",
            .name = "Zombie Spawn",
            .category = CreatorAssetCategory::Gameplay,
            .model_path = "",
            .thumbnail_path = "",
            .footprint = {0.8F, 0.8F, 1.8F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "zombie",
                "spawn",
                "gameplay"
            }
        });

        add({
            .id = "npc_spawn",
            .name = "NPC Spawn",
            .category = CreatorAssetCategory::Gameplay,
            .model_path = "",
            .thumbnail_path = "",
            .footprint = {0.8F, 0.8F, 1.8F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "npc",
                "spawn",
                "gameplay"
            }
        });

        add({
            .id = "vehicle_spawn",
            .name = "Vehicle Spawn",
            .category = CreatorAssetCategory::Gameplay,
            .model_path = "",
            .thumbnail_path = "",
            .footprint = {3.0F, 6.0F, 2.5F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "vehicle",
                "spawn",
                "gameplay"
            }
        });
    }
};

} // namespace outland::creator

#endif // OUTLAND_DEV_TOOLS
