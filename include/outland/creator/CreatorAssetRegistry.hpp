#pragma once

#ifdef OUTLAND_DEV_TOOLS

#include <cstddef>
#include <array>
#include <optional>
#include <tuple>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

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

class CreatorAssetRegistry;
inline void register_urban_assets(CreatorAssetRegistry& registry);
inline void register_character_assets(CreatorAssetRegistry& registry);
inline void register_starter_assets(CreatorAssetRegistry& registry);
inline void register_survival_assets(CreatorAssetRegistry& registry);
inline void register_industrial_assets(CreatorAssetRegistry& registry);
inline void register_vehicle_assets(CreatorAssetRegistry& registry);

class CreatorAssetRegistry {
public:
    CreatorAssetRegistry() {
        register_builtin_assets();
        register_urban_assets(*this);
        register_character_assets(*this);
        register_starter_assets(*this);
        register_survival_assets(*this);
        register_industrial_assets(*this);
        register_vehicle_assets(*this);
        // Verda building kit: textured 3 x 3 m slabs that tile with the 3 m Building Parts walls
        // (tools/generate_kit_slabs.py). Floors, ceilings and roofs for enterable buildings.
        for (const auto& [id, name, file] : {std::tuple{"verda_kit_floor_slab", "Kit Floor Slab 3x3", "floor_slab_3x3.glb"},
                                             std::tuple{"verda_kit_roof_slab", "Kit Roof Slab 3x3", "roof_slab_3x3.glb"}})
            (void)add({.id = id, .name = name, .category = CreatorAssetCategory::BuildingPart,
                .model_path = std::string("assets/verda/kit/") + file, .thumbnail_path = "",
                .footprint = {3, 3, .1F}, .placement = {}, .default_scale = 1, .tags = {"verda", "kit", "floor", "roof"}});
        // Retain legacy palette IDs while giving their former empty placeholders real geometry.
        const auto reuse_model=[&](const char* alias,const char* source_id) {
            const auto* source=find(source_id);if(!source)return;
            for(auto& asset:assets_)if(asset.id==alias){asset.model_path=source->model_path;asset.footprint=source->footprint;break;}
        };
        reuse_model("rock","starter_rock_boulder_lod0");
        reuse_model("wood_fence","urban_walls_fences_white_picket_fence_white_picket_fence_closed_left");
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

        // Complete downloaded buildings; preserve the procedural building entries.
        add_imported_asset("downtown_building_small_1", "Downtown Small Building",
            CreatorAssetCategory::Building, base + "buildings/Building_Small_1.gltf",
            {12.46000F, 14.53601F, 17.02597F});
        add_imported_asset("downtown_building_medium_2", "Downtown Medium Building",
            CreatorAssetCategory::Building, base + "buildings/Building_Medium_2_001.gltf",
            {15.05568F, 13.05568F, 25.00866F});
        add_imported_asset("downtown_building_large_2", "Downtown Large Building",
            CreatorAssetCategory::Building, base + "buildings/Building_Large_2.gltf",
            {20.64411F, 16.64460F, 28.00004F});

        // PARTS
        add_imported_asset(
            "brick_plain_1",
            "Brick Wall",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_Plain_1.gltf",
            {2.00000F, 0.20000F, 1.00000F}
        );

        add_imported_asset(
            "brick_plain_3",
            "Brick Wall Wide",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_Plain_3.gltf",
            {2.00000F, 0.20000F, 3.00000F}
        );

        add_imported_asset(
            "brick_corner_l",
            "Brick Corner L",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_90Angle_L.gltf",
            {0.70620F, 0.70000F, 3.00000F}
        );

        add_imported_asset(
            "brick_corner_r",
            "Brick Corner R",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_90Angle_R.gltf",
            {0.70620F, 0.70000F, 3.00000F}
        );

        add_imported_asset(
            "brick_interior_wall_1",
            "Interior Wall",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_InteriorWall_1.gltf",
            {2.00000F, 0.01000F, 1.00000F}
        );

        add_imported_asset(
            "brick_interior_wall_3",
            "Interior Wall Wide",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_InteriorWall_3.gltf",
            {2.00000F, 0.01000F, 3.00000F}
        );

        add_imported_asset(
            "brick_window_single",
            "Brick Window",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_Window_Square_Single.gltf",
            {2.00000F, 0.27101F, 3.00000F}
        );

        add_imported_asset(
            "brick_window_double",
            "Brick Double Window",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Brick_RedWhite_DoubleWindow.gltf",
            {4.00000F, 0.27734F, 3.00000F}
        );

        add_imported_asset(
            "metal_plain_1",
            "Metal Wall",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Metal_Plain_1.gltf",
            {2.00000F, 0.20000F, 1.00000F}
        );

        add_imported_asset(
            "metal_plain_3",
            "Metal Wall Wide",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Metal_Plain_3.gltf",
            {2.00000F, 0.20000F, 3.00000F}
        );

        add_imported_asset(
            "metal_first_floor_wall",
            "Metal Ground Wall",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Metal_FirstFloor_Wall.gltf",
            {2.00000F, 0.24201F, 3.00000F}
        );

        add_imported_asset(
            "metal_first_floor_window",
            "Metal Ground Window",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Metal_FirstFloor_Window.gltf",
            {2.00000F, 0.25879F, 3.00000F}
        );

        add_imported_asset(
            "metal_window",
            "Metal Window",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Metal_Window.gltf",
            {4.00000F, 0.22390F, 3.00059F}
        );

        // DOORS / FLOORS / ROOFS / ACCESS
        add_imported_asset(
            "doorframe_metal",
            "Metal Door Frame",
            CreatorAssetCategory::BuildingPart,
            base + "parts/DoorFrame_Metal_Single.gltf",
            {2.00000F, 0.20000F, 3.00000F}
        );

        add_imported_asset(
            "doorframe_wood",
            "Wood Door Frame",
            CreatorAssetCategory::BuildingPart,
            base + "parts/DoorFrame_Wooden.gltf",
            {2.30400F, 0.47782F, 3.00000F}
        );

        add_imported_asset(
            "door_1",
            "Door 1",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Door_1.gltf",
            {1.00000F, 0.25985F, 2.20000F}
        );

        add_imported_asset(
            "door_2",
            "Door 2",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Door_2.gltf",
            {1.00000F, 0.21034F, 2.20000F}
        );

        add_imported_asset(
            "door_3",
            "Door 3",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Door_3.gltf",
            {1.00000F, 0.18834F, 2.20000F}
        );

        add_imported_asset(
            "floor_2x2",
            "Floor 2x2",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Floor_2x2.gltf",
            {2.00000F, 2.00000F, 0.10000F}
        );

        add_imported_asset(
            "floor_4x4",
            "Floor 4x4",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Floor_4x4.gltf",
            {4.00000F, 4.00000F, 0.10000F}
        );

        add_imported_asset(
            "roof_2x2",
            "Roof 2x2",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Roof_2x2.gltf",
            {2.00000F, 2.00000F, 0.01000F}
        );

        add_imported_asset(
            "roof_4x4",
            "Roof 4x4",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Roof_4x4.gltf",
            {4.00000F, 4.00000F, 0.01000F}
        );

        add_imported_asset(
            "stairs_entrance",
            "Entrance Stairs",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Stairs_Entrance_Concrete.gltf",
            {2.00000F, 1.99078F, 1.00675F}
        );

        add_imported_asset(
            "entrance_concrete_2x1",
            "Concrete Entrance 2x1",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Entrance_Concrete_2x1.gltf",
            {2.00000F, 1.00321F, 1.00675F}
        );

        add_imported_asset(
            "entrance_concrete_2x2",
            "Concrete Entrance 2x2",
            CreatorAssetCategory::BuildingPart,
            base + "parts/Entrance_Concrete_2x2.gltf",
            {2.00000F, 2.00331F, 1.00675F}
        );

        // ====================================================
        // ROADS
        // ====================================================

        add_imported_asset(
            "street_2lane",
            "Street 2 Lane",
            CreatorAssetCategory::Road,
            base + "roads/Street_2Lane.gltf",
            {6.00000F, 12.00000F, 0.15000F}
        );

        add_imported_asset(
            "street_2lane_nosidewalk",
            "Street 2 Lane Bare",
            CreatorAssetCategory::Road,
            base + "roads/Street_2Lane_noSidewalk.gltf",
            {6.00000F, 6.00000F, 0.01000F}
        );

        add_imported_asset(
            "street_4lane",
            "Street 4 Lane",
            CreatorAssetCategory::Road,
            base + "roads/Street_4Lane.gltf",
            {6.00000F, 18.00000F, 0.15000F}
        );

        add_imported_asset(
            "street_4way",
            "4-Way Intersection",
            CreatorAssetCategory::Road,
            base + "roads/Street_4WayIntersection.gltf",
            {24.66626F, 24.66626F, 0.15000F}
        );

        add_imported_asset(
            "street_t",
            "T Intersection",
            CreatorAssetCategory::Road,
            base + "roads/Street_TIntersection.gltf",
            {24.66626F, 21.33313F, 0.15000F}
        );

        add_imported_asset(
            "street_curve_2lane",
            "Street Curve",
            CreatorAssetCategory::Road,
            base + "roads/Street_Curve_2Lane.gltf",
            {12.00000F, 12.00000F, 0.15000F}
        );

        add_imported_asset(
            "asphalt_6x6",
            "Asphalt 6x6",
            CreatorAssetCategory::Road,
            base + "roads/Street_Asphalt_6x6.gltf",
            {6.00000F, 6.00000F, 0.01000F}
        );

        add_imported_asset(
            "asphalt_9x9",
            "Asphalt 9x9",
            CreatorAssetCategory::Road,
            base + "roads/Street_Asphalt_9x9.gltf",
            {9.00000F, 9.00000F, 0.01000F}
        );

        add_imported_asset(
            "sidewalk_straight",
            "Straight Sidewalk",
            CreatorAssetCategory::Road,
            base + "roads/Sidewalk_Straight_3m.gltf",
            {3.00000F, 3.01000F, 0.15000F}
        );

        add_imported_asset(
            "sidewalk_corner",
            "Sidewalk Corner",
            CreatorAssetCategory::Road,
            base + "roads/Sidewalk_Corner_Flat_3m.gltf",
            {3.01000F, 3.01000F, 0.15000F}
        );

        add_imported_asset(
            "crosswalk",
            "Crosswalk",
            CreatorAssetCategory::Road,
            base + "roads/Decal_Crosswalk.gltf",
            {4.53819F, 5.42604F, 0.01000F}
        );

        add_imported_asset(
            "stop_decal",
            "STOP Road Marking",
            CreatorAssetCategory::Road,
            base + "roads/Decal_Stop.gltf",
            {2.95943F, 1.52198F, 0.01000F}
        );

        // ====================================================
        // CITY PROPS
        // ====================================================

        add_imported_asset(
            "ac_unit",
            "AC Unit",
            CreatorAssetCategory::Prop,
            base + "props/Prop_ACUnit.gltf",
            {0.89317F, 0.34707F, 0.60000F}
        );

        add_imported_asset(
            "bollard",
            "Bollard",
            CreatorAssetCategory::Prop,
            base + "props/Prop_Bollard.gltf",
            {0.21627F, 0.22740F, 0.89178F}
        );

        add_imported_asset(
            "drain",
            "Street Drain",
            CreatorAssetCategory::Prop,
            base + "props/Prop_Drain.gltf",
            {0.58519F, 0.58519F, 0.04041F}
        );

        add_imported_asset(
            "manhole",
            "Manhole Cover",
            CreatorAssetCategory::Prop,
            base + "props/Prop_ManholeCover.gltf",
            {0.92511F, 0.92511F, 0.03297F}
        );

        add_imported_asset(
            "city_planter",
            "City Planter",
            CreatorAssetCategory::Prop,
            base + "props/Prop_Planter_Single.gltf",
            {2.00000F, 2.00000F, 0.60000F}
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

        (void)add({
            .id = "warehouse", .name = "Warehouse", .category = CreatorAssetCategory::Building,
            .model_path = "", .thumbnail_path = "", .footprint = {16.0F, 12.0F, 6.0F},
            .placement = {}, .default_scale = 1.0F,
            .tags = {"warehouse", "industrial", "storage", "verda"}
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

        for(const auto& entry:std::array<std::pair<const char*,const char*>,3>{{
            {"loot_spawn_medical","Medical Loot Spawn"},
            {"loot_spawn_camp","Camp Loot Spawn"},
            {"loot_spawn_weapons","Weapon Loot Spawn"}}}) {
            (void)add({.id=entry.first,.name=entry.second,.category=CreatorAssetCategory::Gameplay,
                .model_path="",.thumbnail_path="",.footprint={.5F,.5F,.5F},.placement={},
                .default_scale=1,.tags={"loot","spawn","gameplay"}});
        }

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

        for (const auto& [id, name] : std::vector<std::pair<std::string, std::string>>{
            {"npc_spawn_emergency", "Emergency NPC Spawn"},
            {"npc_spawn_hostile", "Hostile NPC Spawn"},
            {"npc_spawn_creature", "Creature NPC Spawn"}}) {
            (void)add({.id=id, .name=name, .category=CreatorAssetCategory::Gameplay,
                .model_path="", .thumbnail_path="", .footprint={0.8F,0.8F,1.8F},
                .placement={}, .default_scale=1.0F, .tags={"npc","spawn","gameplay"}});
        }

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

#include "outland/creator/UrbanAssetCatalog.hpp"
#include "outland/creator/CharacterAssetCatalog.hpp"
#include "outland/creator/StarterAssetCatalog.hpp"
#include "outland/creator/SurvivalAssetCatalog.hpp"
#include "outland/creator/IndustrialAssetCatalog.hpp"
#include "outland/creator/VehicleAssetCatalog.hpp"

#endif // OUTLAND_DEV_TOOLS
