#include "outland/creator/CreatorMapIO.hpp"

#ifdef OUTLAND_DEV_TOOLS

#include "outland/world/Building.hpp"
#include "outland/world/GameplayMarker.hpp"
#include "outland/world/Settlement.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/WorldAsset.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

bool near(
    const float a,
    const float b
) {
    return std::fabs(a - b) < 0.0001F;
}

} // namespace

int main() {
    using namespace outland;

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        "outland_creator_map_v2_test.map";

    std::error_code cleanup_error;
    std::filesystem::remove(
        path,
        cleanup_error
    );

    world::VerdaRegion region;

    auto& settlements =
        region.editable_settlements();

    /*
     * Make the fixture deterministic.
     */
    settlements.clear();
    settlements.emplace_back();
    settlements.front().id="test_settlement";

    world::Settlement& settlement =
        settlements.front();

    // --------------------------------------------------------
    // ORIGINAL VERDA CONTENT
    // --------------------------------------------------------

    world::Building original_building;

    original_building.id =
        "espera_original_house";

    original_building.style =
        world::BuildingStyle::RuralHouse;

    original_building.position = {
        2.0F,
        0.0F,
        3.0F
    };

    settlement.buildings.push_back(
        original_building
    );

    world::WorldAsset original_asset;

    original_asset.id =
        "espera_original_tree";

    original_asset.type =
        world::AssetType::Tree;

    original_asset.position = {
        4.0F,
        0.0F,
        5.0F
    };

    settlement.assets.push_back(
        original_asset
    );

    // --------------------------------------------------------
    // CREATOR BUILDING
    // --------------------------------------------------------

    world::Building creator_building;

    creator_building.id =
        "creator_building_warehouse_1";

    creator_building.style =
        world::BuildingStyle::Warehouse;

    creator_building.position = {
        11.0F,
        2.0F,
        13.0F
    };

    creator_building.size = {
        16.0F,
        6.0F,
        12.0F
    };

    creator_building.rotation_y =
        135.0F;

    creator_building.wall_color = {
        101,
        102,
        103,
        104
    };

    creator_building.roof_color = {
        201,
        202,
        203,
        204
    };

    creator_building.enterable =
        true;

    settlement.buildings.push_back(
        creator_building
    );

    // --------------------------------------------------------
    // CREATOR WORLD ASSET
    // --------------------------------------------------------

    world::WorldAsset creator_asset;

    creator_asset.id =
        "creator_tree_1";

    creator_asset.type =
        world::AssetType::Tree;

    creator_asset.model_path =
        "assets/verda/test/tree.gltf";

    creator_asset.position = {
        21.0F,
        1.0F,
        23.0F
    };

    creator_asset.size = {
        2.0F,
        7.0F,
        2.5F
    };

    creator_asset.rotation_y =
        75.0F;

    creator_asset.primary_color = {
        10,
        20,
        30,
        40
    };

    creator_asset.secondary_color = {
        50,
        60,
        70,
        80
    };

    creator_asset.collision =
        false;

    settlement.assets.push_back(
        creator_asset
    );


    // --------------------------------------------------------
    // CREATOR ROAD ASSET
    // --------------------------------------------------------

    world::WorldAsset creator_road;

    creator_road.id =
        "creator_road_straight_1";

    creator_road.type =
        world::AssetType::Road;

    creator_road.model_path =
        "assets/verda/test/road_straight.gltf";

    creator_road.position = {
        31.0F,
        0.0F,
        33.0F
    };

    creator_road.size = {
        4.0F,
        0.2F,
        12.0F
    };

    creator_road.rotation_y =
        90.0F;

    creator_road.collision =
        true;

    settlement.assets.push_back(
        creator_road
    );


    // --------------------------------------------------------
    // CREATOR GAMEPLAY MARKER
    // --------------------------------------------------------

    world::GameplayMarker creator_marker;

    creator_marker.id =
        "creator_marker_zombie_spawn_1";

    creator_marker.type =
        world::GameplayMarkerType::ZombieSpawn;

    creator_marker.position = {
        41.0F,
        1.5F,
        43.0F
    };

    creator_marker.size = {
        0.8F,
        0.8F,
        1.8F
    };

    creator_marker.rotation_y =
        225.0F;

    creator_marker.enabled =
        true;

    settlement.gameplay_markers.push_back(
        creator_marker
    );

    // --------------------------------------------------------
    // SAVE
    // --------------------------------------------------------

    assert(
        creator::CreatorMapIO::save(
            region,
            path.string()
        )
    );

    // --------------------------------------------------------
    // DESTROY CREATOR STATE AFTER SAVE
    // --------------------------------------------------------

    settlement.buildings.erase(
        settlement.buildings.begin() + 1
    );

    settlement.assets.erase(
        settlement.assets.begin() + 1
    );

    settlement.gameplay_markers.clear();

    assert(
        settlement.buildings.size() == 1
    );

    assert(
        settlement.assets.size() == 2
    );

    assert(
        settlement.gameplay_markers.empty()
    );

    // --------------------------------------------------------
    // LOAD
    // --------------------------------------------------------

    assert(
        creator::CreatorMapIO::load(
            region,
            path.string()
        )
    );

    assert(
        settlement.buildings.size() == 2
    );

    assert(
        settlement.assets.size() == 3
    );

    assert(
        settlement.gameplay_markers.size() == 1
    );

    // --------------------------------------------------------
    // ORIGINAL CONTENT MUST SURVIVE
    // --------------------------------------------------------

    assert(
        settlement.buildings[0].id ==
        "espera_original_house"
    );

    assert(
        settlement.assets[0].id ==
        "espera_original_tree"
    );

    // --------------------------------------------------------
    // VERIFY CREATOR BUILDING
    // --------------------------------------------------------

    const world::Building&
        loaded_building =
            settlement.buildings[1];

    assert(
        loaded_building.id ==
        "creator_building_warehouse_1"
    );

    assert(
        loaded_building.style ==
        world::BuildingStyle::Warehouse
    );

    assert(
        near(
            loaded_building.position.x,
            11.0F
        )
    );

    assert(
        near(
            loaded_building.position.y,
            2.0F
        )
    );

    assert(
        near(
            loaded_building.position.z,
            13.0F
        )
    );

    assert(
        near(
            loaded_building.size.x,
            16.0F
        )
    );

    assert(
        near(
            loaded_building.size.y,
            6.0F
        )
    );

    assert(
        near(
            loaded_building.size.z,
            12.0F
        )
    );

    assert(
        near(
            loaded_building.rotation_y,
            135.0F
        )
    );

    assert(
        loaded_building.wall_color.r == 101 &&
        loaded_building.wall_color.g == 102 &&
        loaded_building.wall_color.b == 103 &&
        loaded_building.wall_color.a == 104
    );

    assert(
        loaded_building.roof_color.r == 201 &&
        loaded_building.roof_color.g == 202 &&
        loaded_building.roof_color.b == 203 &&
        loaded_building.roof_color.a == 204
    );

    assert(
        loaded_building.enterable
    );

    // --------------------------------------------------------
    // VERIFY CREATOR WORLD ASSET
    // --------------------------------------------------------

    const world::WorldAsset&
        loaded_asset =
            settlement.assets[1];

    assert(
        loaded_asset.id ==
        "creator_tree_1"
    );

    assert(
        loaded_asset.type ==
        world::AssetType::Tree
    );

    assert(
        loaded_asset.model_path ==
        "assets/verda/test/tree.gltf"
    );

    assert(
        near(
            loaded_asset.position.x,
            21.0F
        )
    );

    assert(
        near(
            loaded_asset.position.y,
            1.0F
        )
    );

    assert(
        near(
            loaded_asset.position.z,
            23.0F
        )
    );

    assert(
        near(
            loaded_asset.size.x,
            2.0F
        )
    );

    assert(
        near(
            loaded_asset.size.y,
            7.0F
        )
    );

    assert(
        near(
            loaded_asset.size.z,
            2.5F
        )
    );

    assert(
        near(
            loaded_asset.rotation_y,
            75.0F
        )
    );

    assert(
        loaded_asset.primary_color.r == 10 &&
        loaded_asset.primary_color.g == 20 &&
        loaded_asset.primary_color.b == 30 &&
        loaded_asset.primary_color.a == 40
    );

    assert(
        loaded_asset.secondary_color.r == 50 &&
        loaded_asset.secondary_color.g == 60 &&
        loaded_asset.secondary_color.b == 70 &&
        loaded_asset.secondary_color.a == 80
    );

    assert(
        !loaded_asset.collision
    );


    // --------------------------------------------------------
    // VERIFY CREATOR ROAD ASSET
    // --------------------------------------------------------

    const world::WorldAsset&
        loaded_road =
            settlement.assets[2];

    assert(
        loaded_road.id ==
        "creator_road_straight_1"
    );

    assert(
        loaded_road.type ==
        world::AssetType::Road
    );

    assert(
        loaded_road.model_path ==
        "assets/verda/test/road_straight.gltf"
    );

    assert(
        near(
            loaded_road.position.x,
            31.0F
        )
    );

    assert(
        near(
            loaded_road.position.z,
            33.0F
        )
    );

    assert(
        near(
            loaded_road.rotation_y,
            90.0F
        )
    );

    assert(
        loaded_road.collision
    );

    // --------------------------------------------------------
    // VERIFY CREATOR GAMEPLAY MARKER
    // --------------------------------------------------------

    const world::GameplayMarker&
        loaded_marker =
            settlement.gameplay_markers[0];

    assert(
        loaded_marker.id ==
        "creator_marker_zombie_spawn_1"
    );

    assert(
        loaded_marker.type ==
        world::GameplayMarkerType::ZombieSpawn
    );

    assert(
        near(
            loaded_marker.position.x,
            41.0F
        )
    );

    assert(
        near(
            loaded_marker.position.y,
            1.5F
        )
    );

    assert(
        near(
            loaded_marker.position.z,
            43.0F
        )
    );

    assert(
        near(
            loaded_marker.size.x,
            0.8F
        )
    );

    assert(
        near(
            loaded_marker.size.y,
            0.8F
        )
    );

    assert(
        near(
            loaded_marker.size.z,
            1.8F
        )
    );

    assert(
        near(
            loaded_marker.rotation_y,
            225.0F
        )
    );

    assert(
        loaded_marker.enabled
    );

    // --------------------------------------------------------
    // CLEANUP
    // --------------------------------------------------------

    std::filesystem::remove(
        path,
        cleanup_error
    );

    std::cout
        << "Creator Map V2 round-trip GREEN\n";

    return 0;
}

#else

int main() {
    return 0;
}

#endif
