#include "outland/world/VerdaRegion.hpp"
#include "outland/world/assets/VerdanArchitecture.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"

#include <raylib.h>
#include <raymath.h>

#include <cmath>

namespace outland::world {

namespace {

void draw_building(
    const Building& building
) {
    using assets::HouseStyle;
    using assets::VerdanArchitecture;

    const float ground_y =
        terrain::TerrainHeight::sample(
            building.position.x,
            building.position.z
        );

    HouseStyle style{
        building.wall_color,

        Color{
            78,
            74,
            66,
            255
        },

        building.roof_color,

        Color{
            105,
            102,
            92,
            255
        },

        Color{
            83,
            119,
            135,
            255
        },

        false,
        false,
        true
    };

    /*
     * Architecture variations.
     *
     * BuildingStyle now changes silhouette,
     * not merely metadata.
     */
    switch (building.style) {

        case BuildingStyle::TwoStoryHouse:
            style.upper_floor = true;
            style.balcony = true;
            break;

        case BuildingStyle::Shop:
            style.upper_floor = false;
            style.balcony = false;

            style.trim = Color{
                67,
                82,
                70,
                255
            };
            break;

        case BuildingStyle::Garage:
            style.upper_floor = false;
            style.balcony = false;

            style.foundation = Color{
                90,
                88,
                82,
                255
            };
            break;

        case BuildingStyle::RuralHouse:
        default:
            break;
    }

    VerdanArchitecture::draw_house(
        {
            building.position.x,
            ground_y,
            building.position.z
        },
        building.size,
        style
    );
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

    Vector3 center =
        Vector3Scale(
            Vector3Add(
                road.start,
                road.end
            ),
            0.5F
        );

    center.y =
        terrain::TerrainHeight::sample(
            center.x,
            center.z
        ) +
        0.04F;

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

    /*
     * raylib 6.0 does not expose DrawCubePro.
     *
     * Build a temporary cube model and rotate its
     * transform so roads can point in any direction.
     */
    Mesh road_mesh =
        GenMeshCube(
            road.width,
            0.08F,
            length
        );

    Model road_model =
        LoadModelFromMesh(
            road_mesh
        );

    road_model.transform =
        MatrixMultiply(
            MatrixRotateY(
                angle * DEG2RAD
            ),
            MatrixTranslate(
                center.x,
                center.y,
                center.z
            )
        );

    road_model
        .materials[0]
        .maps[MATERIAL_MAP_DIFFUSE]
        .color =
            road_color;

    DrawModel(
        road_model,
        {
            0.0F,
            0.0F,
            0.0F
        },
        1.0F,
        WHITE
    );

    UnloadModel(
        road_model
    );
}

void draw_tree(
    Vector3 position
) {
    position.y =
        terrain::TerrainHeight::sample(
            position.x,
            position.z
        );

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
        const float settlement_ground_y =
            terrain::TerrainHeight::sample(
                settlement.center.x,
                settlement.center.z
            );

        DrawCylinder(
            {
                settlement.center.x,
                settlement_ground_y + 0.05F,
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
