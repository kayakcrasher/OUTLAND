#include "outland/engine/render/ViewFrustum.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/assets/VerdanArchitecture.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include "outland/world/assets/VerdaGeometry.hpp"
#include <algorithm>
#include <array>
#include <map>
#include "outland/world/VerdaLayout.hpp"

#include <cmath>


namespace outland::world {

namespace {

// part: 0 the whole house, 1 the shell without its front door, 2 the front door alone.
void draw_building_lod(
    const Building& building,
    bool detailed,
    int part = 0
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
    if (part == 2) VerdanArchitecture::draw_front_door({0.0F, 0.0F, 0.0F}, building.size, style, building.door);
    else VerdanArchitecture::draw_house({0.0F, 0.0F, 0.0F}, building.size, style, detailed, part == 1 ? -1.0F : building.door);
    rlPopMatrix();
}

void draw_building(const Building& building, const Vector3& camera_position) {
    draw_building_lod(building, assets::distance_squared(building.position, camera_position) < 90.0F * 90.0F);
}

void draw_road(
    const Road& road, const Vector3& camera
) {
    // Terrain-following ribbon; immediate triangles do not allocate GPU resources.
    const float dx = road.end.x - road.start.x;
    const float dz = road.end.z - road.start.z;
    const float length = std::sqrt(dx*dx + dz*dz);
    if (length < 0.001F || road.width <= 0.0F) return;
    const float side_x = dz / length * road.width * 0.5F;
    const float side_z = -dx / length * road.width * 0.5F;
    const float projection=std::clamp(((camera.x-road.start.x)*dx+(camera.z-road.start.z)*dz)/(length*length),0.0F,1.0F);
    const float nearest_x=road.start.x+dx*projection, nearest_z=road.start.z+dz*projection;
    if(std::hypot(camera.x-nearest_x,camera.z-nearest_z)>330+road.width) return;
    const int segments=std::max(1,static_cast<int>(std::ceil(length/2.0F)));
    const int begin=std::max(0,static_cast<int>(std::floor((projection-340.0F/length)*segments)));
    const int end=std::min(segments,static_cast<int>(std::ceil((projection+340.0F/length)*segments)));
    const Color color=road.type!=RoadType::Gravel && road.type!=RoadType::Dirt ? Color{76,80,77,255} :
        road.type==RoadType::Gravel ? Color{157,151,127,255} : Color{161,128,83,255};
    // CPU ribbon samples keyed by geometry: camera movement no longer resamples terrain.
    // Replacing/moving a Creator road naturally selects a new key; bound old entries.
    using Key=std::array<float,6>;
    static std::map<Key,std::map<int,std::array<Vector3,2>>> ribbons;
    static bool coast=terrain::TerrainHeight::coastal_layout();
    if(coast!=terrain::TerrainHeight::coastal_layout()) {ribbons.clear();coast=terrain::TerrainHeight::coastal_layout();}
    const Key key{road.start.x,road.start.z,road.end.x,road.end.z,road.width,static_cast<float>(segments)};
    if(!ribbons.contains(key) && ribbons.size()>=64) ribbons.clear();
    auto& ribbon=ribbons[key];
    if(ribbon.size()>4096) std::erase_if(ribbon,[&](const auto& sample){return sample.first<begin-512 || sample.first>end+512;});
    const auto edge=[&](int i)->const std::array<Vector3,2>& {
        const auto found=ribbon.find(i);
        if(found!=ribbon.end()) return found->second;
        const float t=static_cast<float>(i)/segments;
        std::array<Vector3,2> points{};
        for(int side=0;side<2;++side) {
            const float sign=side==0 ? 1.0F : -1.0F;
            const float x=road.start.x+dx*t+side_x*sign,z=road.start.z+dz*t+side_z*sign;
            points[side]={x,terrain::TerrainHeight::sample(x,z)+.06F,z};
        }
        return ribbon.emplace(i,points).first->second;
    };
    for(int i=begin;i<end;++i) {
        const auto& a=edge(i);const auto& b=edge(i+1);
        assets::draw_quad(a[0],a[1],b[1],b[0],color);
    }
}

// lod 0: within 65 m (branches), 1: within 140 m (three crowns), 2: further (one crown).
void draw_tree_lod(Vector3 position, int lod) {
    position.y = terrain::TerrainHeight::sample(position.x, position.z);
    const bool detailed = lod == 0;
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
    if (lod <= 1) {
        DrawSphereEx({position.x-1.0F*scale, position.y+3.6F*scale, position.z+0.35F*scale},
                     1.25F*scale, 4, 6, Color{79,133,58,255});
        DrawSphereEx({position.x+1.1F*scale, position.y+3.8F*scale, position.z-0.25F*scale},
                     1.2F*scale, 4, 6, Color{128,176,77,255});
    }
}

void draw_tree(Vector3 position, const Vector3& camera_position,const engine::ViewFrustum* view) {
    const float dx=position.x-camera_position.x,dz=position.z-camera_position.z;
    if(dx*dx+dz*dz>260.0F*260.0F) return;
    const float ground = terrain::TerrainHeight::sample(position.x, position.z);
    if(view && !view->sphere({position.x,ground+5,position.z},9)) return;
    const float distance = assets::distance_squared({position.x,ground,position.z}, camera_position);
    if (distance > 260.0F * 260.0F) return;
    draw_tree_lod(position, distance < 65.0F * 65.0F ? 0 : distance < 140.0F * 140.0F ? 1 : 2);
}

}

namespace geometry {
void building(const Building& value, bool detailed) { draw_building_lod(value, detailed, 1); }
void building_door(const Building& value) { draw_building_lod(value, true, 2); }
void tree(Vector3 position, int lod) { draw_tree_lod(position, lod); }
int road_segments(const Road& road) {
    const float length = std::hypot(road.end.x - road.start.x, road.end.z - road.start.z);
    return length < .001F || road.width <= 0 ? 0 : std::max(1, static_cast<int>(std::ceil(length / 2.0F)));
}
void road(const Road& road, int begin, int end) {
    const int segments = road_segments(road);
    if (segments == 0) return;
    const float dx = road.end.x - road.start.x, dz = road.end.z - road.start.z, length = std::sqrt(dx * dx + dz * dz);
    const float side_x = dz / length * road.width * .5F, side_z = -dx / length * road.width * .5F;
    const Color color = road.type != RoadType::Gravel && road.type != RoadType::Dirt ? Color{76, 80, 77, 255} :
        road.type == RoadType::Gravel ? Color{157, 151, 127, 255} : Color{161, 128, 83, 255};
    const auto edge = [&](int i) {
        const float t = static_cast<float>(i) / segments;
        std::array<Vector3, 2> points{};
        for (int side = 0; side < 2; ++side) {
            const float sign = side == 0 ? 1.0F : -1.0F;
            const float x = road.start.x + dx * t + side_x * sign, z = road.start.z + dz * t + side_z * sign;
            points[static_cast<std::size_t>(side)] = {x, terrain::TerrainHeight::sample(x, z) + .06F, z};
        }
        return points;
    };
    begin = std::max(0, begin); end = std::min(segments, end);
    auto a = edge(begin);
    for (int i = begin; i < end; ++i) {
        const auto b = edge(i + 1);
        assets::draw_quad(a[0], a[1], b[1], b[0], color);
        a = b;
    }
}
}

VerdaRegion::VerdaRegion() : VerdaRegion(true) {}
VerdaRegion::VerdaRegion(bool coastal_layout) : coastal_layout_(coastal_layout) { generate_training_region(); }

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
            [asset_id,&settlement](const WorldAsset& asset) {
                if(asset.id!=asset_id)return false;
                for(auto& marker:settlement.gameplay_markers)if(marker.id==asset.vehicle.marker)marker.enabled=false;
                return true;
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

    terrain::TerrainHeight::set_coastal_layout(coastal_layout_);
    create_first_village();
    if(coastal_layout_) create_coastal_region();
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

void VerdaRegion::create_coastal_region() {
    auto& espera=settlements_.front();
    const auto target=layout::sites[1].center;
    const Vector3 delta{target.x-espera.center.x,0,target.z-espera.center.z};
    const auto move=[&](Vector3& p){p.x+=delta.x;p.z+=delta.z;};
    move(espera.center);
    for(auto& b:espera.buildings) move(b.position);
    for(auto& a:espera.assets) move(a.position);
    for(auto& m:espera.gameplay_markers) move(m.position);
    for(auto& r:espera.roads){move(r.start);move(r.end);}
    for(std::size_t i=0;i<layout::sites.size();++i) {
        if(i==1) continue;
        const auto& site=layout::sites[i];
        Settlement town;town.id=site.id;town.name=site.name;town.center=site.center;
        town.state=SettlementState::Peaceful;town.survivors=i==0?80:24;
        const float avenue=i==0?110:22;
        for(int side:{-1,1}) for(int row=0;row<4;++row) {
            const float z=(row-1.5F)*30;
            Building building;
            building.id=town.id+"_building_"+std::to_string(town.buildings.size());
            building.position={town.center.x+side*avenue,0,town.center.z+z};
            building.rotation_y=side<0?0:180;
            building.style=i==2 || i==4 ? (row%2==0?BuildingStyle::Warehouse:BuildingStyle::Shop) :
                i==0?BuildingStyle::TwoStoryHouse:BuildingStyle::RuralHouse;
            building.size=i==2 || i==4?Vector3{18,5,16}:Vector3{12,i==0?7.0F:4.0F,10};
            building.wall_color=i==2?Color{192,207,215,255}:i==4?Color{155,147,127,255}:Color{225,214,176,255};
            building.roof_color=i==4?Color{82,86,83,255}:i==3?Color{70,100,70,255}:Color{157,79,54,255};
            building.enterable=true;town.buildings.push_back(building);
        }
        const float span=i==0?160:90;
        town.roads.push_back({{town.center.x,0,town.center.z-span},{town.center.x,0,town.center.z+span},8,RoadType::Asphalt});
        town.roads.push_back({{town.center.x-span,0,town.center.z},{town.center.x+span,0,town.center.z},7,RoadType::Gravel});
        settlements_.push_back(std::move(town));
    }
    auto& capital=settlements_[1];
    capital.gameplay_markers.push_back({"vehicle_spawn_hatchback_capital",GameplayMarkerType::VehicleSpawn,{5,0,8},{3,1.8F,5},180,true});
    const auto link=[&](Vector3 a,Vector3 b){capital.roads.push_back({a,b,8,RoadType::Asphalt});};
    for(std::size_t i=1;i<layout::sites.size();++i) {
        const auto p=layout::sites[i].center;
        Vector3 last{0,0,0};
        for(float fraction:{0.35F,0.65F,1.0F}) {
            const Vector3 next{p.x*fraction,0,p.z*fraction};link(last,next);last=next;
        }
    }
    // Inland ring joins neighbouring coastal hubs without cutting through the sea.
    for(std::size_t i=1;i<layout::sites.size();++i) {
        const auto a=layout::sites[i].center,b=layout::sites[i==4?1:i+1].center;
        float angle=std::atan2(a.z,a.x), end=std::atan2(b.z,b.x);
        while(end<=angle)end+=2*PI;
        Vector3 last=a;
        for(int step=0;step<=6;++step) {
            const float theta=angle+(end-angle)*step/6;
            const Vector3 next{1500*std::cos(theta),0,1500*std::sin(theta)};
            link(last,next);last=next;
        }
        link(last,b);
    }
}

const std::vector<Settlement>&
VerdaRegion::settlements() const {
    return settlements_;
}

void VerdaRegion::draw(const Vector3& camera_position, const engine::ViewFrustum* view) const {
    for (
        const Settlement& settlement :
        settlements_
    ) {
        for (
            const Road& road :
            settlement.roads
        ) {
            draw_road(road, camera_position);
        }

        for (
            const Building& building :
            settlement.buildings
        ) {
            if (assets::distance_squared(building.position, camera_position) < 320.0F * 320.0F &&
                (!view || view->sphere({building.position.x,terrain::TerrainHeight::sample(building.position.x,building.position.z)+building.size.y*.5F,building.position.z},Vector3Length(building.size)))) {
                draw_building(building, camera_position);
            }
        }

        for (
            const WorldAsset& asset :
            settlement.assets
        ) {
            if(!asset.vehicle.definition.empty())continue;
            if (
                asset.type ==
                AssetType::Tree && asset.model_path.empty()
            ) {
                draw_tree(
                    asset.position, camera_position, view
                );
            }
        }

    }
}
}
