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

    void add_imported_asset(
        std::string id,
        std::string name,
        CreatorAssetCategory category,
        std::string model_path,
        CreatorFootprint footprint
    ) {
        CreatorAssetDefinition asset{
            .id = std::move(id),
            .name = std::move(name),
            .category = category,
            .model_path = std::move(model_path),
            .thumbnail_path = "",
            .footprint = footprint,
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "quaternius",
                "downtown",
                "creator"
            }
        };

        (void)add(std::move(asset));
    }

    void register_downtown_creator_pack() {
        const std::string base =
            "assets/verda/creator/downtown/";

        // PARTS
        add_imported_asset(
            "brick_plain_1",
            "Brick Wall",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_Plain_1.gltf",
            {3.0F, 0.4F, 3.0F}
        );

        add_imported_asset(
            "brick_plain_3",
            "Brick Wall Wide",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_Plain_3.gltf",
            {6.0F, 0.4F, 3.0F}
        );

        add_imported_asset(
            "brick_corner_l",
            "Brick Corner L",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_90Angle_L.gltf",
            {3.0F, 3.0F, 3.0F}
        );

        add_imported_asset(
            "brick_corner_r",
            "Brick Corner R",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_90Angle_R.gltf",
            {3.0F, 3.0F, 3.0F}
        );

        add_imported_asset(
            "brick_interior_wall_1",
            "Interior Wall",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_InteriorWall_1.gltf",
            {3.0F, 0.3F, 3.0F}
        );

        add_imported_asset(
            "brick_interior_wall_3",
            "Interior Wall Wide",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_InteriorWall_3.gltf",
            {6.0F, 0.3F, 3.0F}
        );

        add_imported_asset(
            "brick_window_single",
            "Brick Window",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_Window_Square_Single.gltf",
            {3.0F, 0.4F, 3.0F}
        );

        add_imported_asset(
            "brick_window_double",
            "Brick Double Window",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_RedWhite_DoubleWindow.gltf",
            {6.0F, 0.4F, 3.0F}
        );

        add_imported_asset(
            "metal_plain_1",
            "Metal Wall",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Metal_Plain_1.gltf",
            {3.0F, 0.4F, 3.0F}
        );

        add_imported_asset(
            "metal_plain_3",
            "Metal Wall Wide",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Metal_Plain_3.gltf",
            {6.0F, 0.4F, 3.0F}
        );

        add_imported_asset(
            "metal_first_floor_wall",
            "Metal Ground Wall",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Metal_FirstFloor_Wall.gltf",
            {3.0F, 0.4F, 3.0F}
        );

        add_imported_asset(
            "metal_first_floor_window",
            "Metal Ground Window",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Metal_FirstFloor_Window.gltf",
            {3.0F, 0.4F, 3.0F}
        );

        add_imported_asset(
            "metal_window",
            "Metal Window",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Metal_Window.gltf",
            {3.0F, 0.4F, 3.0F}
        );

        // DOORS / FLOORS / ROOFS / ACCESS
        add_imported_asset(
            "doorframe_metal",
            "Metal Door Frame",
            CreatorAssetCategory::BuildingPart,
            base + "parts/DoorFrame_Metal_Single.gltf",
            {2.0F, 0.4F, 3.0F}
        );

        add_imported_asset(
            "doorframe_wood",
            "Wood Door Frame",
            CreatorAssetCategory::BuildingPart,
            base + "parts/DoorFrame_Wooden.gltf",
            {2.0F, 0.4F, 3.0F}
        );

        add_imported_asset(
            "door_1",
            "Door 1",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Door_1.gltf",
            {1.5F, 0.25F, 2.5F}
        );

        add_imported_asset(
            "door_2",
            "Door 2",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Door_2.gltf",
            {1.5F, 0.25F, 2.5F}
        );

        add_imported_asset(
            "door_3",
            "Door 3",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Door_3.gltf",
            {1.5F, 0.25F, 2.5F}
        );

        add_imported_asset(
            "floor_2x2",
            "Floor 2x2",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Floor_2x2.gltf",
            {2.0F, 2.0F, 0.2F}
        );

        add_imported_asset(
            "floor_4x4",
            "Floor 4x4",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Floor_4x4.gltf",
            {4.0F, 4.0F, 0.2F}
        );

        add_imported_asset(
            "roof_2x2",
            "Roof 2x2",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Roof_2x2.gltf",
            {2.0F, 2.0F, 1.0F}
        );

        add_imported_asset(
            "roof_4x4",
            "Roof 4x4",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Roof_4x4.gltf",
            {4.0F, 4.0F, 1.0F}
        );

        add_imported_asset(
            "stairs_entrance",
            "Entrance Stairs",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Stairs_Entrance_Concrete.gltf",
            {3.0F, 3.0F, 1.5F}
        );

        add_imported_asset(
            "entrance_concrete_2x1",
            "Concrete Entrance 2x1",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Entrance_Concrete_2x1.gltf",
            {2.0F, 1.0F, 0.5F}
        );

        add_imported_asset(
            "entrance_concrete_2x2",
            "Concrete Entrance 2x2",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Entrance_Concrete_2x2.gltf",
            {2.0F, 2.0F, 0.5F}
        );

        // ====================================================
        // ROADS
        // ====================================================

        add_imported_asset(
            "street_2lane",
            "Street 2 Lane",
            CreatorAssetCategory::Road,
            base + "roads/Street_2Lane.gltf",
            {6.0F, 12.0F, 0.2F}
        );

        add_imported_asset(
            "street_2lane_nosidewalk",
            "Street 2 Lane Bare",
            CreatorAssetCategory::Road,
            base + "roads/Street_2Lane_noSidewalk.gltf",
            {6.0F, 12.0F, 0.2F}
        );

        add_imported_asset(
            "street_4lane",
            "Street 4 Lane",
            CreatorAssetCategory::Road,
            base + "roads/Street_4Lane.gltf",
            {12.0F, 12.0F, 0.2F}
        );

        add_imported_asset(
            "street_4way",
            "4-Way Intersection",
            CreatorAssetCategory::Road,
            base + "roads/Street_4WayIntersection.gltf",
            {12.0F, 12.0F, 0.2F}
        );

        add_imported_asset(
            "street_t",
            "T Intersection",
            CreatorAssetCategory::Road,
            base + "roads/Street_TIntersection.gltf",
            {12.0F, 12.0F, 0.2F}
        );

        add_imported_asset(
            "street_curve_2lane",
            "Street Curve",
            CreatorAssetCategory::Road,
            base + "roads/Street_Curve_2Lane.gltf",
            {12.0F, 12.0F, 0.2F}
        );

        add_imported_asset(
            "asphalt_6x6",
            "Asphalt 6x6",
            CreatorAssetCategory::Road,
            base + "roads/Street_Asphalt_6x6.gltf",
            {6.0F, 6.0F, 0.2F}
        );

        add_imported_asset(
            "asphalt_9x9",
            "Asphalt 9x9",
            CreatorAssetCategory::Road,
            base + "roads/Street_Asphalt_9x9.gltf",
            {9.0F, 9.0F, 0.2F}
        );

        add_imported_asset(
            "sidewalk_straight",
            "Straight Sidewalk",
            CreatorAssetCategory::Road,
            base + "roads/Sidewalk_Straight_3m.gltf",
            {3.0F, 3.0F, 0.3F}
        );

        add_imported_asset(
            "sidewalk_corner",
            "Sidewalk Corner",
            CreatorAssetCategory::Road,
            base + "roads/Sidewalk_Corner_Flat_3m.gltf",
            {3.0F, 3.0F, 0.3F}
        );

        add_imported_asset(
            "crosswalk",
            "Crosswalk",
            CreatorAssetCategory::Road,
            base + "roads/Decal_Crosswalk.gltf",
            {6.0F, 3.0F, 0.05F}
        );

        add_imported_asset(
            "stop_decal",
            "STOP Road Marking",
            CreatorAssetCategory::Road,
            base + "roads/Decal_Stop.gltf",
            {3.0F, 3.0F, 0.05F}
        );

        // ====================================================
        // CITY PROPS
        // ====================================================

        add_imported_asset(
            "ac_unit",
            "AC Unit",
            CreatorAssetCategory::Prop,
            base + "props/Prop_ACUnit.gltf",
            {1.5F, 1.0F, 1.2F}
        );

        add_imported_asset(
            "bollard",
            "Bollard",
            CreatorAssetCategory::Prop,
            base + "props/Prop_Bollard.gltf",
            {0.5F, 0.5F, 1.2F}
        );

        add_imported_asset(
            "drain",
            "Street Drain",
            CreatorAssetCategory::Prop,
            base + "props/Prop_Drain.gltf",
            {1.0F, 0.5F, 0.1F}
        );

        add_imported_asset(
            "manhole",
            "Manhole Cover",
            CreatorAssetCategory::Prop,
            base + "props/Prop_ManholeCover.gltf",
            {1.0F, 1.0F, 0.1F}
        );

        add_imported_asset(
            "city_planter",
            "City Planter",
            CreatorAssetCategory::Prop,
            base + "props/Prop_Planter_Single.gltf",
            {1.5F, 1.5F, 1.0F}
        );
    }

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
            .footprint = {8.0F, 4.0F, 7.0F},
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
            .footprint = {9.0F, 7.0F, 9.0F},
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
            .footprint = {11.0F, 4.5F, 8.0F},
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
            .footprint = {12.0F, 5.0F, 10.0F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "garage",
                "utility",
                "verda"
            }
        });

        add({
            .id = "warehouse",
            .name = "Warehouse",
            .category = CreatorAssetCategory::Building,
            .model_path = "",
            .thumbnail_path = "",
            .footprint = {16.0F, 6.0F, 12.0F},
            .placement = {},
            .default_scale = 1.0F,
            .tags = {
                "warehouse",
                "industrial",
                "storage",
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

        register_downtown_creator_pack();
    }
};

} // namespace outland::creator

#endif // OUTLAND_DEV_TOOLS
