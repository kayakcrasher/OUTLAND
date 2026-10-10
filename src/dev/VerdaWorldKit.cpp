#include "outland/dev/VerdaWorldKit.hpp"
#include "outland/creator/CreatorAssetRegistry.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include <raymath.h>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace outland::dev {
namespace {
const std::string parts = "assets/verda/urban/Building Parts/";
const std::string shacks = "assets/verda/industrial/shacks/";
const std::string urban = "assets/verda/urban/";

struct Lot { float x0, z0, x1, z1; };

class Kit {
public:
    Kit(world::VerdaRegion& region, const creator::CreatorAssetRegistry& catalog) : region_(region) {
        for (const auto& asset : catalog.assets()) if (!asset.model_path.empty()) by_path_.emplace(asset.model_path, &asset);
        for (const auto& settlement : region.settlements()) {
            for (const auto& asset : settlement.assets) ids_.insert(asset.id);
            for (const auto& building : settlement.buildings) {
                const float r = std::max(building.size.x, building.size.z) * .5F + 3;
                taken_.push_back({building.position.x - r, building.position.z - r, building.position.x + r, building.position.z + r});
            }
            for (const auto& road : settlement.roads) roads_.push_back(road);
            for (const auto& marker : settlement.gameplay_markers)
                taken_.push_back({marker.position.x - 2, marker.position.z - 2, marker.position.x + 2, marker.position.z + 2});
        }
    }
    // Reserve a rectangular lot (world space, axis-aligned bounds); false if anything is there.
    bool claim(Vector3 centre, float half_x, float half_z, float yaw) {
        const float c = std::abs(std::cos(yaw * DEG2RAD)), s = std::abs(std::sin(yaw * DEG2RAD));
        const float hx = half_x * c + half_z * s, hz = half_x * s + half_z * c;
        const Lot lot{centre.x - hx, centre.z - hz, centre.x + hx, centre.z + hz};
        for (const auto& other : taken_)
            if (lot.x0 < other.x1 && lot.x1 > other.x0 && lot.z0 < other.z1 && lot.z1 > other.z0) {++report.skipped_lots; report.skipped.push_back(centre); return false;}
        // Keep lots off the streets (sampled along each road; long diagonal roads stay precise).
        for (const auto& road : roads_) {
            const float margin = road.width * .5F + 1.5F, length = Vector3Distance(road.start, road.end);
            for (float t = 0; t <= length; t += 2) {
                const auto p = Vector3Lerp(road.start, road.end, length > 0 ? t / length : 0);
                if (p.x > lot.x0 - margin && p.x < lot.x1 + margin && p.z > lot.z0 - margin && p.z < lot.z1 + margin) {++report.skipped_lots; report.skipped.push_back(centre); return false;}
            }
        }
        taken_.push_back(lot);
        return true;
    }
    bool put(const std::string& path, Vector3 position, float yaw, bool ground_y = true) {
        const auto found = by_path_.find(path);
        if (found == by_path_.end()) {
            if (std::find(report.missing.begin(), report.missing.end(), path) == report.missing.end()) report.missing.push_back(path);
            return false;
        }
        const auto& definition = *found->second;
        world::WorldAsset asset;
        for (int n = 1;; ++n) {
            asset.id = "creator_" + definition.id + "_kit" + std::to_string(n);
            if (ids_.insert(asset.id).second) break;
        }
        switch (definition.category) {
            case creator::CreatorAssetCategory::Road: asset.type = world::AssetType::Road; break;
            case creator::CreatorAssetCategory::Nature: asset.type = world::AssetType::Tree; break;
            case creator::CreatorAssetCategory::BuildingPart: asset.type = world::AssetType::Wall; break;
            case creator::CreatorAssetCategory::Building: asset.type = world::AssetType::House; break;
            default: asset.type = world::AssetType::Sign; break;
        }
        asset.model_path = path;
        asset.position = position;
        if (ground_y) asset.position.y = world::terrain::TerrainHeight::sample(position.x, position.z);
        // City models are designed around their own origin, but the renderer centres every model on
        // its bounds (awnings and cornices make them lopsided). Shift so the design origin lands here.
        if (path.find("/city/") != std::string::npos)
            if (const auto* mesh = world::physics::MeshCollisionLibrary::get(path)) {
                const float cx = (mesh->source_min.x + mesh->source_max.x) * .5F, cz = (mesh->source_min.z + mesh->source_max.z) * .5F;
                const float a = yaw * DEG2RAD;
                asset.position.x += cx * std::cos(a) + cz * std::sin(a);
                asset.position.z += -cx * std::sin(a) + cz * std::cos(a);
                asset.position.y += mesh->source_min.y;
            }
        asset.size = {definition.footprint.width, definition.footprint.height, definition.footprint.depth};
        asset.rotation_y = yaw;
        asset.vehicle.home = asset.position; asset.vehicle.home_yaw = yaw;
        asset.collision = definition.category != creator::CreatorAssetCategory::Road &&
            (definition.footprint.height > .25F || definition.category == creator::CreatorAssetCategory::BuildingPart);
        if (!region_.place_world_asset(std::move(asset))) return false;
        ++report.assets;
        return true;
    }
    // A procedural street tree (drawn by VerdaRegion; no model file).
    void tree(Vector3 position) {
        world::WorldAsset asset;
        for (int n = 1;; ++n) { asset.id = "creator_kit_tree_" + std::to_string(n); if (ids_.insert(asset.id).second) break; }
        asset.type = world::AssetType::Tree; asset.position = position; asset.size = {1, 7, 1}; asset.collision = true;
        if (region_.place_world_asset(std::move(asset))) ++report.assets;
    }
    WorldKitReport report;
private:
    world::VerdaRegion& region_;
    std::unordered_map<std::string, const creator::CreatorAssetDefinition*> by_path_;
    std::unordered_set<std::string> ids_;
    std::vector<Lot> taken_;
    std::vector<world::Road> roads_;
};

// Building-local (x right, z back) to world, using the same yaw convention as collision/drawing.
Vector3 to_world(Vector3 origin, float yaw, float x, float y, float z) {
    const float a = yaw * DEG2RAD;
    return {origin.x + x * std::cos(a) + z * std::sin(a), origin.y + y, origin.z - x * std::sin(a) + z * std::cos(a)};
}

std::uint32_t hash(std::uint32_t value) {
    value ^= value >> 16; value *= 0x7feb352d; value ^= value >> 15; value *= 0x846ca68b; return value ^ (value >> 16);
}

const std::string kit_floor = "assets/verda/kit/floor_slab_3x3.glb";
const std::string kit_roof = "assets/verda/kit/roof_slab_3x3.glb";

// One- or two-storey building from the 3 m Building Parts kit and the 3 m kit slabs.
// Width/depth in 3 m modules (at least 4 x 2). Front (-z) has the doorway; 8-wide buildings also
// get a back door. Two storeys get a staircase along the back wall under a 6 m gap in the floor.
bool modular_building(Kit& kit, Vector3 origin, float yaw, int modules_x, int modules_z, const std::string& style, int storeys, std::uint32_t seed) {
    const float w = modules_x * 3.0F, d = modules_z * 3.0F;
    if (!kit.claim(origin, w * .5F + 3, d * .5F + 3, yaw)) return false;
    origin.y = world::terrain::TerrainHeight::sample(origin.x, origin.z);
    const auto piece = [&](const std::string& name) { return parts + style + "_" + name + ".glb"; };
    const auto window = [&](int index) {
        const auto roll = hash(seed + static_cast<std::uint32_t>(index) * 7919U) % 5;
        return roll < 2 ? piece("wall_window_1") : roll < 4 ? piece("wall_window_2") : piece("wall");
    };
    constexpr float slab = .1F;
    // Stair well: modules 1 and 2 of the back row stay open on upper floors.
    const auto well = [&](int tx, int tz) { return tz == modules_z - 1 && (tx == 1 || tx == 2); };
    for (int storey = 0; storey < storeys; ++storey) {
        const float base = storey * 3.0F + slab;
        int index = storey * 100;
        // Front and back walls run along local X (piece yaw 90); sides run along Z (yaw 0).
        for (int i = 0; i < modules_x; ++i) {
            const float x = -w * .5F + 1.5F + i * 3;
            const bool door = storey == 0 && i == modules_x / 2;
            kit.put(door ? piece("wall_door_frame") : window(index++), to_world(origin, yaw, x, base, -d * .5F), yaw + 90, false);
            kit.put(storey == 0 && i == modules_x - 1 && modules_x >= 8 ? piece("wall_door_frame") : window(index++), to_world(origin, yaw, x, base, d * .5F), yaw + 90, false);
        }
        for (int i = 0; i < modules_z; ++i) {
            const float z = -d * .5F + 1.5F + i * 3;
            kit.put(window(index++), to_world(origin, yaw, -w * .5F, base, z), yaw, false);
            kit.put(window(index++), to_world(origin, yaw, w * .5F, base, z), yaw, false);
        }
        for (const float x : {-w * .5F, w * .5F}) for (const float z : {-d * .5F, d * .5F})
            kit.put(piece("wall_corner"), to_world(origin, yaw, x, base, z), yaw, false);
        for (int tx = 0; tx < modules_x; ++tx) for (int tz = 0; tz < modules_z; ++tz) {
            if (storey > 0 && well(tx, tz)) continue;
            kit.put(kit_floor, to_world(origin, yaw, -w * .5F + 1.5F + tx * 3, storey * 3.0F, -d * .5F + 1.5F + tz * 3), yaw, false);
        }
        if (storey + 1 < storeys) {
            // Stairs climb along local +X. Each piece is 0.2 m of foot then 1.0 m of 0.2 m treads, so
            // the flights overlap by the foot and the last tread ends at the edge of the next slab.
            const float sx = -w * .5F + 6.4F, sz = d * .5F - 1.5F;
            for (int flight = 0; flight < 3; ++flight)
                kit.put(shacks + "Stairs.glb", to_world(origin, yaw, sx + flight * 1.0F, base + flight * 1.0F, sz), yaw, false);
        }
    }
    for (int tx = 0; tx < modules_x; ++tx) for (int tz = 0; tz < modules_z; ++tz)
        kit.put(kit_roof, to_world(origin, yaw, -w * .5F + 1.5F + tx * 3, storeys * 3.0F, -d * .5F + 1.5F + tz * 3), yaw, false);
    ++kit.report.buildings;
    kit.report.enterable.push_back({origin, yaw, w, d, storeys, -w * .5F + 1.5F + (modules_x / 2) * 3.0F,
        {-w * .5F + 4.5F, slab, d * .5F - 1.5F}});
    return true;
}

// A straight run of fence pieces between two points (pieces span local X).
void fence(Kit& kit, Vector3 from, Vector3 to, const std::vector<std::string>& pieces, float length, std::uint32_t seed) {
    const float dx = to.x - from.x, dz = to.z - from.z, span = std::sqrt(dx * dx + dz * dz);
    const int count = std::max(1, static_cast<int>(std::round(span / length)));
    const float yaw = std::atan2(-dz, dx) * RAD2DEG;
    for (int i = 0; i < count; ++i) {
        const float t = (i + .5F) / count;
        kit.put(pieces[hash(seed + static_cast<std::uint32_t>(i)) % pieces.size()], {from.x + dx * t, 0, from.z + dz * t}, yaw);
    }
}

void prop(Kit& kit, const std::string& path, Vector3 at, float yaw, float half = 1.5F) {
    if (kit.claim(at, half, half, 0)) kit.put(path, at, yaw);
}

Vector3 site(std::string_view id) {
    const auto* found = world::layout::find(id);
    return found ? found->center : Vector3{};
}

// ------------------------------------------------------------------ downtown Verda
constexpr float pitch = 80, street = 12, pad_size = 68, pad_top = .15F;
constexpr float grid_edge = 2 * pitch + street * .5F; // outer street edge
const std::string city = "assets/verda/city/";

// Clear the old capital: its procedural buildings and local streets, every asset inside the grid,
// and reconnect the radial highways to the grid's edge instead of the old central crossing.
void wipe_downtown(world::VerdaRegion& region) {
    const auto inside = [](Vector3 p, float margin) { return std::abs(p.x) < grid_edge + margin && std::abs(p.z) < grid_edge + margin; };
    for (auto& settlement : region.runtime_settlements()) {
        std::erase_if(settlement.assets, [&](const world::WorldAsset& a) { return a.vehicle.definition.empty() && inside(a.position, 4); });
        if (settlement.id != "capital_verda") continue;
        settlement.buildings.clear();
        // The capital's hatchback waits in the southbound lane of the main avenue, clear of the curb.
        for (auto& marker : settlement.gameplay_markers)
            if (marker.id == "vehicle_spawn_hatchback_capital") {marker.position = {-3, marker.position.y, 22}; marker.rotation_y = 180;}
        std::vector<world::Road> kept;
        for (auto road : settlement.roads) {
            const bool a_in = inside(road.start, -1), b_in = inside(road.end, -1);
            if (a_in && b_in) continue; // old local streets
            if (a_in || b_in) {
                // Radial highway: start where it crosses the grid edge, on the matching main street.
                auto& near = a_in ? road.start : road.end;
                const auto& far = a_in ? road.end : road.start;
                if (std::abs(far.x) > std::abs(far.z)) near = {far.x > 0 ? grid_edge : -grid_edge, near.y, 0};
                else near = {0, near.y, far.z > 0 ? grid_edge : -grid_edge};
            }
            kept.push_back(road);
        }
        settlement.roads = std::move(kept);
    }
}

void downtown(Kit& kit, world::VerdaRegion& region) {
    world::Settlement* capital = nullptr;
    for (auto& settlement : region.runtime_settlements()) if (settlement.id == "capital_verda") capital = &settlement;
    if (!capital) return;
    const float ground = world::terrain::TerrainHeight::sample(0, 0);
    // Streets: five avenues and five streets, 12 m wide, plus crosswalk plates at every crossing.
    for (int i = -2; i <= 2; ++i) {
        const float c = i * pitch;
        capital->roads.push_back({{c, 0, -grid_edge}, {c, 0, grid_edge}, street, world::RoadType::Asphalt});
        capital->roads.push_back({{-grid_edge, 0, c}, {grid_edge, 0, c}, street, world::RoadType::Asphalt});
    }
    for (int i = -2; i <= 2; ++i) for (int j = -2; j <= 2; ++j)
        kit.put(city + "ground/intersection_12.glb", {i * pitch, ground, j * pitch}, 0, false);
    // Sixteen blocks: centre at (+-40, +-120). Each block is a 68 m pad (sidewalks included).
    enum class Zone { Tower, Square, MainStreet, Mixed, Parking };
    const auto zone = [](int bx, int bz) {
        const int ax = std::abs(bx), az = std::abs(bz);
        if (ax == 40 && az == 40) return bx < 0 && bz > 0 ? Zone::Square : Zone::Tower;
        if (ax == 120 && az == 120) return bx * bz > 0 ? Zone::Parking : Zone::Mixed;
        return (bx + bz) % 160 == 0 ? Zone::Mixed : Zone::MainStreet;
    };
    const auto at = [&](float x, float z) { return Vector3{x, ground + pad_top, z}; };
    // Rows of attached Main Street buildings along one block edge, fronts facing the street.
    struct Lot { const char* model; float width; float storeys; };
    const auto row = [&](float bx, float bz, int side, std::uint32_t seed) {
        // side: 0 = -z edge (faces -z), 1 = +x, 2 = +z, 3 = -x
        static constexpr const char* colors[]{"red", "brown", "tan", "cream", "dark"};
        const float yaw = side == 0 ? 0 : side == 1 ? 270 : side == 2 ? 180 : 90;
        float along = -30;
        for (int k = 0; along < 29; ++k) {
            const auto roll = hash(seed + static_cast<std::uint32_t>(k) * 31U);
            const bool wide = along <= 12 && roll % 3 == 0;
            const float width = wide ? 18 : 12;
            if (along + width > 30.01F) break;
            const char* color = colors[(roll / 3) % 5];
            int storeys = (roll / 15) % 2 ? 3 : 2;
            if (wide) storeys = 3;
            if (!wide && storeys == 3 && (std::string(color) == "dark" || std::string(color) == "cream")) storeys = 2;
            const std::string model = city + "main_street/main_street_" + color + "_" + (wide ? "18" : "12") + "m_" + std::to_string(storeys) + "st.glb";
            const float mid = along + width * .5F, inset = -30 + 10; // 20 m deep, front on the lot line
            Vector3 position{};
            if (side == 0) position = at(bx + mid, bz + inset);
            if (side == 2) position = at(bx - mid, bz - inset);
            if (side == 1) position = at(bx - inset, bz + mid);
            if (side == 3) position = at(bx + inset, bz - mid);
            if (kit.put(model, position, yaw, false)) {
                constexpr float floor = 3.6F, slab = .2F, wall = .3F;
                const float w = width * .5F;
                // Doors are in the first bay pair: local x = -4.5 on both widths.
                kit.report.enterable.push_back({position, yaw, width, 20, storeys, -4.5F,
                    {w - wall - .65F, slab, -10 + 1.5F}, {0, 0, 1}, 5.04F + 2.2F, slab, floor + slab, {-3, 0, 0}});
                ++kit.report.buildings;
            }
            along += width;
        }
    };
    int parked = 0;
    for (const float bx : {-120.0F, -40.0F, 40.0F, 120.0F}) for (const float bz : {-120.0F, -40.0F, 40.0F, 120.0F}) {
        const auto kind = zone(static_cast<int>(bx), static_cast<int>(bz));
        const auto seed = static_cast<std::uint32_t>((bx + 200) * 7 + (bz + 200) * 13);
        const char* surface = kind == Zone::Square ? "ground/courthouse_square_68.glb" : kind == Zone::Parking ? "ground/parking_lot_68.glb" : "ground/sidewalk_block_68.glb";
        kit.put(city + surface, {bx, ground, bz}, 0, false);
        // Rows face the main axes (x = 0, z = 0) first, then the outer streets.
        const int toward_x = bx > 0 ? 3 : 1, toward_z = bz > 0 ? 0 : 2;
        switch (kind) {
            case Zone::Tower: {
                // The tallest towers stand at the core's inner corners, a mid-rise beside each.
                const bool bank = bx > 0 && bz < 0, deco = bx < 0 && bz < 0;
                const char* tall = bank ? "glass_dark_tower" : deco ? "stone_deco_tower" : "glass_blue_tower";
                const char* low = bank ? "concrete_office" : deco ? "glass_blue_midrise" : "stone_midrise";
                kit.put(city + "towers/" + tall + ".glb", at(bx - (bx > 0 ? 14 : -14), bz - (bz > 0 ? 14 : -14)), bx > 0 ? 90 : 270, false);
                kit.put(city + "towers/" + low + ".glb", at(bx + (bx > 0 ? 14 : -14), bz + (bz > 0 ? 14 : -14)), 0, false);
                // The other two quadrants are office plazas: a grove of trees and benches.
                for (const float sx : {-1.0F, 1.0F}) {
                    const float qx = bx + sx * (bx > 0 ? 14 : -14), qz = bz - sx * (bz > 0 ? 14 : -14);
                    for (const auto& [dx, dz] : std::initializer_list<std::pair<float, float>>{{-6, -6}, {6, -6}, {-6, 6}, {6, 6}, {0, 0}})
                        kit.tree(at(qx + dx, qz + dz));
                    kit.put(urban + "Bus stops/busstop_bench.glb", at(qx + 3, qz), sx > 0 ? 90.0F : 270.0F, false);
                }
                break;
            }
            case Zone::MainStreet:
                row(bx, bz, std::abs(bx) == 40 ? toward_x : toward_z, seed);
                row(bx, bz, std::abs(bx) == 40 ? (toward_x + 2) % 4 : (toward_z + 2) % 4, seed + 7);
                break;
            case Zone::Mixed:
                row(bx, bz, std::abs(bx) == 40 || (std::abs(bz) == 120 && std::abs(bx) == 120) ? toward_x : toward_z, seed);
                kit.put(city + "towers/" + (seed % 2 ? "brick_midrise" : "concrete_office") + ".glb",
                    at(bx + (bx > 0 ? 12 : -12) * (std::abs(bx) == 40 ? 1.0F : 0.0F), bz + (bz > 0 ? 12 : -12) * (std::abs(bx) == 40 ? 0.0F : 1.0F)), 0, false);
                break;
            case Zone::Square:
                // Courthouse square: lawn, crossing paths and a monument; trees round the edge.
                for (int i = 0; i < 4; ++i) for (const float t : {-22.0F, 22.0F}) {
                    const float edge = 27;
                    kit.tree(at(bx + (i % 2 ? t : (i < 2 ? -edge : edge)), bz + (i % 2 ? (i < 2 ? -edge : edge) : t)));
                }
                kit.put(urban + "Bus stops/busstop_clean_bench.glb", at(bx + 31, bz + 12), 270, false);
                break;
            case Zone::Parking:
                break;
        }
        // Street trees and lights on every sidewalk (the square has its own ring of trees).
        for (int side = 0; side < 4; ++side) {
            const float edge = pad_size * .5F - 1.6F;
            const auto spot = [&](float along) {
                return side == 0 ? at(bx + along, bz - edge) : side == 1 ? at(bx + edge, bz + along) : side == 2 ? at(bx - along, bz + edge) : at(bx - edge, bz - along);
            };
            if (kind != Zone::Square) for (const float along : {-16.0F, 16.0F}) kit.tree(spot(along));
            kit.put(urban + "Traffic lights/street_light.glb", spot(0), side == 0 ? 0.0F : side == 1 ? 270.0F : side == 2 ? 180.0F : 90.0F, false);
        }
        // Parallel-parked cars on the curb lanes beside Main Street blocks: real, drivable vehicles.
        if ((kind == Zone::MainStreet || kind == Zone::Mixed) && std::abs(bx) + std::abs(bz) == 160) for (const float along : {8.0F, -18.0F}) {
            const bool along_x = std::abs(bx) == 40;
            const float curb = pad_size * .5F + 2.2F;
            const Vector3 p = along_x ? Vector3{bx + (bx > 0 ? -curb : curb), ground, bz + along} : Vector3{bx + along, ground, bz + (bz > 0 ? -curb : curb)};
            world::GameplayMarker marker;
            marker.id = "vehicle_spawn_hatchback_downtown_" + std::to_string(++parked);
            marker.type = world::GameplayMarkerType::VehicleSpawn;
            marker.position = p; marker.size = {2, 1.6F, 4.4F}; marker.rotation_y = along_x ? 0 : 90;
            capital->gameplay_markers.push_back(marker);
        }
    }
    // Signals at the core's nine crossings, on opposite corners.
    for (int i = -1; i <= 1; ++i) for (int j = -1; j <= 1; ++j) {
        const float x = i * pitch, z = j * pitch, o = street * .5F + 1.2F;
        kit.put(urban + "Traffic lights/traffic_light.glb", at(x + o, z - o), 0, false);
        kit.put(urban + "Traffic lights/traffic_light.glb", at(x - o, z + o), 180, false);
    }
    for (const auto& [x, z, yaw] : std::initializer_list<std::tuple<float, float, float>>{{9, 46, 270}, {-9, -110, 90}, {110, -9, 0}})
        kit.put(urban + "Bus stops/busstop_two_seats.glb", at(x, z), yaw, false);
    // Keep the rest of the kit off downtown.
    kit.claim({0, 0, 0}, grid_edge + 2, grid_edge + 2, 0);
}

void roka(Kit& kit) {
    const auto c = site("west_roka");
    // Industrial yard west of town (clear of the main street at z = 0): sheds, shacks, fenced compound.
    for (const auto& [x, z, name, yaw] : std::initializer_list<std::tuple<float, float, const char*, float>>{
            {-62, -40, "Large Shed A", 0}, {-62, 30, "Large Shed B", 0}, {-62, 68, "Large Shed A", 90}})
        if (kit.claim({c.x + x, 0, c.z + z}, 4.4F, 7.2F, yaw)) kit.put(shacks + name + ".glb", {c.x + x, 0, c.z + z}, yaw);
    for (const auto& [x, z, name, yaw] : std::initializer_list<std::tuple<float, float, const char*, float>>{
            {-88, -62, "Shack A", 0}, {-80, -62, "Leanto A", 0}, {-88, 20, "Leanto B", 90}, {-88, 45, "Shack A", 180}})
        if (kit.claim({c.x + x, 0, c.z + z}, 2, 3.2F, yaw)) kit.put(shacks + name + ".glb", {c.x + x, 0, c.z + z}, yaw);
    modular_building(kit, {c.x + 50, 0, c.z - 40}, 270, 8, 4, "brick", 2, 21); // works office
    modular_building(kit, {c.x + 50, 0, c.z + 30}, 270, 4, 4, "stuco", 1, 22);
    const std::vector<std::string> chain{shacks + "Chainlink Fence.glb"};
    fence(kit, {c.x - 96, 0, c.z - 75}, {c.x - 40, 0, c.z - 75}, chain, 2.46F, 3);
    fence(kit, {c.x - 96, 0, c.z - 75}, {c.x - 96, 0, c.z - 8}, chain, 2.46F, 4);
    fence(kit, {c.x - 96, 0, c.z + 8}, {c.x - 96, 0, c.z + 85}, chain, 2.46F, 5);
    fence(kit, {c.x - 96, 0, c.z + 85}, {c.x - 40, 0, c.z + 85}, chain, 2.46F, 6);
    for (int i = 0; i < 12; ++i) {
        const auto r = hash(300U + static_cast<std::uint32_t>(i));
        const float z = -70 + static_cast<float>((r / 31) % 150);
        if (std::abs(z) < 7) continue;
        prop(kit, shacks + (i % 3 == 0 ? "Pallet.glb" : "Barrel.glb"), {c.x - 50 + static_cast<float>(r % 6), 0, c.z + z}, static_cast<float>(r % 360), .8F);
    }
    for (int i = 0; i < 4; ++i) prop(kit, shacks + "Concrete Barricade.glb", {c.x - 44, 0, c.z - 60 + i * 9.0F}, 90, 1.1F);
    prop(kit, urban + "Litter/Dumpsters/dumpster_lid_closed_red.glb", {c.x - 46, 0, c.z + 50}, 90, 2);
    prop(kit, urban + "Litter/Dumpsters/dumpster_full_no_lid.glb", {c.x - 46, 0, c.z + 58}, 90, 2);
    for (int i = 0; i < 4; ++i) kit.put(urban + "Pipes/pipe_rusted.glb", {c.x - 75, 0, c.z - 25 + i * 2.0F}, 0);
}

void porto_luma(Kit& kit) {
    const auto c = site("port_luma");
    // Waterfront to the east: warehouses, a quay of bollards and barriers, and stacked cargo.
    for (const auto& [x, z, name] : std::initializer_list<std::tuple<float, float, const char*>>{
            {60, -45, "Large Shed B"}, {60, 25, "Large Shed A"}, {60, 60, "Large Shed B"}})
        if (kit.claim({c.x + x, 0, c.z + z}, 4.4F, 7.2F, 0)) kit.put(shacks + std::string(name) + ".glb", {c.x + x, 0, c.z + z}, 0);
    modular_building(kit, {c.x - 50, 0, c.z - 35}, 90, 8, 4, "stuco", 2, 31); // harbour office
    modular_building(kit, {c.x - 50, 0, c.z + 35}, 90, 4, 4, "brick", 1, 32);
    for (int i = 0; i < 16; ++i) kit.put(urban + "Cones & Barriers/Bollards/square_bollard.glb", {c.x + 82, 0, c.z - 60 + i * 8.0F}, 0);
    for (int i = 0; i < 6; ++i) prop(kit, urban + "Cones & Barriers/Concrete Barriers/Jersey_barrier_dirty.glb", {c.x + 75, 0, c.z - 50 + i * 20.0F}, 90, 1.4F);
    for (int i = 0; i < 14; ++i) {
        const auto r = hash(500U + static_cast<std::uint32_t>(i));
        const Vector3 at{c.x + 70 + static_cast<float>(r % 8), 0, c.z - 55 + static_cast<float>((r / 9) % 110)};
        prop(kit, shacks + (i % 2 ? "Pallet.glb" : "Barrel.glb"), at, static_cast<float>(r % 360), 1);
    }
    prop(kit, urban + "Bus stops/busstop_two_seats.glb", {c.x + 9, 0, c.z - 25}, 90, 2.5F);
    for (int i = -3; i <= 3; ++i) kit.put(urban + "Traffic lights/street_light_clean.glb", {c.x + 6.5F, 0, c.z + i * 25.0F + 12}, 90);
}

void suda_haveno(Kit& kit) {
    const auto c = site("south_haven");
    // Residential lanes: small homes with picket-fenced front gardens.
    int seed = 41;
    for (const auto& [x, z, yaw, style] : std::initializer_list<std::tuple<float, float, float, const char*>>{
            {-50, -30, 90, "stuco"}, {-50, 25, 90, "brick"}, {50, -30, 270, "fancy_brick"}, {50, 25, 270, "stuco"}}) {
        const Vector3 at{c.x + x, 0, c.z + z};
        if (!modular_building(kit, at, yaw, 4, 4, style, seed % 2 ? 2 : 1, static_cast<std::uint32_t>(seed))) {++seed; continue;}
        // Garden fence 4 m in front of the doorway, gate in the middle.
        const Vector3 left = to_world(at, yaw, -6, 0, -10), right = to_world(at, yaw, 6, 0, -10);
        const Vector3 gate_l = to_world(at, yaw, -1.2F, 0, -10), gate_r = to_world(at, yaw, 1.2F, 0, -10);
        fence(kit, left, gate_l, {urban + "Walls & Fences/white_picket_fence/white_picket_fence_closed_left.glb"}, 2.38F, static_cast<std::uint32_t>(seed));
        fence(kit, gate_r, right, {urban + "Walls & Fences/white_picket_fence/white_picket_fence_closed_right.glb"}, 2.38F, static_cast<std::uint32_t>(seed + 1));
        kit.put(urban + "Litter/TrashCan/trash_can_alt1.glb", to_world(at, yaw, 7.5F, 0, -8), yaw);
        ++seed;
    }
    prop(kit, urban + "Bus stops/busstop_round_roof.glb", {c.x + 9, 0, c.z + 60}, 90, 2.5F);
    for (int i = -3; i <= 3; ++i) kit.put(urban + "Traffic lights/street_light.glb", {c.x - 6.5F, 0, c.z + i * 25.0F + 5}, -90);
}

void espera(Kit& kit) {
    const auto c = site("village_espera");
    // Farmland: drystone field walls, a lean-to barn and post-and-rail paddock fences.
    const std::vector<std::string> stone{urban + "Walls & Fences/drystone_wall/drystone_wall.glb", urban + "Walls & Fences/drystone_wall/drystone_wall_capped.glb"};
    fence(kit, {c.x - 90, 0, c.z + 40}, {c.x - 40, 0, c.z + 40}, stone, 2.0F, 61);
    fence(kit, {c.x - 90, 0, c.z + 40}, {c.x - 90, 0, c.z + 90}, stone, 2.0F, 62);
    const std::vector<std::string> rails{urban + "Fences/low_wooden_fence/wooden_fence_closed.glb", urban + "Fences/low_wooden_fence/wooden_fence_open.glb",
        urban + "Fences/low_wooden_fence/wooden_fence_broken.glb"};
    fence(kit, {c.x + 40, 0, c.z + 45}, {c.x + 85, 0, c.z + 45}, rails, 2.2F, 63);
    fence(kit, {c.x + 85, 0, c.z + 45}, {c.x + 85, 0, c.z + 85}, rails, 2.2F, 64);
    if (kit.claim({c.x - 60, 0, c.z + 65}, 3, 4, 0)) kit.put(shacks + "Leanto B.glb", {c.x - 60, 0, c.z + 65}, 0);
    if (kit.claim({c.x + 60, 0, c.z + 65}, 3, 4, 0)) kit.put(shacks + "Shack A.glb", {c.x + 60, 0, c.z + 65}, 90);
    modular_building(kit, {c.x - 55, 0, c.z - 45}, 90, 4, 4, "stuco", 1, 65);
    modular_building(kit, {c.x + 55, 0, c.z - 45}, 270, 4, 4, "brick", 2, 66);
    prop(kit, urban + "Bus stops/busstop_single_seat.glb", {c.x + 9, 0, c.z + 34}, 90, 2.5F);
}
}

WorldKitReport build_verda_towns(world::VerdaRegion& region, const creator::CreatorAssetRegistry& catalog) {
    wipe_downtown(region);
    Kit kit(region, catalog);
    downtown(kit, region);
    roka(kit);
    porto_luma(kit);
    suda_haveno(kit);
    espera(kit);
    return kit.report;
}
}
