#include "outland/world/VerdaRegion.hpp"

#include <raylib.h>
#include <raymath.h>

#include <cmath>

namespace outland::world {

namespace {

void draw_building(
    const Building& building
) {
    Vector3 body_position =
        building.position;

    body_position.y +=
        building.size.y * 0.5F;

    DrawCubeV(
        body_position,
        building.size,
        building.wall_color
    );

    DrawCubeWiresV(
        body_position,
        building.size,
        DARKGRAY
    );

    // Simple roof.
    const Vector3 roof_position{
        building.position.x,
        building.position.y +
            building.size.y +
            0.35F,
        building.position.z
    };

    const Vector3 roof_size{
        building.size.x + 0.6F,
        0.7F,
        building.size.z + 0.6F
    };

    DrawCubeV(
        roof_position,
        roof_size,
        building.roof_color
    );

    // Door.
    const Vector3 door_position{
        building.position.x,
        building.position.y + 1.1F,
        building.position.z -
            building.size.z * 0.5F -
            0.03F
    };

    DrawCube(
        door_position,
        1.2F,
        2.2F,
        0.10F,
        Color{
            75,
            55,
            40,
            255
        }
    );

    // Two front windows.
    const float window_offset =
        building.size.x * 0.28F;

    for (
        const float side :
        {-1.0F, 1.0F}
    ) {
        const Vector3 window_position{
            building.position.x +
                window_offset * side,

            building.position.y +
                building.size.y * 0.55F,

            building.position.z -
                building.size.z * 0.5F -
                0.04F
        };

        DrawCube(
            window_position,
            1.3F,
            1.1F,
            0.08F,
            Color{
                85,
                120,
                135,
                255
            }
        );
    }
}

void draw_road(
    const Road& road
) {
    const Vector3 delta =
        Vector3Subtract(
            road.end,
            road.start
        );

    const float length =
        Vector3Length(delta);

    const Vector3 center =
        Vector3Scale(
            Vector3Add(
                road.start,
                road.end
            ),
            0.5F
        );

    Color road_color{
        115,
        96,
        68,
        255
    };

    if (
        road.type ==
        RoadType::Gravel
    ) {
        road_color = {
            120,
            120,
            110,
            255
        };
    }

    if (
        road.type ==
        RoadType::Asphalt
    ) {
        road_color = {
            70,
            70,
            68,
            255
        };
    }

    const float angle =
        std::atan2(
            delta.x,
            delta.z
        ) *
        RAD2DEG;

    DrawCubePro(
        center,
        {
            road.width,
            0.08F,
            length
        },
        {
            0.0F,
            1.0F,
            0.0F
        },
        angle,
        road_color
    );
}

void draw_tree(
    Vector3 position
) {
    DrawCylinder(
        {
            position.x,
            position.y + 1.5F,
            position.z
        },
        0.25F,
        0.35F,
        3.0F,
        8,
        Color{
            90,
            65,
            40,
            255
        }
    );

    DrawSphere(
        {
            position.x,
            position.y + 4.0F,
            position.z
        },
        1.7F,
        Color{
            55,
            100,
            50,
            255
        }
    );
}

}

VerdaRegion::VerdaRegion() {
    generate_training_region();
}

void VerdaRegion::generate_training_region() {
    settlements_.clear();

    create_first_village();
}

void VerdaRegion::create_first_village() {
    Settlement village;

    village.id =
        "village_espera";

    village.name =
        "Espera";

    village.center = {
        0.0F,
        0.0F,
        -70.0F
    };

    village.state =
        SettlementState::Abandoned;

    // Main road from training area.
    village.roads.push_back(
        Road{
            {
                0.0F,
                0.03F,
                -20.0F
            },
            {
                0.0F,
                0.03F,
                -120.0F
            },
            6.0F,
            RoadType::Dirt
        }
    );

    // Cross road through village.
    village.roads.push_back(
        Road{
            {
                -45.0F,
                0.04F,
                -72.0F
            },
            {
                45.0F,
                0.04F,
                -72.0F
            },
            5.0F,
            RoadType::Gravel
        }
    );

    const Color plaster{
        210,
        198,
        170,
        255
    };

    const Color faded_blue{
        150,
        175,
        180,
        255
    };

    const Color faded_green{
        145,
        165,
        125,
        255
    };

    const Color brick{
        165,
        120,
        95,
        255
    };

    const Color roof_red{
        120,
        65,
        50,
        255
    };

    const Color roof_green{
        70,
        100,
        70,
        255
    };

    // --------------------------------------------------------
    // WEST SIDE
    // --------------------------------------------------------

    village.buildings.push_back(
        Building{
            "espera_house_01",
            BuildingStyle::RuralHouse,
            {-14.0F, 0.0F, -52.0F},
            {8.0F, 4.0F, 7.0F},
            0.0F,
            plaster,
            roof_red,
            true
        }
    );

    village.buildings.push_back(
        Building{
            "espera_house_02",
            BuildingStyle::RuralHouse,
            {-18.0F, 0.0F, -68.0F},
            {9.0F, 4.5F, 8.0F},
            0.0F,
            faded_blue,
            roof_green,
            true
        }
    );

    village.buildings.push_back(
        Building{
            "espera_house_03",
            BuildingStyle::TwoStoryHouse,
            {-16.0F, 0.0F, -88.0F},
            {9.0F, 7.0F, 9.0F},
            0.0F,
            brick,
            roof_red,
            true
        }
    );

    // --------------------------------------------------------
    // EAST SIDE
    // --------------------------------------------------------

    village.buildings.push_back(
        Building{
            "espera_house_04",
            BuildingStyle::RuralHouse,
            {15.0F, 0.0F, -55.0F},
            {8.0F, 4.0F, 8.0F},
            180.0F,
            faded_green,
            roof_green,
            true
        }
    );

    village.buildings.push_back(
        Building{
            "espera_shop_01",
            BuildingStyle::Shop,
            {17.0F, 0.0F, -73.0F},
            {11.0F, 4.5F, 8.0F},
            180.0F,
            plaster,
            roof_red,
            true
        }
    );

    village.buildings.push_back(
        Building{
            "espera_garage_01",
            BuildingStyle::Garage,
            {16.0F, 0.0F, -94.0F},
            {12.0F, 5.0F, 10.0F},
            180.0F,
            brick,
            roof_green,
            true
        }
    );

    // --------------------------------------------------------
    // TREES
    // --------------------------------------------------------

    const Vector3 tree_positions[] = {
        {-27.0F, 0.0F, -48.0F},
        {-30.0F, 0.0F, -64.0F},
        {-27.0F, 0.0F, -82.0F},
        {-30.0F, 0.0F, -101.0F},

        {29.0F, 0.0F, -48.0F},
        {31.0F, 0.0F, -66.0F},
        {29.0F, 0.0F, -84.0F},
        {32.0F, 0.0F, -104.0F},

        {-8.0F, 0.0F, -110.0F},
        {10.0F, 0.0F, -114.0F}
    };

    int tree_number = 0;

    for (
        const Vector3 position :
        tree_positions
    ) {
        WorldAsset tree;

        tree.id =
            "espera_tree_" +
            std::to_string(
                tree_number++
            );

        tree.type =
            AssetType::Tree;

        tree.position =
            position;

        tree.collision =
            true;

        village.assets.push_back(
            tree
        );
    }

    settlements_.push_back(
        village
    );
}

const std::vector<Settlement>&
VerdaRegion::settlements() const {
    return settlements_;
}

void VerdaRegion::draw() const {
    for (
        const Settlement& settlement :
        settlements_
    ) {
        for (
            const Road& road :
            settlement.roads
        ) {
            draw_road(road);
        }

        for (
            const Building& building :
            settlement.buildings
        ) {
            draw_building(
                building
            );
        }

        for (
            const WorldAsset& asset :
            settlement.assets
        ) {
            if (
                asset.type ==
                AssetType::Tree
            ) {
                draw_tree(
                    asset.position
                );
            }
        }

        // Development marker for settlement center.
        DrawCylinder(
            {
                settlement.center.x,
                0.05F,
                settlement.center.z
            },
            0.8F,
            0.8F,
            0.1F,
            16,
            GREEN
        );
    }
}

}
