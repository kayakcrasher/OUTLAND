#include "outland/world/VerdaRegion.hpp"
#include "outland/world/assets/VerdanArchitecture.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include "outland/world/assets/VerdaGeometry.hpp"
#include <algorithm>

#include <cmath>


namespace outland::world {

namespace {

void draw_building(
    const Building& building,
    const Vector3& camera_position
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

        case BuildingStyle::Warehouse:
            // Large, low industrial shell.
            // It deliberately uses the same open modular architecture
            // contract as every other OUTLAND building so doors and
            // windows remain real gameplay openings.
            style.upper_floor = false;
            style.balcony = false;

            style.plaster = Color{
                148,
                151,
                145,
                255
            };

            style.trim = Color{
                66,
                70,
                68,
                255
            };

            style.foundation = Color{
                78,
                80,
                76,
                255
            };

            style.roof = Color{
                82,
                86,
                83,
                255
            };
            break;

        case BuildingStyle::RuralHouse:
        default:
            break;
    }

    rlPushMatrix();
    rlTranslatef(building.position.x, ground_y, building.position.z);
    rlRotatef(building.rotation_y, 0.0F, 1.0F, 0.0F);
    VerdanArchitecture::draw_house({0.0F, 0.0F, 0.0F}, building.size, style,
        assets::distance_squared(building.position, camera_position) < 90.0F * 90.0F);
    rlPopMatrix();
}

void draw_road(
    const Road& road
) {
    // Terrain-following ribbon; immediate triangles do not allocate GPU resources.
    const float dx = road.end.x - road.start.x;
    const float dz = road.end.z - road.start.z;
    const float length = std::sqrt(dx*dx + dz*dz);
    if (length < 0.001F || road.width <= 0.0F) return;
    const float side_x = dz / length * road.width * 0.5F;
    const float side_z = -dx / length * road.width * 0.5F;
    const int segments = std::clamp(static_cast<int>(std::ceil(length / 2.0F)), 1, 512);
    const Color color = road.type == RoadType::Asphalt ? Color{76,80,77,255}
                      : road.type == RoadType::Gravel ? Color{157,151,127,255}
                      : Color{161,128,83,255};
    const auto edge = [&](float t, float side) {
        const float x = road.start.x + dx*t + side_x*side;
        const float z = road.start.z + dz*t + side_z*side;
        return Vector3{x, terrain::TerrainHeight::sample(x,z) + 0.06F, z};
    };
    for (int i = 0; i < segments; ++i) {
        const float a = static_cast<float>(i) / segments;
        const float b = static_cast<float>(i+1) / segments;
        assets::draw_quad(edge(a, 1), edge(a, -1), edge(b, -1), edge(b, 1), color);
    }
}

void draw_tree(Vector3 position, const Vector3& camera_position) {
    position.y = terrain::TerrainHeight::sample(position.x, position.z);
    const float distance = assets::distance_squared(position, camera_position);
    if (distance > 260.0F * 260.0F) return;
    const bool detailed = distance < 65.0F * 65.0F;
    const float variant = assets::variation(static_cast<int>(position.x), static_cast<int>(position.z), 5);
    const float scale = 0.85F + variant * 0.45F;
    const Color bark{115,83,52,255};
    DrawCylinderEx(position, {position.x+0.18F*scale, position.y+3.5F*scale, position.z},
                   0.35F*scale, 0.16F*scale, detailed ? 8 : 5, bark);
    if (detailed) {
        for (float side : {-1.0F, 1.0F}) {
            DrawCylinderEx({position.x, position.y+2.1F*scale, position.z},
                           {position.x+side*1.1F*scale, position.y+3.4F*scale, position.z+0.3F*side},
                           0.14F*scale, 0.06F*scale, 5, bark);
        }
    }
    DrawSphereEx({position.x, position.y+4.4F*scale, position.z}, 1.6F*scale,
                 detailed ? 6 : 4, detailed ? 8 : 6, Color{103,157,67,255});
    if (distance < 140.0F * 140.0F) {
        DrawSphereEx({position.x-1.0F*scale, position.y+3.6F*scale, position.z+0.35F*scale},
                     1.25F*scale, 4, 6, Color{79,133,58,255});
        DrawSphereEx({position.x+1.1F*scale, position.y+3.8F*scale, position.z-0.25F*scale},
                     1.2F*scale, 4, 6, Color{128,176,77,255});
    }
}

}

VerdaRegion::VerdaRegion() {
    generate_training_region();
}

#ifdef OUTLAND_DEV_TOOLS

std::vector<Settlement>&
VerdaRegion::editable_settlements() {
    return settlements_;
}

bool VerdaRegion::place_world_asset(
    WorldAsset asset,
    std::size_t settlement_index
) {
    if (settlement_index >= settlements_.size()) {
        return false;
    }

    if (asset.id.empty()) {
        return false;
    }

    // Creator IDs must remain unique so selection,
    // deletion and future SAVE/LOAD remain deterministic.
    for (const Settlement& settlement : settlements_) {
        for (const WorldAsset& existing : settlement.assets) {
            if (existing.id == asset.id) {
                return false;
            }
        }
    }

    settlements_[settlement_index].assets.push_back(
        std::move(asset)
    );

    return true;
}

bool VerdaRegion::delete_world_asset(
    std::string_view asset_id
) {
    if (asset_id.empty()) {
        return false;
    }

    for (Settlement& settlement : settlements_) {
        const auto before = settlement.assets.size();

        std::erase_if(
            settlement.assets,
            [asset_id](const WorldAsset& asset) {
                return asset.id == asset_id;
            }
        );

        if (settlement.assets.size() != before) {
            return true;
        }
    }

    return false;
}


bool VerdaRegion::delete_building(
    const std::string_view building_id
) {
    for (auto& settlement : settlements_) {
        const auto before =
            settlement.buildings.size();

        std::erase_if(
            settlement.buildings,
            [&](const Building& building) {
                return building.id == building_id;
            }
        );

        if (settlement.buildings.size() != before) {
            return true;
        }
    }

    return false;
}

bool VerdaRegion::delete_road(
    const std::size_t settlement_index,
    const std::size_t road_index
) {
    if (settlement_index >= settlements_.size()) {
        return false;
    }

    auto& roads =
        settlements_[settlement_index].roads;

    if (road_index >= roads.size()) {
        return false;
    }

    roads.erase(
        roads.begin() +
        static_cast<std::ptrdiff_t>(road_index)
    );

    return true;
}

#endif // OUTLAND_DEV_TOOLS

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
        232,
        219,
        182,
        255
    };

    const Color faded_blue{
        165,
        199,
        204,
        255
    };

    const Color faded_green{
        172,
        193,
        139,
        255
    };

    const Color brick{
        165,
        120,
        95,
        255
    };

    const Color roof_red{
        157,
        79,
        54,
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

void VerdaRegion::draw(const Vector3& camera_position) const {
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
            if (assets::distance_squared(building.position, camera_position) < 320.0F * 320.0F) {
                draw_building(building, camera_position);
            }
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
                    asset.position, camera_position
                );
            }
        }

    }
}
}
