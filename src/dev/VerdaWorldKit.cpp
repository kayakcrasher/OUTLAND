#include "outland/dev/VerdaWorldKit.hpp"
#include "outland/creator/CreatorAssetRegistry.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/ParkingGarage.hpp"
#include "outland/world/Ballparks.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/MotorTrade.hpp"
#include "outland/world/Ranches.hpp"
#include "outland/world/Zones.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/roads/RoadGraph.hpp"
#include "outland/world/roads/Turnaround.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <functional>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace outland::dev {
namespace {
const std::string parts = "assets/verda/urban/Building Parts/";
const std::string shacks = "assets/verda/industrial/shacks/";
const std::string prebuild = "assets/verda/urban/Buildings Prebuild/";
const std::string urban = "assets/verda/urban/";

struct Lot { float x0, z0, x1, z1; };

class Kit {
public:
    Kit(world::VerdaRegion& region, const creator::CreatorAssetRegistry& catalog) : region_(region) {
        for (const auto& asset : catalog.assets()) if (!asset.model_path.empty()) by_path_.emplace(asset.model_path, &asset);
        for (const auto& settlement : region.settlements()) {
            for (const auto& asset : settlement.assets) ids_.insert(asset.id);
            for (const auto& building : settlement.buildings) taken_.push_back(building_lot(building));
            for (const auto& road : settlement.roads) roads_.push_back(road);
            for (const auto& marker : settlement.gameplay_markers)
                taken_.push_back({marker.position.x - 2, marker.position.z - 2, marker.position.x + 2, marker.position.z + 2});
        }
    }
    // The lot a procedural building reserved (its footprint plus a 3 m margin).
    static Lot building_lot(const world::Building& building) {
        const float r = std::max(building.size.x, building.size.z) * .5F + 3;
        return {building.position.x - r, building.position.z - r, building.position.x + r, building.position.z + r};
    }
    // Would claim() succeed? (Reserves nothing.)
    bool fits(Vector3 centre, float half_x, float half_z, float yaw) {
        const auto report_before = report.skipped_lots;
        const auto skipped_before = report.skipped.size();
        const auto taken_before = taken_.size();
        const bool ok = claim(centre, half_x, half_z, yaw);
        if (ok) taken_.resize(taken_before);
        report.skipped_lots = report_before;
        report.skipped.resize(skipped_before);
        return ok;
    }
    // Reserve a rectangular lot (world space, axis-aligned bounds); false if anything is there.
    // `own` is a lot the new one may overlap (a driveway inside its house's margin).
    bool claim(Vector3 centre, float half_x, float half_z, float yaw, const Lot* own = nullptr) {
        const float c = std::abs(std::cos(yaw * DEG2RAD)), s = std::abs(std::sin(yaw * DEG2RAD));
        const float hx = half_x * c + half_z * s, hz = half_x * s + half_z * c;
        const Lot lot{centre.x - hx, centre.z - hz, centre.x + hx, centre.z + hz};
        for (const auto& other : taken_) {
            if (own && other.x0 == own->x0 && other.z0 == own->z0 && other.x1 == own->x1 && other.z1 == own->z1) continue;
            if (lot.x0 < other.x1 && lot.x1 > other.x0 && lot.z0 < other.z1 && lot.z1 > other.z0) {++report.skipped_lots; report.skipped.push_back(centre); return false;}
        }
        // Keep lots off the streets (sampled along each road), tested against the lot's real,
        // rotated rectangle so lots beside a diagonal road are not refused by their bounding box.
        const float ca = std::cos(yaw * DEG2RAD), sa = std::sin(yaw * DEG2RAD);
        for (const auto& road : roads_) {
            const float margin = road.width * .5F + 1.5F, length = Vector3Distance(road.start, road.end);
            for (float t = 0; t <= length; t += 2) {
                const auto p = Vector3Lerp(road.start, road.end, length > 0 ? t / length : 0);
                if (!(p.x > lot.x0 - margin && p.x < lot.x1 + margin && p.z > lot.z0 - margin && p.z < lot.z1 + margin)) continue;
                const float dx = p.x - centre.x, dz = p.z - centre.z;
                const float lx = dx * ca - dz * sa, lz = dx * sa + dz * ca; // world -> lot local (to_world inverse)
                if (std::abs(lx) < half_x + margin && std::abs(lz) < half_z + margin) {++report.skipped_lots; report.skipped.push_back(centre); return false;}
            }
        }
        taken_.push_back(lot);
        return true;
    }
    // `solid` false: nothing collides with it (a gantry over a road: vehicles take a model's whole bounds as solid).
    bool put(const std::string& path, Vector3 position, float yaw, bool ground_y = true, bool solid = true) {
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
        asset.collision = solid && definition.category != creator::CreatorAssetCategory::Road &&
            (definition.footprint.height > .25F || definition.category == creator::CreatorAssetCategory::BuildingPart);
        if (!region_.place_world_asset(std::move(asset))) return false;
        ++report.assets;
        return true;
    }
    // A procedural street tree (drawn by VerdaRegion; no model file).
    void tree(Vector3 position) {
        world::WorldAsset asset;
        for (int n = 1;; ++n) { asset.id = "creator_kit_tree_" + std::to_string(n); if (ids_.insert(asset.id).second) break; }
        // Never on a kerb a car would clip: the trunk stands clear of every carriageway (the id is
        // spent either way, so the trees after keep theirs).
        for (const auto& town : region_.settlements())
            for (const auto& road : town.roads) {
                const float dx = road.end.x - road.start.x, dz = road.end.z - road.start.z, l2 = dx * dx + dz * dz;
                const float t = l2 > 0 ? std::clamp(((position.x - road.start.x) * dx + (position.z - road.start.z) * dz) / l2, 0.0F, 1.0F) : 0;
                const float d = std::hypot(position.x - road.start.x - dx * t, position.z - road.start.z - dz * t);
                if (road.type == world::RoadType::Highway && d < world::highways::median_half + .3F) continue;   // the planted median
                if (d < road.width * .5F + .9F) return;
            }
        asset.type = world::AssetType::Tree; asset.position = position; asset.size = {1, 7, 1}; asset.collision = true;
        if (region_.place_world_asset(std::move(asset))) ++report.assets;
    }
    // What a part-built building is for, for Explore's island life: a purpose marker just inside
    // the front door, facing out - exactly what a builder places by hand.
    // `id`: a fixed id instead of the next numbered one (a building the game looks for by name).
    void purpose(Vector3 at, float yaw, world::BuildingPurpose purpose, const std::string& id = {}) {
        auto& settlements = region_.editable_settlements();
        if (settlements.empty()) return;
        world::GameplayMarker marker;
        if (!id.empty() && ids_.insert(id).second) marker.id = id;
        else for (int n = 1;; ++n) { marker.id = "creator_marker_building_purpose_kit_" + std::to_string(n); if (ids_.insert(marker.id).second) break; }
        marker.type = world::GameplayMarkerType::BuildingPurpose; marker.position = at; marker.rotation_y = yaw;
        marker.size = {1.2F, 2.2F, 1.2F}; marker.purpose = purpose;
        settlements.front().gameplay_markers.push_back(std::move(marker));
        ++report.purposes;
    }
    // Forget the streets and lots of a district that is being rebuilt from scratch.
    void clear_area(Vector3 centre, float half) {
        const auto in = [&](Vector3 p) { return std::abs(p.x - centre.x) <= half && std::abs(p.z - centre.z) <= half; };
        std::erase_if(roads_, [&](const world::Road& r) { return in(r.start) && in(r.end); });
        std::erase_if(taken_, [&](const Lot& l) { return in({l.x0, 0, l.z0}) && in({l.x1, 0, l.z1}); });
    }
    void forget_road(const world::Road& old) {
        std::erase_if(roads_, [&](const world::Road& r) { return Vector3Equals(r.start, old.start) && Vector3Equals(r.end, old.end); });
    }
    // Streets laid after the kit was opened keep later lots off them too.
    void add_roads(const std::vector<world::Road>& roads) {
        for (const auto& road : roads)
            if (std::find_if(roads_.begin(), roads_.end(), [&](const world::Road& r) {
                    return Vector3Equals(r.start, road.start) && Vector3Equals(r.end, road.end); }) == roads_.end())
                roads_.push_back(road);
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
bool modular_building(Kit& kit, Vector3 origin, float yaw, int modules_x, int modules_z, const std::string& style, int storeys, std::uint32_t seed,
                      world::BuildingPurpose purpose = world::BuildingPurpose::Home, float lift = 0) {
    const float w = modules_x * 3.0F, d = modules_z * 3.0F;
    if (!kit.claim(origin, w * .5F + 3, d * .5F + 3, yaw)) return false;
    origin.y = world::terrain::TerrainHeight::sample(origin.x, origin.z) + lift;   // lift: on a city block's paving
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
    const float door_x = -w * .5F + 1.5F + (modules_x / 2) * 3.0F;
    kit.purpose(to_world(origin, yaw, door_x, slab, -d * .5F + 1.2F), yaw, purpose);
    kit.report.enterable.push_back({origin, yaw, w, d, storeys, door_x,
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
// (pitch and street are the county towns' 80 m grid of 12 m streets.)
constexpr float pitch = 80, street = 12, pad_size = 68, pad_top = .15F;
// Downtown Verda City: wide 16 m streets, and the north-south highway straight down the middle as
// a planted boulevard (layout::downtown_*). Four columns of 68 m blocks either side of it, four rows
// either side of the main cross street; every street crosses the highway at grade.
constexpr float dt_street = 16;
constexpr std::array<float, 4> dt_block_x{-129, -45, 45, 129}, dt_block_z{-126, -42, 42, 126};
constexpr std::array<float, 5> dt_street_x{-171, -87, 0, 87, 171}, dt_street_z{-168, -84, 0, 84, 168};
constexpr float grid_edge_x = world::layout::downtown_half_x, grid_edge_z = world::layout::downtown_half_z;
const std::string city = "assets/verda/city/";

// Clear the old capital: its procedural buildings and local streets, every asset inside the grid,
// and reconnect the radial highways to the grid's edge instead of the old central crossing.
void wipe_downtown(world::VerdaRegion& region) {
    const auto inside = [](Vector3 p, float margin) { return std::abs(p.x) < grid_edge_x + margin && std::abs(p.z) < grid_edge_z + margin; };
    for (auto& settlement : region.runtime_settlements()) {
        std::erase_if(settlement.assets, [&](const world::WorldAsset& a) { return a.vehicle.definition.empty() && inside(a.position, 4); });
        if (settlement.id != "capital_verda") continue;
        settlement.buildings.clear();
        // The capital's hatchback waits at the kerb of the main cross street (the avenue is the highway now).
        for (auto& marker : settlement.gameplay_markers)
            if (marker.id == "vehicle_spawn_hatchback_capital") {marker.position = {-60, marker.position.y, 6.8F}; marker.rotation_y = 90;}
        std::vector<world::Road> kept;
        for (auto road : settlement.roads) {
            const bool a_in = inside(road.start, -1), b_in = inside(road.end, -1);
            if (a_in && b_in) continue; // old local streets
            if (a_in || b_in) {
                // Radial highway: start where it crosses the grid edge, on the matching main street.
                auto& near = a_in ? road.start : road.end;
                const auto& far = a_in ? road.end : road.start;
                if (std::abs(far.x) > std::abs(far.z)) near = {far.x > 0 ? grid_edge_x : -grid_edge_x, near.y, 0};
                else near = {0, near.y, far.z > 0 ? grid_edge_z : -grid_edge_z};
            }
            kept.push_back(road);
        }
        settlement.roads = std::move(kept);
    }
}

// Attached Main Street buildings along one edge of a 68 m block, fronts on the street.
void main_street_row(Kit& kit, float ground, float bx, float bz, int side, std::uint32_t seed) {
    const auto at = [&](float x, float z) { return Vector3{x, ground + pad_top, z}; };
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
}

// The highway's run through downtown as a boulevard: plane trees and double lamps down the planted
// median between the crossings (the renderer draws the kerb, lawn and hedges there), and a sign
// gantry over each end where it enters the grid (the boards are runtime signs).
void boulevard(Kit& kit, float ground) {
    const auto at = [&](float x, float z) { return Vector3{x, ground + .25F, z}; };
    for (std::size_t j = 0; j + 1 < dt_street_z.size(); ++j) {
        const float z0 = dt_street_z[j] + 20, z1 = dt_street_z[j + 1] - 20;
        for (int k = 0; k <= 3; ++k) {
            const float z = z0 + (z1 - z0) * static_cast<float>(k) / 3.0F;
            if (k % 3 == 0) {
                kit.put(urban + "Traffic lights/street_light.glb", at(0, z), 90, false);
                kit.put(urban + "Traffic lights/street_light.glb", at(0, z), 270, false);
            } else kit.tree(at(0, z));
        }
    }
    // Lamps and trees on the verges too, the length of the run.
    for (std::size_t j = 0; j + 1 < dt_street_z.size(); ++j)
        for (const float side : {-1.0F, 1.0F})
            for (const float f : {.35F, .65F})
                kit.tree(at(side * (world::highways::paved_half + 1.0F), dt_street_z[j] + (dt_street_z[j + 1] - dt_street_z[j]) * f));
    for (const float z : {-grid_edge_z - 14, grid_edge_z + 14})
        kit.put(city + "civic/highway_gantry.glb", {0, world::terrain::TerrainHeight::sample(0, z), z}, 0, false, false);
}

void downtown(Kit& kit, world::VerdaRegion& region) {
    world::Settlement* capital = nullptr;
    for (auto& settlement : region.runtime_settlements()) if (settlement.id == "capital_verda") capital = &settlement;
    if (!capital) return;
    const float ground = world::terrain::TerrainHeight::sample(0, 0);
    // Streets: four avenues and five streets, 16 m wide (the highway is the fifth avenue, x = 0),
    // and crosswalk plates where two streets cross (the highway lays its own apron).
    for (const float x : dt_street_x)
        if (x != 0) capital->roads.push_back({{x, 0, -grid_edge_z}, {x, 0, grid_edge_z}, dt_street, world::RoadType::Asphalt});
    for (const float z : dt_street_z) capital->roads.push_back({{-grid_edge_x, 0, z}, {grid_edge_x, 0, z}, dt_street, world::RoadType::Asphalt});
    for (const float x : dt_street_x) for (const float z : dt_street_z)
        if (x != 0) kit.put(city + "ground/intersection_16.glb", {x, ground, z}, 0, false);
    // Sixteen blocks, each a 68 m pad (sidewalks included). Inner/outer by column and row.
    enum class Zone { Tower, Square, MainStreet, Mixed, Parking };
    const auto zone = [](float bx, float bz) {
        const bool ix = std::abs(bx) < 90, iz = std::abs(bz) < 90;
        if (ix && iz) return bx < 0 && bz > 0 ? Zone::Square : Zone::Tower;
        if (!ix && !iz) return bx * bz > 0 ? Zone::Parking : Zone::Mixed;
        return (bx > 0) == (bz > 0) ? Zone::Mixed : Zone::MainStreet;
    };
    const auto at = [&](float x, float z) { return Vector3{x, ground + pad_top, z}; };
    const auto row = [&](float bx, float bz, int side, std::uint32_t seed) { main_street_row(kit, ground, bx, bz, side, seed); };
    int parked = 0;
    for (std::size_t ix = 0; ix < 4; ++ix) for (std::size_t iz = 0; iz < 4; ++iz) {
        const float bx = dt_block_x[ix], bz = dt_block_z[iz];
        const bool inner_x = ix == 1 || ix == 2, inner_z = iz == 1 || iz == 2;
        const auto kind = zone(bx, bz);
        // (Seeds from the old 80 m grid's block centres, so each block keeps its buildings.)
        const float gx = inner_x ? (bx > 0 ? 40.0F : -40.0F) : (bx > 0 ? 120.0F : -120.0F);
        const float gz = inner_z ? (bz > 0 ? 40.0F : -40.0F) : (bz > 0 ? 120.0F : -120.0F);
        const auto seed = static_cast<std::uint32_t>((gx + 200) * 7 + (gz + 200) * 13);
        const char* surface = kind == Zone::Square ? "ground/courthouse_square_68.glb" : kind == Zone::Parking ? "ground/parking_lot_68.glb" : "ground/sidewalk_block_68.glb";
        kit.put(city + surface, {bx, ground, bz}, 0, false);
        // Rows face the main axes (x = 0, z = 0) first, then the outer streets.
        const int toward_x = bx > 0 ? 3 : 1, toward_z = bz > 0 ? 0 : 2;
        switch (kind) {
            case Zone::Tower: {
                // The tallest towers stand at the core's inner corners, a mid-rise beside each.
                const bool bank = bx > 0 && bz < 0, deco = bx < 0 && bz < 0;
                // The deco corner is Stela Turo, Verda's art deco skyscraper (enterable, 17 floors, a
                // penthouse for sale in Explore): its offices and loot are set up in government().
                const char* tall = bank ? "glass_dark_tower" : deco ? "stela_turo" : "glass_blue_tower";
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
                row(bx, bz, inner_x ? toward_x : toward_z, seed);
                row(bx, bz, inner_x ? (toward_x + 2) % 4 : (toward_z + 2) % 4, seed + 7);
                break;
            case Zone::Mixed:
                row(bx, bz, inner_x || (!inner_z && !inner_x) ? toward_x : toward_z, seed);
                kit.put(city + "towers/" + (seed % 2 ? "brick_midrise" : "concrete_office") + ".glb",
                    at(bx + (bx > 0 ? 12 : -12) * (inner_x ? 1.0F : 0.0F), bz + (bz > 0 ? 12 : -12) * (inner_x ? 0.0F : 1.0F)), 0, false);
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
                // The south-west lot: Parkumejo Centra, the multi-storey car park with the mansion on its
                // roof (world::garage), its entrance and front on the inner street (north).
                if (bx < 0 && bz < 0 && kit.put(world::garage::model, at(bx, bz), 180, false)) {
                    kit.report.parking_garage = true;
                    kit.report.parking_garage_at = at(bx, bz);
                    // Cars parked on its levels (the island's drivers leave them be: "_garage_").
                    const auto bay = [&](int level, float x, int slot) {
                        auto p = to_world(at(bx, bz), 180, x, world::garage::decks[static_cast<std::size_t>(level)], -13.2F + 2.6F * static_cast<float>(slot));
                        world::GameplayMarker car;
                        car.id = std::string("vehicle_spawn_") + (slot % 3 ? "hatchback" : "sedan") + "_garage_" + std::to_string(level) + "_" + std::to_string(slot);
                        car.type = world::GameplayMarkerType::VehicleSpawn;
                        car.position = p; car.size = {2, 1.6F, 4.4F}; car.rotation_y = x < -10 ? 270.0F : 90.0F;
                        capital->gameplay_markers.push_back(car);
                    };
                    for (const auto& [level, x, slot] : std::initializer_list<std::tuple<int, float, int>>{
                             {0, -15.9F, 1}, {0, -3.9F, 4}, {1, -15.9F, 0}, {1, -15.9F, 3}, {1, -3.9F, 7}, {2, -15.9F, 5}, {2, -3.9F, 2}})
                        bay(level, x, slot);
                }
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
        // Parallel-parked cars on the curb lanes beside Main Street blocks: real, drivable vehicles
        // (on the street away from the highway for the blocks beside it).
        if ((kind == Zone::MainStreet || kind == Zone::Mixed) && inner_x != inner_z) for (const float along : {8.0F, -18.0F}) {
            const bool along_x = inner_x;
            const float curb = pad_size * .5F + 1.2F;   // tight to the kerb, clear of the traffic lane
            const Vector3 p = along_x ? Vector3{bx + (bx > 0 ? curb : -curb), ground, bz + along} : Vector3{bx + along, ground, bz + (bz > 0 ? -curb : curb)};
            world::GameplayMarker marker;
            marker.id = "vehicle_spawn_hatchback_downtown_" + std::to_string(++parked);
            marker.type = world::GameplayMarkerType::VehicleSpawn;
            marker.position = p; marker.size = {2, 1.6F, 4.4F}; marker.rotation_y = along_x ? 0 : 90;
            capital->gameplay_markers.push_back(marker);
        }
    }
    // Signals at the core's nine crossings (three of them on the highway), on opposite corners.
    for (std::size_t i = 1; i <= 3; ++i) for (std::size_t j = 1; j <= 3; ++j) {
        const float x = dt_street_x[i], z = dt_street_z[j];
        const float ox = (x == 0 ? world::highways::paved_half + 1.6F : dt_street * .5F + 1.2F), oz = dt_street * .5F + 1.2F;
        kit.put(urban + "Traffic lights/traffic_light.glb", at(x + ox, z - oz), 0, false);
        kit.put(urban + "Traffic lights/traffic_light.glb", at(x - ox, z + oz), 180, false);
    }
    for (const auto& [x, z, yaw] : std::initializer_list<std::tuple<float, float, float>>{{13, 50, 270}, {-13, -114, 90}, {114, -11, 0}})
        kit.put(urban + "Bus stops/busstop_two_seats.glb", at(x, z), yaw, false);
    boulevard(kit, ground);
    // Keep the rest of the kit off downtown.
    kit.claim({0, 0, 0}, grid_edge_x + 2, grid_edge_z + 2, 0);
}

// ------------------------------------------------------------------ Verda City's south blocks
// Three new blocks south of downtown on the same 80 m grid (the avenues carry on south to a new
// street at z = 240; the highway south leaves from x = 0, so the block beside it stays open):
// apartments (four blocks of flats round a courtyard), an office building on a plaza, and a bar
// block - a dark-brick corner bar and Main Street shops with rooms above, flats behind.
void downtown_south(Kit& kit, world::VerdaRegion& region) {
    world::Settlement* capital = nullptr;
    for (auto& settlement : region.runtime_settlements()) if (settlement.id == "capital_verda") capital = &settlement;
    if (!capital) return;
    const float ground = world::terrain::TerrainHeight::sample(0, 0);
    const auto at = [&](float x, float z) { return Vector3{x, ground + pad_top, z}; };
    // One row of blocks south of the grid on the same columns, and a street along its far side
    // that crosses the highway.
    constexpr float row = grid_edge_z + 34, south = grid_edge_z + 68 + dt_street * .5F, edge = grid_edge_z;
    std::vector<world::Road> roads;
    for (const float x : {-171.0F, -87.0F, 87.0F, 171.0F})
        roads.push_back({{x, 0, edge}, {x, 0, south + dt_street * .5F}, dt_street, world::RoadType::Asphalt});
    roads.push_back({{-grid_edge_x, 0, south}, {grid_edge_x, 0, south}, dt_street, world::RoadType::Asphalt});
    for (const auto& road : roads) capital->roads.push_back(road);
    kit.add_roads(roads);
    for (const float x : {-171.0F, -87.0F, 87.0F, 171.0F}) kit.put(city + "ground/intersection_16.glb", {x, ground, south}, 0, false);
    const auto home_marker = [&](Vector3 p, float yaw) { kit.purpose({p.x, ground + .3F, p.z}, yaw, world::BuildingPurpose::Home); };
    const auto dress = [&](float bx, float bz) {
        kit.put(city + "ground/sidewalk_block_68.glb", {bx, ground, bz}, 0, false);
        for (int side = 0; side < 4; ++side) {
            const float e = pad_size * .5F - 1.6F;
            const auto spot = [&](float along) {
                return side == 0 ? at(bx + along, bz - e) : side == 1 ? at(bx + e, bz + along) : side == 2 ? at(bx - along, bz + e) : at(bx - e, bz - along);
            };
            for (const float along : {-16.0F, 16.0F}) kit.tree(spot(along));
            kit.put(urban + "Traffic lights/street_light.glb", spot(0), side == 0 ? 0.0F : side == 1 ? 270.0F : side == 2 ? 180.0F : 90.0F, false);
        }
    };
    // Apartments: four blocks of flats round a courtyard, every one a home.
    {
        const float bx = dt_block_x[0], bz = row;
        dress(bx, bz);
        const char* models[]{"flat_square_brick.glb", "flat_square_concrete_alt.glb", "flat_square_concrete.glb", "flat_square_brick_alt.glb"};
        int n = 0;
        for (const float dx : {-15.0F, 15.0F}) for (const float dz : {-15.0F, 15.0F}) {
            const Vector3 p = at(bx + dx, bz + dz);
            if (!kit.claim(p, 8.6F, 8.6F, 0)) continue;
            kit.put(prebuild + models[n++ % 4], p, dx < 0 ? 90.0F : 270.0F, false);
            home_marker(at(bx + dx * .5F, bz + dz), dx < 0 ? 90.0F : 270.0F);
        }
        for (const float dz : {-5.0F, 5.0F}) kit.tree(at(bx, bz + dz));
        kit.put(urban + "Bus stops/busstop_bench.glb", at(bx, bz), 0, false);
        ++kit.report.south_blocks;
    }
    // The office building: an eight-storey concrete office on a plaza with trees and benches.
    {
        const float bx = dt_block_x[1], bz = row;
        dress(bx, bz);
        if (kit.claim(at(bx, bz - 8), 16, 12, 0)) kit.put(city + "towers/concrete_office.glb", at(bx, bz - 8), 0, false);
        for (const float dx : {-20.0F, -8.0F, 8.0F, 20.0F}) kit.tree(at(bx + dx, bz + 20));
        for (const float dx : {-14.0F, 14.0F}) kit.put(urban + "Bus stops/busstop_bench.glb", at(bx + dx, bz + 14), 180, false);
        ++kit.report.south_blocks;
    }
    // The bar block: a dark-brick corner bar and Main Street shops along the north side (facing
    // downtown), two blocks of flats behind.
    {
        const float bx = dt_block_x[3], bz = row;   // the Green Hand's corner bar is here
        dress(bx, bz);
        struct Front { const char* model; float mid, width; int storeys; };
        const Front fronts[]{{"main_street_dark_12m_2st", -24, 12, 2}, {"main_street_brown_18m_3st", -9, 18, 3},
                             {"main_street_red_12m_3st", 6, 12, 3}, {"main_street_tan_12m_2st", 18, 12, 2}};
        bool bar = true;
        for (const auto& f : fronts) {
            const Vector3 position = at(bx + f.mid, bz - 20);
            if (!kit.put(city + "main_street/" + f.model + ".glb", position, 0, false)) continue;
            constexpr float floor = 3.6F, slab = .2F, wall = .3F;
            kit.report.enterable.push_back({position, 0, f.width, 20, f.storeys, -4.5F,
                {f.width * .5F - wall - .65F, slab, -10 + 1.5F}, {0, 0, 1}, 5.04F + 2.2F, slab, floor + slab, {-3, 0, 0}});
            ++kit.report.buildings;
            if (bar) { // the corner bar: a Pub marker inside retags the Main Street building
                kit.purpose(to_world(position, 0, 0, slab + .1F, -5), 0, world::BuildingPurpose::Pub);
                ++kit.report.retagged;
                bar = false;
            }
        }
        const char* flats[]{"flat_Lshape_brick.glb", "flat_square_concrete.glb"};
        for (int i = 0; i < 2; ++i) {
            const float dx = i == 0 ? -15.0F : 15.0F;
            const Vector3 p = at(bx + dx, bz + 17);
            if (!kit.claim(p, 8.6F, 8.6F, 0)) continue;
            kit.put(prebuild + flats[i], p, 180, false);
            home_marker(at(bx + dx, bz + 10), 180);
        }
        ++kit.report.south_blocks;
    }
    // Parliament on the block beside the highway, its steps and lawn facing the boulevard (bounds
    // centre; the model's front, -Z, toward x = 0).
    {
        const Vector3 seat{dt_block_x[2], world::terrain::TerrainHeight::sample(dt_block_x[2], row), row};
        if (kit.claim(seat, 28, 30, 0) && kit.put("assets/verda/city/civic/parliament.glb", seat, 90, false)) {
            kit.purpose(to_world(seat, 90, 0, 1.85F, -8), 90, world::BuildingPurpose::Office);
            kit.report.parliament = true;
        }
    }
    // Keep the rest of the kit off the new blocks.
    for (const float bx : {dt_block_x[0], dt_block_x[1], dt_block_x[3]}) kit.claim({bx, 0, row}, 36, 36, 0);
}

// ------------------------------------------------------------------ Roka, the industrial city
// Verda's works town, rebuilt as a city on Porto Luma's plan: a 4 x 4 grid of 12 m streets
// (avenues and streets at +-40 and +-120 from the centre) round Placo de la Laboristoj. Main
// Street rows north and south of the square, worker flats on the west blocks with the town hall
// between them, and the east blocks are yards - sheds, hangars and factories on asphalt, with
// trucks. West of the grid stand the works: a long factory hall, a hangar, pipes and a fenced yard.
// The highways from the capital and the ring road end at the grid's east edge.
world::Settlement* settlement(world::VerdaRegion& region, std::string_view id);
void vehicle_marker(world::Settlement& town, const std::string& id, Vector3 at, float yaw);
void roka_city(Kit& kit, world::VerdaRegion& region) {
    auto* roka = settlement(region, "west_roka");
    auto* capital = settlement(region, "capital_verda");
    if (!roka || !capital) return;
    const auto c = site("west_roka");
    const float edge = pitch + street * .5F + 40; // 126
    const float ground = world::terrain::TerrainHeight::sample(c.x, c.z);
    roka->buildings.clear();
    roka->roads.clear();
    kit.clear_area(c, 215);
    for (auto& road : capital->roads)
        for (auto* end : {&road.start, &road.end})
            if (std::abs(end->x - c.x) < .5F && std::abs(end->z - c.z) < .5F) {kit.forget_road(road); *end = {c.x + edge, end->y, c.z - 40};}
    for (const float o : {-120.0F, -40.0F, 40.0F, 120.0F}) {
        roka->roads.push_back({{c.x + o, 0, c.z - edge}, {c.x + o, 0, c.z + edge}, street, world::RoadType::Asphalt});
        roka->roads.push_back({{c.x - edge, 0, c.z + o}, {c.x + edge, 0, c.z + o}, street, world::RoadType::Asphalt});
    }
    // The works road west from the grid (from the west street's centre line, so it joins it).
    roka->roads.push_back({{c.x - 120, 0, c.z}, {c.x - 196, 0, c.z}, 10, world::RoadType::Asphalt});
    kit.add_roads(roka->roads);
    kit.add_roads(capital->roads);
    for (const float i : {-120.0F, -40.0F, 40.0F, 120.0F}) for (const float j : {-120.0F, -40.0F, 40.0F, 120.0F})
        kit.put(city + "ground/intersection_12.glb", {c.x + i, ground, c.z + j}, 0, false);
    const auto at = [&](float x, float z) { return Vector3{c.x + x, ground + pad_top, c.z + z}; };
    int parked = 0, trucks = 0;
    const auto truck = [&](Vector3 p, float yaw) {
        vehicle_marker(*roka, "vehicle_spawn_truck_roka_" + std::to_string(++trucks), p, yaw);
    };
    const auto home_marker = [&](Vector3 p, float yaw) { kit.purpose({p.x, ground + .3F, p.z}, yaw, world::BuildingPurpose::Home); };
    for (const float bx : {-80.0F, 0.0F, 80.0F}) for (const float bz : {-80.0F, 0.0F, 80.0F}) {
        const auto seed = static_cast<std::uint32_t>((bx + 300) * 13 + (bz + 300) * 19 + 4000);
        const bool square = bx == 0 && bz == 0, yard = bx == 80, flats = bx == -80 && bz != 0, hall = bx == -80 && bz == 0;
        kit.put(city + (square ? "ground/courthouse_square_68.glb" : yard ? "ground/parking_lot_68.glb" : "ground/sidewalk_block_68.glb"),
                {c.x + bx, ground, c.z + bz}, 0, false);
        if (square) {
            // Placo de la Laboristoj: trees round the edge, benches, a pallet stack for the old works.
            for (int i = 0; i < 4; ++i) for (const float t : {-22.0F, 22.0F})
                kit.tree(at(i % 2 ? t : (i < 2 ? -27 : 27), i % 2 ? (i < 2 ? -27 : 27) : t));
            kit.put(urban + "Bus stops/busstop_clean_bench.glb", at(31, 12), 270, false);
            kit.put(urban + "Bus stops/busstop_clean_bench.glb", at(-31, -12), 90, false);
        } else if (yard) {
            // Yards: a hangar each side of the block (the middle one), trucks, pallets and barrels.
            // (No more of the shacks kit's Large Sheds anywhere: their materials have no colour, so
            // they stood about the island as blank white boxes.)
            const bool hangar = bz == 0;
            for (const float side : {-1.0F, 1.0F}) {
                const Vector3 p = at(bx + side * 15, bz - 6);
                if (hangar && kit.claim(p, 6, 3, 0)) kit.put(prebuild + "Hangar1.glb", p, side > 0 ? 270.0F : 90.0F, false);
            }
            if (hangar) modular_building(kit, at(bx, bz + 18), 180, 6, 3, "brick", 2, seed, world::BuildingPurpose::Industrial);
            truck(at(bx + (bz == 0 ? -22 : 22), bz + 24), bz == 0 ? 90.0F : 270.0F);
            for (int i = 0; i < 8; ++i) {
                const auto r = hash(seed + static_cast<std::uint32_t>(i));
                prop(kit, shacks + (i % 3 ? "Pallet.glb" : "Barrel.glb"), at(bx - 26 + static_cast<float>(r % 52), bz - 27 + static_cast<float>((r / 53) % 4)),
                     static_cast<float>(r % 360), .9F);
            }
            prop(kit, urban + "Litter/Dumpsters/dumpster_lid_closed_red.glb", at(bx + 28, bz + 6), 90, 2);
        } else if (flats) {
            // Worker flats: brick and concrete blocks round a courtyard, each a home.
            const char* models[]{"flat_square_brick.glb", "flat_square_concrete.glb", "flat_square_brick_alt.glb", "flat_square_concrete_alt.glb"};
            int n = 0;
            for (const float dx : {-15.0F, 15.0F}) for (const float dz : {-15.0F, 15.0F}) {
                const Vector3 p = at(bx + dx, bz + dz);
                if (!kit.claim(p, 8.6F, 8.6F, 0)) continue;
                kit.put(prebuild + models[(n + static_cast<int>(seed)) % 4], p, dx < 0 ? 90.0F : 270.0F, false);
                home_marker(at(bx + dx * .5F, bz + dz), dx < 0 ? 90.0F : 270.0F);
                ++n;
            }
            kit.tree(at(bx, bz - 4));
            kit.tree(at(bx, bz + 4));
        } else if (hall) {
            // The town hall (urbodomo) and the clinic facing the square.
            modular_building(kit, at(bx + 14, bz - 14), 270, 6, 4, "brick", 2, seed, world::BuildingPurpose::Office);
            modular_building(kit, at(bx + 14, bz + 16), 270, 4, 3, "stuco", 1, seed + 1, world::BuildingPurpose::Clinic);
            for (const float z : {-20.0F, 0.0F, 20.0F}) kit.tree(at(bx - 22, bz + z));
        } else {
            // Main Street north and south of the square: shops, a pub, rooms above.
            const int toward = bz > 0 ? 0 : 2;
            main_street_row(kit, ground, c.x + bx, c.z + bz, toward, seed);
            main_street_row(kit, ground, c.x + bx, c.z + bz, (toward + 2) % 4, seed + 7);
        }
        for (int side = 0; side < 4; ++side) {
            const float e = pad_size * .5F - 1.6F;
            const auto spot = [&](float along) {
                return side == 0 ? at(bx + along, bz - e) : side == 1 ? at(bx + e, bz + along) : side == 2 ? at(bx - along, bz + e) : at(bx - e, bz - along);
            };
            if (!square && !yard) for (const float along : {-16.0F, 16.0F}) kit.tree(spot(along));
            kit.put(urban + "Traffic lights/street_light.glb", spot(0), side == 0 ? 0.0F : side == 1 ? 270.0F : side == 2 ? 180.0F : 90.0F, false);
        }
        if (!square && bx == 0) for (const float along : {8.0F, -18.0F}) {
            const float curb = pad_size * .5F + 1.2F;   // tight to the kerb, clear of the traffic lane
            vehicle_marker(*roka, "vehicle_spawn_hatchback_roka_" + std::to_string(++parked), {c.x + along, 0, c.z + bz + (bz > 0 ? -curb : curb)}, 90);
        }
    }
    for (const float i : {-40.0F, 40.0F}) for (const float j : {-40.0F, 40.0F}) {
        const float o = street * .5F + 1.2F;
        kit.put(urban + "Traffic lights/traffic_light.glb", at(i + o, j - o), 0, false);
        kit.put(urban + "Traffic lights/traffic_light.glb", at(i - o, j + o), 180, false);
    }
    kit.claim(c, edge + 2, edge + 2, 0);
    // The works, west of the grid either side of the works road: a long factory hall, a
    // hangar, pipe runs and a fenced stock yard.
    modular_building(kit, {c.x - 165, 0, c.z - 45}, 0, 10, 5, "brick", 2, 4401, world::BuildingPurpose::Industrial);
    modular_building(kit, {c.x - 165, 0, c.z + 48}, 180, 8, 4, "stuco", 1, 4402, world::BuildingPurpose::Industrial);
    if (kit.claim({c.x - 140, 0, c.z + 20}, 6, 3, 0)) kit.put(prebuild + "Hangar1.glb", {c.x - 140, 0, c.z + 20}, 180);
    for (int i = 0; i < 10; ++i) kit.put(urban + "Pipes/pipe_rusted.glb", {c.x - 150 + i * 2.0F, 0, c.z - 15}, 90);
    for (int i = 0; i < 6; ++i) kit.put(urban + "Pipes/pipe.glb", {c.x - 190, 0, c.z - 30 + i * 2.0F}, 0);
    const std::vector<std::string> chain{shacks + "Chainlink Fence.glb"};
    fence(kit, {c.x - 205, 0, c.z - 110}, {c.x - 128, 0, c.z - 110}, chain, 2.46F, 41);
    fence(kit, {c.x - 205, 0, c.z - 110}, {c.x - 205, 0, c.z - 8}, chain, 2.46F, 42);
    fence(kit, {c.x - 205, 0, c.z + 8}, {c.x - 205, 0, c.z + 110}, chain, 2.46F, 43);
    fence(kit, {c.x - 205, 0, c.z + 110}, {c.x - 128, 0, c.z + 110}, chain, 2.46F, 44);
    for (int i = 0; i < 16; ++i) {
        const auto r = hash(4500U + static_cast<std::uint32_t>(i));
        const Vector3 p{c.x - 195 + static_cast<float>(r % 50), 0, c.z + (i % 2 ? 1.0F : -1.0F) * (20 + static_cast<float>((r / 51) % 60))};
        prop(kit, shacks + (i % 3 ? "Barrel.glb" : "Pallet.glb"), p, static_cast<float>(r % 360), .9F);
    }
    for (int i = 0; i < 6; ++i) prop(kit, shacks + "Concrete Barricade.glb", {c.x - 130, 0, c.z - 70 + i * 9.0F}, 0, 1.1F);
    truck({c.x - 160, 0, c.z - 8}, 90);
    for (const float x : {-150.0F, -175.0F, -200.0F}) kit.put(urban + "Traffic lights/street_light.glb", {c.x + x, 0, c.z - 6.4F}, 180, false);
    kit.claim({c.x - 165, 0, c.z}, 42, 112, 0);
    roka->survivors = 0; // every home in the city is lived in
    ++kit.report.cities;
}

// ------------------------------------------------------------------ Sovaĝejo's trailer park
// Single-wide mobile homes (Trailer Park pack) south-west of Roka, on the flat ground between the
// industrial yard and the hills: two rows of lots off a gravel lane, porches and steps toward the
// lane, cars on pads, a fire pit, a chainlink perimeter, and a gravel road into town. The trailers
// are enterable (their doors are knocked out in MeshCollision) and every one is a HOME.
const std::string trailers = "assets/verda/residential/trailers/";
// ------------------------------------------------------------------ Sovaĝejo
// The heart of Ferarbaro, Verda's backcountry: the trailer park plus homesteads strung along a
// dirt road south of Roka. Patched rather than ruined - workshops and lean-tos, a car that runs
// and the parts donors on blocks beside it, rain barrels, firewood, pallets and a fire pit.
constexpr std::string_view sovagejo_id = "ferarbaro_sovagejo";
// The dirt road from Roka's west avenue north to the park gate runs along this x.
float sovagejo_road_x() { return world::layout::sovagejo_center.x; }
world::Settlement* sovagejo(world::VerdaRegion& region) {
    for (auto& settlement : region.runtime_settlements()) if (settlement.id == sovagejo_id) return &settlement;
    world::Settlement town;
    town.id = std::string(sovagejo_id);
    town.name = "Sovaĝejo";
    town.center = world::layout::sovagejo_center;
    town.state = world::SettlementState::Peaceful;
    region.runtime_settlements().push_back(std::move(town));
    return &region.runtime_settlements().back();
}
void car_marker(world::Settlement& town, Vector3 at, float yaw, bool donor, int& count) {
    world::GameplayMarker car;
    car.id = std::string(donor ? "vehicle_spawn_hatchback_donor_" : "vehicle_spawn_hatchback_") + "sovagejo_" + std::to_string(++count);
    car.type = world::GameplayMarkerType::VehicleSpawn;
    car.position = {at.x, world::terrain::TerrainHeight::sample(at.x, at.z), at.z};
    car.size = {2, 1.6F, 4.4F};
    car.rotation_y = yaw;
    town.gameplay_markers.push_back(car);
}
// One homestead facing its road: `front` is the yard's edge on the road, `yaw` turns local +z
// (house -> road) into the world.
void homestead(Kit& kit, world::Settlement& town, Vector3 front, float yaw, std::uint32_t seed, int& cars) {
    const auto at = [&](float x, float z) { return to_world(front, yaw, x, 0, z); };
    const auto r = hash(seed);
    // House: a shack or a small timber home, door to the road.
    const auto house = at(0, -11);
    if (!kit.claim(house, 4, 4, yaw)) return;
    kit.put(shacks + "Shack A.glb", house, yaw + 180);
    // Workshop: a second shack for the ones who fix everything, and a lean-to carport.
    if (r % 3 == 0) {
        const auto shop = at(11, -13);
        if (kit.claim(shop, 2.2F, 2.2F, yaw)) kit.put(shacks + "Shack A.glb", shop, yaw + 90);
    }
    prop(kit, shacks + (r % 2 ? "Leanto A.glb" : "Leanto B.glb"), at(r % 3 == 0 ? -12 : 9, -10), yaw + 180, 2.2F);
    // The car that runs, and one or two parts donors on blocks.
    if (kit.claim(at(-8, -2), 1.4F, 2.6F, yaw)) car_marker(town, at(-8, -2), yaw + 90, false, cars);
    if (kit.claim(at(7, -3), 1.4F, 2.6F, yaw)) car_marker(town, at(7, -3), yaw + 70 + static_cast<float>(r % 40), true, cars);
    if (r % 2 == 0 && kit.claim(at(-13, -9), 1.4F, 2.6F, yaw)) car_marker(town, at(-13, -9), yaw + 15, true, cars);
    // Rain barrels at the house corner, firewood out back, pallets by the workshop, a fire pit.
    prop(kit, shacks + "Barrel.glb", at(4.6F, -7), static_cast<float>(r % 360), .7F);
    prop(kit, shacks + "Barrel.glb", at(5.6F, -7.6F), static_cast<float>(r % 90), .7F);
    for (int i = 0; i < 3; ++i) prop(kit, "assets/verda/survival/runtime/branch.glb", at(-4.0F + i * 1.1F, -17), yaw + 90, .6F);
    prop(kit, shacks + "Pallet.glb", at(8, -4), yaw + static_cast<float>(r % 30), .9F);
    if (r % 4 != 1) prop(kit, shacks + "Pallet.glb", at(10, -3), yaw + 80, .9F);
    if (r % 3 != 2) prop(kit, "assets/verda/survival/runtime/bonfire.glb", at(-5, -20), 0, 1.2F);
    if (r % 5 == 3) prop(kit, "assets/verda/survival/runtime/Shelter_tent.glb", at(-12, -20), yaw, 2.2F);
    // A mended wooden fence round the back and one side of the yard; the front stays open to the road.
    const std::vector<std::string> rails{urban + "Fences/low_wooden_fence/wooden_fence_closed.glb",
        urban + "Fences/low_wooden_fence/wooden_fence_closed.glb", urban + "Fences/low_wooden_fence/wooden_fence_broken.glb",
        urban + "Fences/low_wooden_fence/wooden_fence_open.glb"};
    fence(kit, at(-18, -30), at(18, -30), rails, 2.2F, seed * 7);
    fence(kit, at(r % 2 ? 18 : -18, -30), at(r % 2 ? 18 : -18, -6), rails, 2.2F, seed * 7 + 3);
    kit.tree(at(-14, -24));
    kit.tree(at(5, -25));
    if (r % 2) kit.tree(at(-16, 0));
    ++kit.report.homesteads;
}
void sovagejo_homesteads(Kit& kit, world::VerdaRegion& region) {
    auto* town = sovagejo(region);
    // Along the dirt road north out of Roka (trailer_park lays it): homesteads either side, each
    // facing the road - houses east of it face west, and so on.
    const float x = sovagejo_road_x();
    int cars = 0;
    homestead(kit, *town, {x + 6, 0, 175}, -90, 901, cars);
    homestead(kit, *town, {x + 6, 0, 235}, -90, 902, cars);
    homestead(kit, *town, {x - 6, 0, 205}, 90, 903, cars);
    homestead(kit, *town, {x - 6, 0, 265}, 90, 904, cars);
    homestead(kit, *town, {x + 6, 0, 295}, -90, 905, cars);
    homestead(kit, *town, {x - 6, 0, 162}, 90, 906, cars);
    town->survivors += 2 * kit.report.homesteads;
}

// ------------------------------------------------------------------ bus stops
// Every town gets a stop from the Transit kit beside a main road near its centre: the shelter
// facing the road, the stop sign at the kerb, kerb pads and a tall street lamp. The capital's
// stop is on a highway just outside downtown, where a 12 m bus can pull in. Verda also keeps the
// line's buses, parked on the shoulder across from its stop.
const std::string transit = "assets/verda/transit/";
void bus_stops(Kit& kit, world::VerdaRegion& region) {
    std::vector<Vector3> ends;
    for (const auto& town : region.settlements()) for (const auto& road : town.roads) {ends.push_back(road.start); ends.push_back(road.end);}
    const auto clear_of_junctions = [&](Vector3 p) {
        for (const auto& e : ends) if (std::hypot(p.x - e.x, p.z - e.z) < 24) return false;
        return true;
    };
    // Candidate kerb spots per town, best first.
    struct Spot { Vector3 at, along; float width, score; world::RoadType type; };
    struct Stop { std::size_t town; std::vector<Spot> spots; };
    std::vector<Stop> stops;
    for (std::size_t t = 0; t < region.runtime_settlements().size(); ++t) {
        const auto& town = region.runtime_settlements()[t];
        const bool capital = town.id == "capital_verda";
        // A hamlet's stop is out on the highway by its gate, not down its own lanes.
        const bool hamlet = town.id.starts_with("hamlet_");
        // The backcountry and the small highway hamlets have no stop (Ruldomaro keeps its own).
        if (town.id == sovagejo_id || (hamlet && town.id != "hamlet_ruldomaro")) continue;
        const float near = capital ? 235 : hamlet ? 45 : 12, far = capital ? 360 : 170, ideal = capital ? 255 : hamlet ? 80 : 35;
        Stop stop{t, {}};
        for (const auto& other : region.settlements()) for (const auto& road : other.roads) {
            if (hamlet && &other == &town) continue;
            // Any road a bus fits (the backcountry's dirt roads too), long enough to pull in.
            if (road.width < 5 || world::roads::turnaround_road(road.width)) continue;
            const float length = std::hypot(road.end.x - road.start.x, road.end.z - road.start.z);
            if (length < 50) continue;
            const Vector3 along{(road.end.x - road.start.x) / length, 0, (road.end.z - road.start.z) / length};
            for (float d = 0; d <= length; d += 5) {
                const Vector3 at{road.start.x + along.x * d, 0, road.start.z + along.z * d};
                const float r = std::hypot(at.x - town.center.x, at.z - town.center.z);
                if (r < near || r > far || !clear_of_junctions(at)) continue;
                // Nearer this town than any other (the stop takes its name).
                bool own = true;
                for (const auto& rival : region.settlements())
                    if (&rival != &town && std::hypot(at.x - rival.center.x, at.z - rival.center.z) < r) own = false;
                if (!own) continue;
                stop.spots.push_back({at, along, road.width, std::abs(r - ideal), road.type});
            }
        }
        std::sort(stop.spots.begin(), stop.spots.end(), [](const Spot& a, const Spot& b) {return a.score < b.score;});
        if (!stop.spots.empty()) stops.push_back(std::move(stop));
    }
    // The line runs from the capital, then clockwise from the north (as game::transit::BusLine
    // does), so each shelter goes on the kerb the bus pulls into: the side traffic keeps to.
    const auto clockwise = [](Vector3 p) {
        return std::hypot(p.x, p.z) < 600 ? -1.0F : std::fmod(std::atan2(p.x, -p.z) + 2 * PI, 2 * PI);
    };
    std::sort(stops.begin(), stops.end(), [&](const Stop& a, const Stop& b) {return clockwise(a.spots.front().at) < clockwise(b.spots.front().at);});
    const world::roads::RoadGraph graph(region);
    for (std::size_t i = 0; i < stops.size(); ++i) {
        const auto& stop = stops[i];
        const auto& next = stops[(i + 1) % stops.size()];
        const bool capital = region.runtime_settlements()[stop.town].id == "capital_verda";
        const auto& previous = stops[(i + stops.size() - 1) % stops.size()];
        bool placed = false;
        for (const auto& spot : stop.spots) {
            // Which way the bus arrives here, and which way it leaves for the next stop.
            const Vector3 back{-spot.along.x, 0, -spot.along.z};
            const auto align = [&](Vector3 a, Vector3 b) { return (b.x - a.x) * spot.along.x + (b.z - a.z) * spot.along.z >= 0 ? spot.along : back; };
            Vector3 travel = spot.along, leaving = spot.along;
            const auto in = graph.route(previous.spots.front().at, spot.at);
            for (std::size_t k = in.points.size(); k-- > 1;)
                if (std::hypot(in.points[k].x - in.points[k - 1].x, in.points[k].z - in.points[k - 1].z) > 2) {travel = align(in.points[k - 1], in.points[k]); break;}
            const auto out_leg = graph.route(spot.at, next.spots.front().at);
            for (std::size_t k = 1; k < out_leg.points.size(); ++k)
                if (std::hypot(out_leg.points[k].x - spot.at.x, out_leg.points[k].z - spot.at.z) > 2) {leaving = align(spot.at, out_leg.points[k]); break;}
            const bool reverses = travel.x * leaving.x + travel.z * leaving.z < 0;
            for (const float flip : {1.0F}) {
                // Traffic keeps to (-d.z, d.x) of its direction d (world::roads::route_point):
                // the shelter goes on that kerb or not at all (then the next spot is tried).
                const Vector3 side{-travel.z * flip, 0, travel.x * flip};
                const auto out = [&](float off, float ahead) {
                    return Vector3{spot.at.x + side.x * (spot.width * .5F + off) + travel.x * ahead, 0,
                                   spot.at.z + side.z * (spot.width * .5F + off) + travel.z * ahead};
                };
                const float facing = std::atan2(-side.x, -side.z) * RAD2DEG; // the shelter's front (+Z) to the road
                const auto shelter = out(3.3F, 0);
                if (!kit.claim(shelter, 2.4F, 1.3F, facing)) continue;
                const auto t = stop.town;
                if (reverses) {
                    // The line comes back the way it came: a paved turnaround past the stop - off
                    // to the side if there is room, else a turning circle where the road ends.
                    std::vector<world::roads::Turnaround> options;
                    for (const float beyond : world::roads::Turnaround::beyond) for (const float where : {1.0F, -1.0F}) {
                        const Vector3 junction{spot.at.x + travel.x * beyond, 0, spot.at.z + travel.z * beyond};
                        if (graph.nearest(junction).distance > 1 || !clear_of_junctions(junction)) continue;
                        options.push_back(world::roads::turnaround_at(junction, {side.x * where, 0, side.z * where}, spot.width * .5F + 8));
                    }
                    for (const auto& end : ends) {
                        // A dead end ahead on this stop's road.
                        const float ahead = (end.x - spot.at.x) * travel.x + (end.z - spot.at.z) * travel.z;
                        const float lateral = std::abs((end.x - spot.at.x) * travel.z - (end.z - spot.at.z) * travel.x);
                        if (ahead < 15 || ahead > 220 || lateral > 1) continue;
                        int meeting = 0;
                        for (const auto& other : ends) meeting += std::hypot(other.x - end.x, other.z - end.z) < 1;
                        if (meeting == 1) options.push_back(world::roads::turnaround_at(end, travel, 6));
                    }
                    bool built = false;
                    for (const auto& loop : options) {
                        const float r = world::roads::Turnaround::radius;
                        bool dry = true;
                        for (const auto& p : loop.ring) dry = dry && world::terrain::TerrainHeight::sample(p.x, p.z) > world::layout::sea_level + 1;
                        // Nothing already standing in the way (fences, props, trees).
                        bool open = true;
                        for (const auto& town : region.settlements()) for (const auto& asset : town.assets)
                            if (std::hypot(asset.position.x - loop.centre.x, asset.position.z - loop.centre.z) < r + 5 ||
                                std::hypot(asset.position.x - (loop.junction.x + loop.stem_end.x) * .5F, asset.position.z - (loop.junction.z + loop.stem_end.z) * .5F) < 5)
                                open = false;
                        if (!dry || !open || !kit.claim(loop.centre, r + 1, r + 1, 0)) continue;
                        auto& roads = region.runtime_settlements()[t].roads;
                        const float w = world::roads::Turnaround::width;
                        // Paved like the road it leaves (asphalt off a highway).
                        const auto paving = spot.type == world::RoadType::Highway ? world::RoadType::Asphalt : spot.type;
                        roads.push_back({loop.junction, loop.stem_end, w, paving});
                        for (std::size_t k = 0; k < loop.ring.size(); ++k)
                            roads.push_back({loop.ring[k], loop.ring[(k + 1) % loop.ring.size()], w, paving});
                        ++kit.report.turnarounds;
                        built = true;
                        break;
                    }
                    // No room to turn here: try another spot for this town's stop.
                    if (!built) continue;
                }
                kit.put(transit + (t % 2 ? "bus_stop_alt.glb" : "bus_stop.glb"), shelter, facing);
                for (const float a : {-1.0F, 1.0F}) kit.put(transit + "concrete.glb", out(1.2F, a * 2.1F), facing);
                kit.put(transit + (t % 3 == 0 ? "bus_stop_sign.glb" : t % 3 == 1 ? "bus_stop_sign_01.glb" : "bus_stop_sign_02.glb"),
                        out(.7F, 4.0F), facing + 90);
                const auto lamp = out(2.3F, -6.5F);
                if (kit.claim(lamp, .5F, .5F, 0)) kit.put(transit + (t % 2 ? "lamppost_01.glb" : "lamppost.glb"), lamp, facing);
                if (capital) {
                    // The depot: the line's buses wait on the far shoulder, facing along the road.
                    auto& verda = region.runtime_settlements()[t];
                    for (int bus = 0; bus < 3; ++bus) {
                        const Vector3 at{spot.at.x - side.x * (spot.width * .5F + 2.2F) + travel.x * (18 + bus * 16.0F), 0,
                                         spot.at.z - side.z * (spot.width * .5F + 2.2F) + travel.z * (18 + bus * 16.0F)};
                        world::GameplayMarker marker;
                        marker.id = "vehicle_spawn_bus_linio1_" + std::to_string(bus + 1);
                        marker.type = world::GameplayMarkerType::VehicleSpawn;
                        marker.position = {at.x, world::terrain::TerrainHeight::sample(at.x, at.z), at.z};
                        marker.size = {3, 3.4F, 12.3F};
                        marker.rotation_y = std::atan2(-travel.x, -travel.z) * RAD2DEG;
                        verda.gameplay_markers.push_back(marker);
                    }
                }
                ++kit.report.bus_stops;
                placed = true;
                break;
            }
            if (placed) break;
        }
    }
}

void trailer_park(Kit& kit, world::VerdaRegion& region) {
    world::Settlement* town = sovagejo(region);
    if (!town) return;
    const auto c = site("west_roka");
    const Vector3 park = world::layout::sovagejo_center;
    // Local frame: the lane runs south (+a) to the gate, where the dirt road carries on down to
    // Roka's west avenue; +b is west.
    const Vector3 u{0, 0, 1}, v{-1, 0, 0};
    const auto at = [&](float a, float b) { return Vector3{park.x + u.x * a + v.x * b, 0, park.z + u.z * a + v.z * b}; };
    const float lane_yaw = std::atan2(u.x, u.z) * RAD2DEG;
    constexpr float lane_half = 3, row = 13.5F, lot = 13;
    const Vector3 gate = at(52, 0), avenue{c.x - 40, 0, c.z - (pitch + street * .5F + 40)};
    town->roads.push_back({at(-50, 0), gate, lane_half * 2, world::RoadType::Dirt});
    town->roads.push_back({gate, {gate.x, 0, avenue.z}, 5, world::RoadType::Dirt});
    kit.add_roads(town->roads);
    int parked = 0, homes = 0;
    for (int side = 0; side < 2; ++side) for (int i = 0; i < 6; ++i) {
        const auto r = hash(700U + static_cast<std::uint32_t>(side * 6 + i));
        const float a = -32.5F + i * lot, b = side == 0 ? -row : row;
        // Steps descend along the model's +Z: each row faces the lane.
        const float yaw = lane_yaw + (side == 0 ? -90.0F : 90.0F) + static_cast<float>(static_cast<int>(r % 7) - 3);
        const int model = 1 + static_cast<int>((side * 7 + i * 5 + r) % 6);
        const auto centre = at(a, b);
        if (!kit.claim(centre, 3.4F, 7.8F, yaw)) continue;
        if (!kit.put(trailers + "trailer_0" + std::to_string(model) + ".glb", centre, yaw)) continue;
        // HOME marker just inside the door (model-local x = -0.75, z = 0.56; floor 0.85 m), facing out.
        auto inside = to_world(centre, yaw, .25F, 0, .56F);
        inside.y = world::terrain::TerrainHeight::sample(centre.x, centre.z) + .85F;
        kit.purpose(inside, 90 + yaw, world::BuildingPurpose::Home);
        ++homes;
        // The yard: a car on the pad beside most homes, junk out back, shade trees on alternate lots.
        if (r % 4 != 0)
            vehicle_marker(*town, "vehicle_spawn_hatchback_trailer_park_" + std::to_string(++parked),
                           to_world(centre, yaw, side == 0 ? 6.6F : -6.6F, 0, 4.0F), yaw + (r % 2 ? 180.0F : 0.0F));
        prop(kit, shacks + (r % 3 == 0 ? "Pallet.glb" : "Barrel.glb"), to_world(centre, yaw, 2.2F, 0, -9.0F), static_cast<float>(r % 360), .8F);
        if (r % 5 == 1) prop(kit, shacks + "Barrel.glb", to_world(centre, yaw, 3.4F, 0, -8.6F), static_cast<float>(r % 90), .8F);
        if (i % 2 == side) kit.tree(to_world(centre, yaw, -1.0F, 0, -10.0F));
    }
    // Common ground at the north end: a fire pit ringed by benches.
    prop(kit, "assets/verda/survival/runtime/bonfire.glb", at(-44, 0), 0, 1.2F);
    for (const auto& [a, b, yaw] : std::initializer_list<std::tuple<float, float, float>>{{-44, -4.2F, 0}, {-44, 4.2F, 180}, {-48.2F, 0, 90}})
        prop(kit, urban + "Bus stops/busstop_bench.glb", at(a, b), lane_yaw + yaw - 90, 1.6F);
    // Street lights along the lane, and the chainlink perimeter with the gate at the lane.
    for (const float a : {-26.0F, 0.0F, 26.0F}) kit.put(urban + "Traffic lights/street_light.glb", at(a, lane_half + 1.2F), lane_yaw + 90, false);
    const std::vector<std::string> chain{shacks + "Chainlink Fence.glb"};
    fence(kit, at(-52, -24), at(52, -24), chain, 2.46F, 71);
    fence(kit, at(-52, 24), at(52, 24), chain, 2.46F, 72);
    fence(kit, at(-52, -24), at(-52, 24), chain, 2.46F, 73);
    fence(kit, at(52, -24), at(52, -lane_half - 2), chain, 2.46F, 74);
    fence(kit, at(52, lane_half + 2), at(52, 24), chain, 2.46F, 75);
    // The park brings its own people: about three per home (a stated population caps island life).
    town->survivors += homes * 3;
    ++kit.report.parks;
}

// ------------------------------------------------------------------ Espera and Suda Haveno
// The two county towns are built to the same city plan as Porto Luma and Roka: a 4 x 4 grid of
// 12 m streets (avenues and streets at +-40 and +-120 from the centre) round a central square,
// Main Street rows facing the square and the streets behind, signals at the inner crossings,
// street lights, kerbside cars. Then each its own: Espera, the farm town of the north, has the
// farmers' co-op yard (a red barn, grain bins, hay) and a feed store; Suda Haveno, the harbour
// town of the south, has the fish market and a boat shed by a warehouse yard. Both have blocks of
// flats, a car park and a corner mid-rise. The highways from the capital end at the grid's
// highway-side edge street.
enum class TownBlock { Square, Rows, Flats, Parking, Yard, Midrise };
struct TownPlan {
    std::string_view id, tag;      // settlement, marker prefix
    std::uint32_t seed;
    bool farm;                     // Espera's co-op yard, or Suda Haveno's harbour yard
    std::array<TownBlock, 9> blocks; // bx -80, 0, 80 (outer) by bz -80, 0, 80 (inner)
};

void county_town(Kit& kit, world::VerdaRegion& region, const TownPlan& plan) {
    auto* town = settlement(region, plan.id);
    if (!town) return;
    const auto c = site(plan.id);
    const float edge = pitch + street * .5F + 40; // 126
    const float ground = world::terrain::TerrainHeight::sample(c.x, c.z);
    // The old village goes: its houses, lanes, trees and loose pieces inside the new streets.
    const float reach = edge + 30;
    const auto inside = [&](Vector3 p) { return std::abs(p.x - c.x) < reach && std::abs(p.z - c.z) < reach; };
    town->buildings.clear();
    town->roads.clear();
    for (auto& s : region.runtime_settlements())
        kit.report.cleared += static_cast<int>(std::erase_if(s.assets, [&](const world::WorldAsset& a) { return a.vehicle.definition.empty() && inside(a.position); }));
    kit.clear_area(c, reach);
    // Loot laid out for the old village moves to the new square.
    int moved = 0;
    for (auto& m : town->gameplay_markers)
        if (m.type == world::GameplayMarkerType::LootSpawn && inside(m.position)) {
            m.position = {c.x + (moved % 2 ? 24.0F : -24.0F), ground + pad_top + .3F, c.z + (moved / 2 % 2 ? 24.0F : -24.0F)};
            ++moved;
        }
    for (const float o : {-120.0F, -40.0F, 40.0F, 120.0F}) {
        town->roads.push_back({{c.x + o, 0, c.z - edge}, {c.x + o, 0, c.z + edge}, street, world::RoadType::Asphalt});
        town->roads.push_back({{c.x - edge, 0, c.z + o}, {c.x + edge, 0, c.z + o}, street, world::RoadType::Asphalt});
    }
    kit.add_roads(town->roads);
    for (const float i : {-120.0F, -40.0F, 40.0F, 120.0F}) for (const float j : {-120.0F, -40.0F, 40.0F, 120.0F})
        kit.put(city + "ground/intersection_12.glb", {c.x + i, ground, c.z + j}, 0, false);
    const auto at = [&](float x, float z) { return Vector3{c.x + x, ground + pad_top, c.z + z}; };
    const auto home_marker = [&](Vector3 p, float yaw) { kit.purpose({p.x, ground + .3F, p.z}, yaw, world::BuildingPurpose::Home); };
    int parked = 0, block = 0;
    for (const float bx : {-80.0F, 0.0F, 80.0F}) for (const float bz : {-80.0F, 0.0F, 80.0F}) {
        const TownBlock kind = plan.blocks[static_cast<std::size_t>(block++)];
        const auto seed = plan.seed + static_cast<std::uint32_t>((bx + 300) * 11 + (bz + 300) * 17);
        const bool square = kind == TownBlock::Square, open = kind == TownBlock::Parking || kind == TownBlock::Yard;
        // (Yards stand on the sidewalk block like Roka's: the car-park surface would lift a floor.)
        kit.put(city + (square ? "ground/courthouse_square_68.glb" : kind == TownBlock::Parking ? "ground/parking_lot_68.glb" : "ground/sidewalk_block_68.glb"),
                {c.x + bx, ground, c.z + bz}, 0, false);
        // The side toward the centre (a row there faces the square or the main street).
        const int toward = bx > 0 ? 3 : bx < 0 ? 1 : bz > 0 ? 0 : 2;
        switch (kind) {
        case TownBlock::Square:
            for (int i = 0; i < 4; ++i) for (const float t : {-22.0F, 22.0F})
                kit.tree(at(i % 2 ? t : (i < 2 ? -27 : 27), i % 2 ? (i < 2 ? -27 : 27) : t));
            kit.put(urban + "Bus stops/busstop_clean_bench.glb", at(31, 12), 270, false);
            kit.put(urban + "Bus stops/busstop_clean_bench.glb", at(-31, -12), 90, false);
            if (plan.farm) kit.put("assets/verda/ranch/windmill.glb", at(0, 0), 45, false); // the old village pump
            break;
        case TownBlock::Rows:
            main_street_row(kit, ground, c.x + bx, c.z + bz, toward, seed);
            main_street_row(kit, ground, c.x + bx, c.z + bz, (toward + 2) % 4, seed + 7);
            break;
        case TownBlock::Midrise:
            main_street_row(kit, ground, c.x + bx, c.z + bz, toward, seed);
            kit.put(city + "towers/" + (seed % 2 ? "brick_midrise" : "stone_midrise") + ".glb",
                    at(bx + (bx > 0 ? 14 : -14), bz + (bz > 0 ? 14 : -14)), 0, false);
            break;
        case TownBlock::Flats: {
            // Flats round a courtyard, each a home.
            const char* models[]{"flat_square_brick.glb", "flat_square_concrete.glb", "flat_square_brick_alt.glb", "flat_square_concrete_alt.glb"};
            int n = 0;
            for (const float dx : {-15.0F, 15.0F}) for (const float dz : {-15.0F, 15.0F}) {
                const Vector3 p = at(bx + dx, bz + dz);
                if (!kit.claim(p, 8.6F, 8.6F, 0)) continue;
                kit.put(prebuild + models[(n + static_cast<int>(seed)) % 4], p, dx < 0 ? 90.0F : 270.0F, false);
                home_marker(at(bx + dx * .5F, bz + dz), dx < 0 ? 90.0F : 270.0F);
                ++n;
            }
            kit.tree(at(bx, bz - 4));
            kit.tree(at(bx, bz + 4));
            break;
        }
        case TownBlock::Parking:
            for (const float dz : {-12.0F, 12.0F}) for (const float dx : {-20.0F, -6.0F, 8.0F})
                if (hash(seed + static_cast<std::uint32_t>(dx * 3 + dz * 7 + 100)) % 3 == 0)
                    vehicle_marker(*town, "vehicle_spawn_hatchback_" + std::string(plan.tag) + "_lot_" + std::to_string(++parked), {c.x + bx + dx, 0, c.z + bz + dz}, 0);
            break;
        case TownBlock::Yard: {
            const float back = bz > 0 ? 1.0F : -1.0F;   // the far side, away from the centre
            if (plan.farm) {
                // The farmers' co-op: a red barn and grain bins on the far side, the feed store on
                // the street, hay and pallets in the yard.
                const std::string ranch = "assets/verda/ranch/";
                if (kit.claim(at(bx - 8, bz + back * 14), 9, 14, 90)) kit.put(ranch + "barn.glb", at(bx - 8, bz + back * 14), 90, false);
                for (const float dx : {16.0F, 26.0F}) kit.put(ranch + "grain_bin.glb", at(bx + dx, bz + back * 20), 0, false);
                modular_building(kit, at(bx + 14, bz - back * 18), back > 0 ? 180.0F : 0.0F, 4, 3, "brick", 1, seed, world::BuildingPurpose::Shop, pad_top);
                for (int i = 0; i < 10; ++i) kit.put(ranch + "hay_bale.glb", at(bx - 26 + (i % 5) * 1.8F, bz - back * 4 + (i / 5) * 1.9F), 0, false);
                kit.put(ranch + "water_trough.glb", at(bx + 4, bz - back * 2), 0, false);
            } else {
                // The harbour yard: the fish market hall and a boat shed, pallets and barrels.
                modular_building(kit, at(bx - 12, bz + back * 14), back > 0 ? 180.0F : 0.0F, 6, 3, "stuco", 1, seed, world::BuildingPurpose::Dock, pad_top);
                for (int i = 0; i < 12; ++i) {
                    const auto r = hash(seed + 50U + static_cast<std::uint32_t>(i));
                    prop(kit, shacks + (i % 3 ? "Pallet.glb" : "Barrel.glb"),
                         at(bx - 26 + static_cast<float>(r % 52), bz - back * (8 + static_cast<float>((r / 53) % 10))), static_cast<float>(r % 360), .9F);
                }
            }
            prop(kit, urban + "Litter/Dumpsters/dumpster_lid_closed_red.glb", at(bx + 28, bz - back * 6), 90, 2);
            break;
        }
        }
        for (int side = 0; side < 4; ++side) {
            const float e = pad_size * .5F - 1.6F;
            const auto spot = [&](float along) {
                return side == 0 ? at(bx + along, bz - e) : side == 1 ? at(bx + e, bz + along) : side == 2 ? at(bx - along, bz + e) : at(bx - e, bz - along);
            };
            if (!square && !open) for (const float along : {-16.0F, 16.0F}) kit.tree(spot(along));
            kit.put(urban + "Traffic lights/street_light.glb", spot(0), side == 0 ? 0.0F : side == 1 ? 270.0F : side == 2 ? 180.0F : 90.0F, false);
        }
        // Kerbside cars on the streets round the square, tight to the kerb (clear of the bus).
        if (!square && (bx == 0 || bz == 0)) for (const float along : {8.0F, -18.0F}) {
            const bool along_z = bz == 0;
            const float curb = pad_size * .5F + 1.2F;
            const Vector3 p = along_z ? Vector3{c.x + bx + (bx > 0 ? -curb : curb), 0, c.z + along} : Vector3{c.x + along, 0, c.z + bz + (bz > 0 ? -curb : curb)};
            vehicle_marker(*town, "vehicle_spawn_hatchback_" + std::string(plan.tag) + "_" + std::to_string(++parked), p, along_z ? 0.0F : 90.0F);
        }
    }
    for (const float i : {-40.0F, 40.0F}) for (const float j : {-40.0F, 40.0F}) {
        const float o = street * .5F + 1.2F;
        kit.put(urban + "Traffic lights/traffic_light.glb", at(i + o, j - o), 0, false);
        kit.put(urban + "Traffic lights/traffic_light.glb", at(i - o, j + o), 180, false);
    }
    kit.claim(c, edge + 2, edge + 2, 0);
    town->survivors = 0; // every home in the town is lived in
    ++kit.report.cities;
}

// Espera faces south down the highway to the capital (which ends at its south edge street);
// Suda Haveno faces north up it.
void espera(Kit& kit, world::VerdaRegion& region) {
    using B = TownBlock;
    county_town(kit, region, {"village_espera", "espera", 6100, true,
        {B::Flats, B::Rows, B::Yard,        // bx -80: north, middle, south
         B::Rows, B::Square, B::Rows,       // bx 0
         B::Midrise, B::Rows, B::Parking}}); // bx 80
}
void suda_haveno(Kit& kit, world::VerdaRegion& region) {
    using B = TownBlock;
    county_town(kit, region, {"south_haven", "suda", 6200, false,
        {B::Parking, B::Rows, B::Flats,
         B::Rows, B::Square, B::Rows,
         B::Midrise, B::Rows, B::Yard}});
}

// Island life drives: most houses in the small towns get the family car on the drive beside them.
void driveways(Kit& kit, world::VerdaRegion& region) {
    for (auto& town : region.runtime_settlements()) {
        if (town.id.find("capital") != std::string::npos) continue; // downtown parks at the kerb
        int parked = 0;
        std::vector<world::GameplayMarker> cars;
        for (const auto& house : town.buildings) {
            if (house.style != world::BuildingStyle::RuralHouse && house.style != world::BuildingStyle::TwoStoryHouse) continue;
            const auto r = hash(900U + static_cast<std::uint32_t>(std::hash<std::string>{}(house.id) & 0xFFFF));
            if (r % 4 == 0) continue;
            const Lot own = Kit::building_lot(house);
            const float terrain = world::terrain::TerrainHeight::sample(house.position.x, house.position.z);
            for (const float side : {r % 2 ? 1.0F : -1.0F, r % 2 ? -1.0F : 1.0F}) {
                // Beside the house, level with its front, nose to the road.
                Vector3 spot = to_world({house.position.x, terrain, house.position.z}, house.rotation_y, side * (house.size.x * .5F + 2.3F), 0, -house.size.z * .5F + 2.6F);
                if (!kit.claim(spot, 1.3F, 2.6F, house.rotation_y, &own)) continue;
                world::GameplayMarker car;
                car.id = "vehicle_spawn_hatchback_" + town.id + "_drive_" + std::to_string(++parked);
                car.type = world::GameplayMarkerType::VehicleSpawn;
                spot.y = world::terrain::TerrainHeight::sample(spot.x, spot.z);
                car.position = spot;
                car.size = {2, 1.6F, 4.4F};
                car.rotation_y = house.rotation_y + 180;
                cars.push_back(car);
                ++kit.report.driveways;
                break;
            }
        }
        town.gameplay_markers.insert(town.gameplay_markers.end(), cars.begin(), cars.end());
    }
}
// ------------------------------------------------------------------ La Verda Basballigo's ballparks
// One in each county on its level pad (world::ballpark::parks): the front - the gate, the car park
// and the bus bays - toward the nearest road, a road in to the gate, and the club's bus in its bay.
void ballparks(Kit& kit, world::VerdaRegion& region) {
    namespace B = world::ballpark;
    for (const auto& park : B::parks) {
        auto* town = settlement(region, park.site);
        if (!town) continue;
        Vector3 road_point{};
        float best = 1e9F;
        for (const auto& t : region.settlements())
            for (const auto& road : t.roads) {
                if (road.type == world::RoadType::Highway) continue;
                const Vector3 ab = Vector3Subtract(road.end, road.start);
                const float length = ab.x * ab.x + ab.z * ab.z;
                const float u = length > 0 ? std::clamp(((park.centre.x - road.start.x) * ab.x + (park.centre.z - road.start.z) * ab.z) / length, 0.0F, 1.0F) : 0;
                const Vector3 q{road.start.x + ab.x * u, 0, road.start.z + ab.z * u};
                const float d = std::hypot(q.x - park.centre.x, q.z - park.centre.z) - road.width * .5F;
                if (d < best) { best = d; road_point = q; }
            }
        if (best > 400) continue;
        // The model's front (-Z) toward the road.
        const float dx = road_point.x - park.centre.x, dz = road_point.z - park.centre.z;
        const float yaw = std::atan2(-dx, -dz) * RAD2DEG;
        const Vector3 origin{park.centre.x, world::terrain::TerrainHeight::sample(park.centre.x, park.centre.z), park.centre.z};
        // Its whole footprint (the model's bounds, off-centre toward the car park) clear of roads and lots.
        const Vector3 middle = to_world(origin, yaw, (B::model_min.x + B::model_max.x) * .5F, 0, (B::model_min.z + B::model_max.z) * .5F);
        // (Verda City's is built on the stadium ground kept for it.)
        const Lot reserve{stadium_reserve.x - stadium_half, stadium_reserve.z - stadium_half, stadium_reserve.x + stadium_half, stadium_reserve.z + stadium_half};
        const bool on_reserve = std::hypot(park.centre.x - stadium_reserve.x, park.centre.z - stadium_reserve.z) < 1;
        if (!kit.claim(middle, (B::model_max.x - B::model_min.x) * .5F, (B::model_max.z - B::model_min.z) * .5F, yaw, on_reserve ? &reserve : nullptr)) continue;
        if (!kit.put(B::model(park.team), origin, yaw, false)) continue;
        kit.report.ballparks.push_back(origin);
        // The road in: from the gate out to the road.
        const Vector3 gate = to_world(origin, yaw, 0, 0, -76);
        const world::Road access{{gate.x, 0, gate.z}, {road_point.x, 0, road_point.z}, 8, world::RoadType::Asphalt};
        if (Vector3Distance(access.start, access.end) > 2) { town->roads.push_back(access); kit.add_roads({access}); }
        // The club's bus, in its bay (the bus line leaves it be: "_team_").
        world::GameplayMarker bus;
        bus.id = "vehicle_spawn_bus_team_" + std::to_string(park.team);
        bus.type = world::GameplayMarkerType::VehicleSpawn;
        bus.position = to_world(origin, yaw, B::home_bus.x, 0, B::home_bus.z);
        bus.size = {2.6F, 3.2F, 11};
        bus.rotation_y = yaw;
        town->gameplay_markers.push_back(bus);
    }
}

world::Settlement* settlement(world::VerdaRegion& region, std::string_view id) {
    for (auto& town : region.runtime_settlements()) if (town.id == id) return &town;
    return nullptr;
}

void vehicle_marker(world::Settlement& town, const std::string& id, Vector3 at, float yaw) {
    world::GameplayMarker car;
    car.id = id;
    car.type = world::GameplayMarkerType::VehicleSpawn;
    car.position = {at.x, world::terrain::TerrainHeight::sample(at.x, at.z), at.z};
    car.size = {2, 1.6F, 4.4F};
    car.rotation_y = yaw;
    town.gameplay_markers.push_back(car);
}

// ------------------------------------------------------------------ West Verda (Okcidenta Verda)
// Classic suburban blocks west of downtown on level ground: the downtown streets carry on west as
// tree-lined residential streets, a boulevard on the old Roka road, and rows of detached homes -
// ranches and two-storey houses on 24 m lots with front yards, picket fences, a car on the drive -
// backing onto each other across the blocks. A park, a strip of shops, a church and a clinic sit
// on the boulevard. Houses are procedural buildings (cheap to draw, enterable) owned by the capital.
constexpr float suburb_east_x = -grid_edge_x, suburb_mid_x = -330, suburb_west_x = -476;
constexpr float truck_road = 14;   // the main trucking routes' width (rigs pulling 13 m trailers)

// One detached house facing a street (`facing`: +1 the street is at larger z, -1 smaller z).
bool suburban_house(Kit& kit, world::Settlement& capital, Vector3 at, int facing, std::uint32_t seed, int& cars) {
    const auto r = hash(seed);
    const bool two = r % 5 < 2;
    const Vector3 size = two ? Vector3{11, 7, 9} : Vector3{13, 4, 10};
    const float yaw = facing > 0 ? 180 : 0; // procedural houses front on local -z
    if (!kit.claim(at, size.x * .5F + 1, size.z * .5F + 1, yaw)) return false;
    static constexpr Color walls[]{{236, 232, 220, 255}, {226, 214, 170, 255}, {176, 198, 214, 255}, {184, 196, 160, 255},
        {214, 196, 172, 255}, {196, 198, 196, 255}, {232, 206, 196, 255}, {150, 168, 182, 255}};
    static constexpr Color roofs[]{{72, 74, 78, 255}, {104, 78, 60, 255}, {128, 66, 52, 255}, {86, 96, 104, 255}, {58, 62, 58, 255}};
    world::Building house;
    house.id = "verda_okcidento_house_" + std::to_string(capital.buildings.size() + 1);
    house.style = two ? world::BuildingStyle::TwoStoryHouse : world::BuildingStyle::RuralHouse;
    house.position = {at.x, 0, at.z};
    house.size = size;
    house.rotation_y = yaw;
    house.wall_color = walls[(r / 5) % 8];
    house.roof_color = roofs[(r / 40) % 5];
    house.enterable = true;
    house.purpose = world::BuildingPurpose::Home;
    capital.buildings.push_back(house);
    ++kit.report.houses;
    const Vector3 ground{at.x, 0, at.z};
    const float side = r % 2 ? 1.0F : -1.0F;
    // The family car on the drive beside the house, nose to the street.
    if ((r / 200) % 4 == 0) {
        vehicle_marker(capital, "vehicle_spawn_hatchback_okcidento_" + std::to_string(++cars),
                       to_world(ground, yaw, side * (size.x * .5F + 2.4F), 0, -size.z * .5F + 2.2F), yaw + 180);
        ++kit.report.driveways;
    }
    // Front yard: a tree, sometimes a white picket fence with a gate on the path, a bin by the drive.
    if ((r / 1000) % 3 != 0) kit.tree(to_world(ground, yaw, -side * (size.x * .5F - 1.5F), 0, -size.z * .5F - 4.2F));
    if ((r / 3000) % 3 == 0) {
        const float z = -size.z * .5F - 3.6F, half = 10.5F; // the lawn edge, 1.4 m short of the kerb
        fence(kit, to_world(ground, yaw, -half, 0, z), to_world(ground, yaw, -1.2F, 0, z),
              {urban + "Walls & Fences/white_picket_fence/white_picket_fence_closed_left.glb"}, 2.38F, r);
        fence(kit, to_world(ground, yaw, 1.2F, 0, z), to_world(ground, yaw, half, 0, z),
              {urban + "Walls & Fences/white_picket_fence/white_picket_fence_closed_right.glb"}, 2.38F, r + 1);
    }
    kit.put(urban + "Litter/TrashCan/trash_can_alt1.glb", to_world(ground, yaw, side * (size.x * .5F + .8F), 0, -size.z * .5F - 3.4F), yaw);
    return true;
}

void west_suburbs(Kit& kit, world::VerdaRegion& region) {
    auto* capital = settlement(region, "capital_verda");
    if (!capital) return;
    // The Roka highway now leaves from the boulevard's west end instead of downtown's edge.
    for (auto& road : capital->roads)
        for (auto* end : {&road.start, &road.end})
            if (std::abs(end->x + grid_edge_x) < .5F && std::abs(end->z) < .5F) {
                const auto& other = end == &road.start ? road.end : road.start;
                if (other.x < suburb_west_x) {kit.forget_road(road); *end = {suburb_west_x, end->y, 0};}
            }
    // Streets: the boulevard on z = 0, the downtown streets carried west, two cross streets.
    capital->roads.push_back({{suburb_east_x, 0, 0}, {suburb_west_x, 0, 0}, 10, world::RoadType::Asphalt});
    // z = -84 is the truckers' road (Verda Transporto's yard is at its west end, and the Roka
    // highway leaves from the boulevard's west end): 14 m, and so is the link up to the boulevard.
    for (const float z : {-168.0F, -84.0F, 84.0F, 168.0F})
        capital->roads.push_back({{suburb_east_x, 0, z}, {suburb_west_x, 0, z}, z == -84 ? truck_road : 8.0F, world::RoadType::Asphalt});
    capital->roads.push_back({{suburb_mid_x, 0, -168}, {suburb_mid_x, 0, 168}, 8, world::RoadType::Asphalt});
    capital->roads.push_back({{suburb_west_x, 0, -168}, {suburb_west_x, 0, -84}, 8, world::RoadType::Asphalt});
    capital->roads.push_back({{suburb_west_x, 0, -84}, {suburb_west_x, 0, 0}, truck_road, world::RoadType::Asphalt});
    capital->roads.push_back({{suburb_west_x, 0, 0}, {suburb_west_x, 0, 168}, 8, world::RoadType::Asphalt});
    kit.add_roads(capital->roads);
    // Homes on both sides of every residential street, 24 m lots, set back 14 m from the centreline.
    int cars = 0;
    std::uint32_t seed = 7000;
    for (const float street_z : {-168.0F, -84.0F, 84.0F, 168.0F})
        for (const int facing : {1, -1}) {
            const float z = street_z - facing * (street_z == -84 ? 17.0F : 14.0F);   // (set back further on the truckers' road)
            for (const auto& [from, count, step] : std::initializer_list<std::tuple<float, int, float>>{{-200, 6, 22}, {-348, 5, 24}})
                for (int i = 0; i < count; ++i) suburban_house(kit, *capital, {from - static_cast<float>(i) * step, 0, z}, facing, ++seed, cars);
        }
    // Street lights in front yards, alternating sides, well clear of the driveways (8.9 m west of
    // each home's centre).
    for (const float street_z : {-168.0F, -84.0F, 0.0F, 84.0F, 168.0F})
        for (int i = 0; i < 6; ++i) {
            constexpr float lights[]{-196, -240, -284, -344, -392, -440};
            const float x = lights[i], side = i % 2 ? 1.0F : -1.0F;
            kit.put(urban + "Traffic lights/street_light.glb", {x, 0, street_z + side * (street_z == 0 ? 6.4F : street_z == -84 ? truck_road * .5F + 1.4F : 5.4F)}, side > 0 ? 0.0F : 180.0F);
        }
    // On the boulevard: the park (north-east), shops (south-east), a church and a clinic (west).
    for (int i = 0; i < 5; ++i) for (int j = 0; j < 3; ++j) {
        const auto r = hash(7500U + static_cast<std::uint32_t>(i * 3 + j));
        kit.tree({-196 - i * 27.0F + static_cast<float>(r % 7), 0, -18 - j * 14.0F - static_cast<float>((r / 7) % 5)});
    }
    for (int i = 0; i < 4; ++i)
        kit.put(urban + "Bus stops/busstop_bench.glb", {-210 - i * 32.0F, 0, -9}, 180);
    modular_building(kit, {-212, 0, 20}, 0, 8, 4, "stuco", 1, 7601, world::BuildingPurpose::Shop);
    modular_building(kit, {-270, 0, 20}, 0, 8, 4, "brick", 1, 7602, world::BuildingPurpose::Shop);
    modular_building(kit, {-376, 0, -20}, 180, 4, 4, "fancy_brick", 1, 7603, world::BuildingPurpose::Church);
    modular_building(kit, {-420, 0, 20}, 0, 8, 4, "stuco", 1, 7604, world::BuildingPurpose::Clinic);
    for (int i = 0; i < 3; ++i)
        vehicle_marker(*capital, "vehicle_spawn_hatchback_okcidento_shops_" + std::to_string(i + 1), {-226 - i * 22.0F, 0, 9.3F}, 270);
    capital->survivors += kit.report.houses; // west homes (built first)
}

// ------------------------------------------------------------------ Porto Luma
// The eastern trading port is a city now: a 3 x 3 grid of 80 m blocks round Placo Luma (the
// civic square), Main Street rows, two harbour towers, a car park, and the waterfront district on
// the east side - warehouses, the harbour office and a quay of bollards facing the Sea of Light.
void porto_luma_city(Kit& kit, world::VerdaRegion& region) {
    auto* port = settlement(region, "port_luma");
    auto* capital = settlement(region, "capital_verda");
    if (!port || !capital) return;
    const auto c = site("port_luma");
    const float edge = pitch + street * .5F + 40; // 126: avenues at +-40 and +-120
    const float ground = world::terrain::TerrainHeight::sample(c.x, c.z);
    port->buildings.clear();
    port->roads.clear();
    kit.clear_area(c, 140);
    // Highways that ran to the old crossing stop at the grid's west edge, on the street north of
    // the square (Placo Luma fills the centre).
    for (auto& road : capital->roads)
        for (auto* end : {&road.start, &road.end})
            if (std::abs(end->x - c.x) < .5F && std::abs(end->z - c.z) < .5F) {kit.forget_road(road); *end = {c.x - edge, end->y, c.z - 40};}
    for (const float o : {-120.0F, -40.0F, 40.0F, 120.0F}) {
        port->roads.push_back({{c.x + o, 0, c.z - edge}, {c.x + o, 0, c.z + edge}, street, world::RoadType::Asphalt});
        port->roads.push_back({{c.x - edge, 0, c.z + o}, {c.x + edge, 0, c.z + o}, street, world::RoadType::Asphalt});
    }
    // The quay road along the waterfront.
    port->roads.push_back({{c.x + edge, 0, c.z - 40}, {c.x + 196, 0, c.z - 40}, 9, world::RoadType::Asphalt});
    port->roads.push_back({{c.x + edge, 0, c.z + 40}, {c.x + 196, 0, c.z + 40}, truck_road, world::RoadType::Asphalt});   // (on down the fishers' ramp)
    port->roads.push_back({{c.x + 196, 0, c.z - 110}, {c.x + 196, 0, c.z + 110}, truck_road, world::RoadType::Asphalt}); // (between the quay's ramps)
    kit.add_roads(port->roads);
    kit.add_roads(capital->roads);
    for (const float i : {-120.0F, -40.0F, 40.0F, 120.0F}) for (const float j : {-120.0F, -40.0F, 40.0F, 120.0F})
        kit.put(city + "ground/intersection_12.glb", {c.x + i, ground, c.z + j}, 0, false);
    const auto at = [&](float x, float z) { return Vector3{c.x + x, ground + pad_top, c.z + z}; };
    int parked = 0;
    for (const float bx : {-80.0F, 0.0F, 80.0F}) for (const float bz : {-80.0F, 0.0F, 80.0F}) {
        const auto seed = static_cast<std::uint32_t>((bx + 300) * 11 + (bz + 300) * 17 + 9000);
        const bool square = bx == 0 && bz == 0, parking = bx == 80 && bz == 80, tower = bx == 80 && bz == -80;
        kit.put(city + (square ? "ground/courthouse_square_68.glb" : parking ? "ground/parking_lot_68.glb" : "ground/sidewalk_block_68.glb"),
                {c.x + bx, ground, c.z + bz}, 0, false);
        // Rows face the square first (the side toward the centre), then the street behind.
        const int toward = bx > 0 ? 3 : bx < 0 ? 1 : bz > 0 ? 0 : 2;
        if (square) {
            for (int i = 0; i < 4; ++i) for (const float t : {-22.0F, 22.0F})
                kit.tree(at(i % 2 ? t : (i < 2 ? -27 : 27), i % 2 ? (i < 2 ? -27 : 27) : t));
            kit.put(urban + "Bus stops/busstop_clean_bench.glb", at(31, 12), 270, false);
            kit.put(urban + "Bus stops/busstop_clean_bench.glb", at(-31, -12), 90, false);
        } else if (tower) {
            kit.put(city + "towers/glass_blue_tower.glb", at(bx - 14, bz + 14), 270, false);
            kit.put(city + "towers/concrete_office.glb", at(bx + 14, bz - 14), 0, false);
            for (const auto& [dx, dz] : std::initializer_list<std::pair<float, float>>{{-6, -6}, {6, -6}, {-6, 6}, {6, 6}})
                kit.tree(at(bx + 14 + dx, bz + 14 + dz));
        } else if (!parking) {
            main_street_row(kit, ground, c.x + bx, c.z + bz, toward, seed);
            if (std::abs(bx) + std::abs(bz) == 160) // corner blocks: a mid-rise behind the row
                kit.put(city + "towers/" + (seed % 2 ? "brick_midrise" : "stone_midrise") + ".glb",
                        at(bx + (bx > 0 ? 14 : -14), bz + (bz > 0 ? 14 : -14)), 0, false);
            else
                main_street_row(kit, ground, c.x + bx, c.z + bz, (toward + 2) % 4, seed + 7);
        }
        for (int side = 0; side < 4; ++side) {
            const float e = pad_size * .5F - 1.6F;
            const auto spot = [&](float along) {
                return side == 0 ? at(bx + along, bz - e) : side == 1 ? at(bx + e, bz + along) : side == 2 ? at(bx - along, bz + e) : at(bx - e, bz - along);
            };
            if (!square) for (const float along : {-16.0F, 16.0F}) kit.tree(spot(along));
            kit.put(urban + "Traffic lights/street_light.glb", spot(0), side == 0 ? 0.0F : side == 1 ? 270.0F : side == 2 ? 180.0F : 90.0F, false);
        }
        // Kerbside cars on the streets round the square.
        if (!square && (bx == 0 || bz == 0)) for (const float along : {8.0F, -18.0F}) {
            const bool along_z = bz == 0;
            const float curb = pad_size * .5F + 1.2F;   // tight to the kerb, clear of the traffic lane
            const Vector3 p = along_z ? Vector3{c.x + bx + (bx > 0 ? -curb : curb), 0, c.z + along} : Vector3{c.x + along, 0, c.z + bz + (bz > 0 ? -curb : curb)};
            vehicle_marker(*port, "vehicle_spawn_hatchback_porto_" + std::to_string(++parked), p, along_z ? 0.0F : 90.0F);
        }
    }
    for (const float i : {-40.0F, 40.0F}) for (const float j : {-40.0F, 40.0F}) {
        const float o = street * .5F + 1.2F;
        kit.put(urban + "Traffic lights/traffic_light.glb", at(i + o, j - o), 0, false);
        kit.put(urban + "Traffic lights/traffic_light.glb", at(i - o, j + o), 180, false);
    }
    kit.claim(c, edge + 2, edge + 2, 0);
    // Waterfront: the harbour office, the quay edge.
    modular_building(kit, {c.x + 158, 0, c.z - 118}, 0, 8, 4, "stuco", 2, 9101, world::BuildingPurpose::Office); // harbour office
    modular_building(kit, {c.x + 158, 0, c.z + 118}, 180, 4, 4, "brick", 1, 9102, world::BuildingPurpose::Dock);
    for (int i = 0; i < 28; ++i) kit.put(urban + "Cones & Barriers/Bollards/square_bollard.glb", {c.x + 206, 0, c.z - 108 + i * 8.0F}, 0);
    for (int i = 0; i < 18; ++i) {
        const auto r = hash(9200U + static_cast<std::uint32_t>(i));
        const Vector3 p{c.x + 140 + static_cast<float>(r % 40), 0, c.z + (i % 2 ? 50.0F : -50.0F) + static_cast<float>((r / 41) % 14) - 7};
        prop(kit, shacks + (i % 3 ? "Pallet.glb" : "Barrel.glb"), p, static_cast<float>(r % 360), 1);
    }
    for (int i = -4; i <= 4; ++i) kit.put(urban + "Traffic lights/street_light_clean.glb", {c.x + 204.5F, 0, c.z + i * 25.0F}, 90);
    port->survivors = std::max(port->survivors, 60);
}
// ------------------------------------------------------------------ East Verda (Orienta Verda)
// The fancy side of town, east of downtown: the same street plan as the west, but bigger lots
// (30 m), deeper lawns and the residential house model (two storeys, front porch, garage) in four
// paint schemes, wrought-iron front fences, two trees a lot, a car on the drive, a tree-lined
// boulevard on the Porto Luma road with a park and a brick clubhouse.
constexpr float east_near_x = grid_edge_x, east_mid_x = 330, east_far_x = 476;
const std::string houses = "assets/verda/residential/houses/";

bool fancy_house(Kit& kit, world::Settlement& capital, Vector3 at, int facing, std::uint32_t seed, int& cars) {
    static constexpr const char* schemes[]{"white", "blue", "sage", "cream"};
    constexpr float half_w = 7.21F, half_d = 10.09F;
    const auto r = hash(seed);
    const float yaw = facing > 0 ? 180 : 0; // the model's porch faces local -z
    if (!kit.claim(at, half_w + 1, half_d + 1, yaw)) return false;
    if (!kit.put(houses + "house_" + schemes[r % 4] + ".glb", at, yaw)) return false;
    ++kit.report.houses;
    const Vector3 ground{at.x, world::terrain::TerrainHeight::sample(at.x, at.z), at.z};
    // Enterable (tools/extract_house.py): the front door beside the garage is an open doorway,
    // the front wall is at local z = -5.85, and the floor top is 6 cm up. Seventeen steps climb +z
    // beside the garage from z = -2 to the upstairs floor (3.3 m) at z = 2.76; the hall runs to x = -0.6.
    constexpr float door_x = -2.41F, front = 5.85F, floor = .06F, upper = 3.3F;
    kit.report.enterable.push_back({ground, yaw, 13.6F, front * 2, 2, door_x, {-2.85F, 0, -2.6F}, {0, 0, 1}, 6, floor, upper, {1.5F, 0, 0}});
    kit.purpose(to_world(ground, yaw, door_x, floor, -front + 1.2F), yaw, world::BuildingPurpose::Home);
    ++kit.report.buildings;
    // The garage is on the model's -x side: the car waits on the drive in front of it.
    if ((r / 4) % 3 == 0) {
        vehicle_marker(capital, "vehicle_spawn_hatchback_orienta_" + std::to_string(++cars),
                       to_world(ground, yaw, -4.4F, 0, -half_d - 1.6F), yaw + 180);
        ++kit.report.driveways;
    }
    // Two lawn trees, and on some lots a wrought-iron fence along the lawn with a gap for the path.
    kit.tree(to_world(ground, yaw, half_w + 4.2F, 0, -half_d + 2));
    kit.tree(to_world(ground, yaw, -half_w - 4.6F, 0, half_d - 3));
    if ((r / 12) % 2 == 0) {
        const float z = -half_d - 6.4F;
        const std::vector<std::string> iron{urban + "Fences/metal_fence/metalfence_one_side_topbar.glb"};
        fence(kit, to_world(ground, yaw, 1.6F, 0, z), to_world(ground, yaw, 12.4F, 0, z), iron, 2.7F, r);
        fence(kit, to_world(ground, yaw, -12.4F, 0, z), to_world(ground, yaw, -7.2F, 0, z), iron, 2.7F, r + 1);
    }
    kit.put(urban + "Litter/TrashCan/trash_can_alt1.glb", to_world(ground, yaw, -7.4F, 0, -half_d + .5F), yaw);
    return true;
}

void east_suburbs(Kit& kit, world::VerdaRegion& region) {
    auto* capital = settlement(region, "capital_verda");
    if (!capital) return;
    // The Porto Luma highway now leaves from the boulevard's east end.
    for (auto& road : capital->roads)
        for (auto* end : {&road.start, &road.end})
            if (std::abs(end->x - grid_edge_x) < .5F && std::abs(end->z) < .5F) {
                const auto& other = end == &road.start ? road.end : road.start;
                if (other.x > east_far_x) {kit.forget_road(road); *end = {east_far_x, end->y, 0};}
            }
    capital->roads.push_back({{east_near_x, 0, 0}, {east_far_x, 0, 0}, 10, world::RoadType::Asphalt});
    for (const float z : {-168.0F, -84.0F, 84.0F, 168.0F})
        capital->roads.push_back({{east_near_x, 0, z}, {east_far_x, 0, z}, 8, world::RoadType::Asphalt});
    for (const float x : {east_mid_x, east_far_x})
        capital->roads.push_back({{x, 0, -168}, {x, 0, 168}, 8, world::RoadType::Asphalt});
    kit.add_roads(capital->roads);
    int cars = 0;
    const int before = kit.report.houses;
    std::uint32_t seed = 8000;
    for (const float street_z : {-168.0F, -84.0F, 84.0F, 168.0F})
        for (const int facing : {1, -1}) {
            const float z = street_z - facing * 21.0F;
            for (const auto& [from, count] : std::initializer_list<std::pair<float, int>>{{191, 5}, {351, 4}})
                for (int i = 0; i < count; ++i) fancy_house(kit, *capital, {from + i * 30.0F, 0, z}, facing, ++seed, cars);
        }
    for (const float street_z : {-168.0F, -84.0F, 84.0F, 168.0F})
        for (int i = 0; i < 6; ++i) {
            const float x = east_near_x + 26 + i * 50.0F, side = i % 2 ? 1.0F : -1.0F;
            if (std::abs(x - east_mid_x) < 8) continue;
            kit.put(urban + "Traffic lights/street_light_clean.glb", {x, 0, street_z + side * 5.4F}, side > 0 ? 0.0F : 180.0F);
        }
    // The boulevard: trees and tall double lamps both sides, a park, and the clubhouse.
    for (float x = east_near_x + 12; x < east_far_x - 8; x += 20)
        for (const float side : {-1.0F, 1.0F}) {
            if (side > 0 && x > 340 && x < 420) continue; // clubhouse frontage
            if (std::abs(x - east_mid_x) < 8) continue;    // the cross street
            kit.tree({x, 0, side * 8.5F});
        }
    for (float x = east_near_x + 22; x < east_far_x - 8; x += 40)
        kit.put("assets/verda/transit/lamppost_01.glb", {x, 0, static_cast<int>(x) % 80 < 40 ? 6.4F : -6.4F}, static_cast<int>(x) % 80 < 40 ? 0.0F : 180.0F);
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 2; ++j) {
        const auto r = hash(8500U + static_cast<std::uint32_t>(i * 2 + j));
        kit.tree({196 + i * 32.0F + static_cast<float>(r % 7), 0, -24 - j * 16.0F - static_cast<float>((r / 7) % 4)});
    }
    for (int i = 0; i < 4; ++i) kit.put(urban + "Bus stops/busstop_clean_bench.glb", {210 + i * 32.0F, 0, -13}, 180);
    modular_building(kit, {380, 0, 24}, 0, 8, 4, "fancy_brick", 2, 8601, world::BuildingPurpose::Pub); // clubhouse
    capital->survivors += (kit.report.houses - before) * 3; // a family in every home
}
// ------------------------------------------------------------------ pickup trucks
// Working pickups where a truck belongs: Roka's industrial yard, the Porto Luma waterfront,
// Espera's farm and a Sovaĝejo homestead. Each takes the first clear spot near where it is wanted.
void trucks(Kit& kit, world::VerdaRegion& region) {
    struct Want { const char* town; Vector3 at; float yaw; };
    const auto port = site("port_luma"), espera = site("village_espera");
    const Want wanted[]{
        {"port_luma", {port.x + 186, 0, port.z - 62}, 0}, {"port_luma", {port.x + 186, 0, port.z + 62}, 180},
        {"village_espera", {espera.x - 80 + 20, 0, espera.z + 80 - 8}, 90},   // in the co-op yard
        {"ferarbaro_sovagejo", {world::layout::sovagejo_center.x + 9, 0, world::layout::sovagejo_center.z + 64}, 0}, // by the park gate
    };
    std::map<std::string, int> count;
    for (const auto& want : wanted) {
        auto* town = settlement(region, want.town);
        if (!town) continue;
        for (const float shift : {0.0F, 4.0F, -4.0F, 8.0F, -8.0F, 12.0F}) {
            const float a = want.yaw * DEG2RAD;
            const Vector3 spot{want.at.x + std::cos(a) * shift, 0, want.at.z - std::sin(a) * shift};
            if (!kit.claim(spot, 1.3F, 3.0F, want.yaw)) continue;
            vehicle_marker(*town, "vehicle_spawn_truck_" + town->id + "_" + std::to_string(++count[town->id]), spot, want.yaw);
            ++kit.report.trucks;
            break;
        }
    }
}
// ------------------------------------------------------------------ Ruldomaro
// "Ruldomaro" - a gathering of ruldomoj (mobile homes): a small trailer-park hamlet on its own
// level pad off the eastern ring road, between Espera and Porto Luma. A gravel spur leaves the
// highway (the bus stops there) into a gravel lane with two rows of trailers, a general store
// by the gate, a storage shed, a fire pit with benches, street lights and a chainlink fence.
// Loot lies in the homes (camp), the store (general), the shed (weapons) and by the fire pit
// (medical). Its own settlement, so it has people, a name on the map and a stop on the line.
void trailer_hamlet(Kit& kit, world::VerdaRegion& region, const world::layout::Site& hamlet, std::uint32_t seed, int per_side) {
    const Vector3 c = hamlet.center;
    const std::string slug(hamlet.id.substr(hamlet.id.find('_') + 1));
    world::Settlement* town = settlement(region, hamlet.id);
    if (!town) {
        world::Settlement fresh;
        fresh.id = std::string(hamlet.id);
        fresh.name = std::string(hamlet.name);
        fresh.center = c;
        fresh.state = world::SettlementState::Peaceful;
        fresh.survivors = 0;
        region.runtime_settlements().push_back(std::move(fresh));
        town = &region.runtime_settlements().back();
    }
    // Local frame: the lane runs (+a) toward the highway - due east for Ruldomaro - and +b is to
    // its right.
    Vector3 u{1, 0, 0};
    if (hamlet.id != "hamlet_ruldomaro")
        if (auto* capital = settlement(region, "capital_verda")) {
            float nearest = 1e9F;
            for (const auto& road : capital->roads) {
                const float dx = road.end.x - road.start.x, dz = road.end.z - road.start.z, l2 = dx * dx + dz * dz;
                if (l2 < 1) continue;
                const float t = std::clamp(((c.x - road.start.x) * dx + (c.z - road.start.z) * dz) / l2, 0.0F, 1.0F);
                const Vector3 p{road.start.x + dx * t, 0, road.start.z + dz * t};
                const float d = std::hypot(p.x - c.x, p.z - c.z);
                if (d < nearest && d > 1) { nearest = d; u = {(p.x - c.x) / d, 0, (p.z - c.z) / d}; }
            }
            // Square to the compass (lots are claimed as axis-aligned boxes); the spur angles off.
            u = std::abs(u.x) > std::abs(u.z) ? Vector3{u.x > 0 ? 1.0F : -1.0F, 0, 0} : Vector3{0, 0, u.z > 0 ? 1.0F : -1.0F};
        }
    const Vector3 v{-u.z, 0, u.x};
    const auto at = [&](float a, float b) { return Vector3{c.x + u.x * a + v.x * b, 0, c.z + u.z * a + v.z * b}; };
    const float lane_yaw = std::atan2(u.x, u.z) * RAD2DEG; // facing out along the lane
    // The spur meets the highway square-on: the nearest point of the ring road.
    Vector3 junction = at(80, 0);
    float best = 1e9F;
    if (auto* capital = settlement(region, "capital_verda"))
        for (const auto& road : capital->roads) {
            const float dx = road.end.x - road.start.x, dz = road.end.z - road.start.z, l2 = dx * dx + dz * dz;
            if (l2 < 1) continue;
            const Vector3 from = at(48, 0);
            const float t = std::clamp(((from.x - road.start.x) * dx + (from.z - road.start.z) * dz) / l2, 0.0F, 1.0F);
            const Vector3 p{road.start.x + dx * t, 0, road.start.z + dz * t};
            const float d = std::hypot(p.x - from.x, p.z - from.z);
            if (d < best) { best = d; junction = p; }
        }
    constexpr float lane_half = 3;
    town->roads.push_back({at(-58, 0), at(48, 0), lane_half * 2, world::RoadType::Gravel});
    town->roads.push_back({at(48, 0), junction, 6, world::RoadType::Gravel});
    kit.add_roads(town->roads);
    int homes = 0, cars = 0, loot = 0;
    const auto loot_marker = [&](const char* table, Vector3 p, float height) {
        world::GameplayMarker marker;
        marker.id = std::string("loot_spawn_") + table + "_" + slug + "_" + std::to_string(++loot);
        marker.type = world::GameplayMarkerType::LootSpawn;
        marker.position = {p.x, world::terrain::TerrainHeight::sample(p.x, p.z) + height, p.z};
        marker.size = {.5F, .5F, .5F};
        town->gameplay_markers.push_back(marker);
    };
    // Two rows of trailers either side of the lane, doors and steps to the lane.
    for (int side = 0; side < 2; ++side) for (int i = 0; i < per_side; ++i) {
        const auto r = hash(seed + static_cast<std::uint32_t>(side * per_side + i));
        const float a = -42 + i * 14.0F, b = side == 0 ? -13.5F : 13.5F;
        // The model's steps are on its +Z: the -b row faces +b, the +b row faces -b.
        const float yaw = lane_yaw + (side == 0 ? -90.0F : 90.0F) + static_cast<float>(static_cast<int>(r % 7) - 3);
        const int model = 1 + static_cast<int>((side * 5 + i * 3 + r) % 6);
        const auto centre = at(a, b);
        if (!kit.claim(centre, 3.4F, 7.8F, yaw)) continue;
        if (!kit.put(trailers + "trailer_0" + std::to_string(model) + ".glb", centre, yaw)) continue;
        auto inside = to_world(centre, yaw, .25F, 0, .56F);
        inside.y = world::terrain::TerrainHeight::sample(centre.x, centre.z) + .85F;
        kit.purpose(inside, 90 + yaw, world::BuildingPurpose::Home);
        ++homes;
        loot_marker(i % 3 == 2 ? "general" : "camp", to_world(centre, yaw, .25F, 0, -2.2F), .9F);
        if (r % 3 != 0) {
            const bool pickup = cars == 1;
            vehicle_marker(*town, std::string(pickup ? "vehicle_spawn_truck_" : "vehicle_spawn_hatchback_") + slug + "_" + std::to_string(++cars),
                           to_world(centre, yaw, side == 0 ? 6.8F : -6.8F, 0, 3.0F), yaw);   // nose to the lane: it can drive straight out
        }
        prop(kit, shacks + (r % 2 ? "Barrel.glb" : "Pallet.glb"), to_world(centre, yaw, 2.4F, 0, -9.2F), static_cast<float>(r % 360), .8F);
        if (i % 2 == side) kit.tree(to_world(centre, yaw, -1.2F, 0, -10.5F));
    }
    // The general store by the gate, facing the lane; the shed and the fire pit at the far end.
    modular_building(kit, at(30, -17), lane_yaw + 90, 5, 4, "stuco", 1, seed + 11, world::BuildingPurpose::Shop);
    loot_marker("general", at(30, -21), .3F);
    if (kit.claim(at(-55, -16), 2.4F, 3.4F, lane_yaw)) {
        kit.put(shacks + "Shack A.glb", at(-55, -16), lane_yaw);
        loot_marker("weapons", at(-55, -16), .15F);
    }
    prop(kit, "assets/verda/survival/runtime/bonfire.glb", at(-52, 12), 0, 1.2F);
    for (const auto& [da, db, yaw] : std::initializer_list<std::tuple<float, float, float>>{{-52, 7.8F, 0}, {-52, 16.2F, 180}, {-47.8F, 12, 90}})
        prop(kit, urban + "Bus stops/busstop_bench.glb", at(da, db), lane_yaw + yaw, 1.6F);
    loot_marker("medical", at(-49, 15), .1F);
    for (const float a : {-36.0F, -8.0F, 20.0F}) kit.put(urban + "Traffic lights/street_light.glb", at(a, lane_half + 1.2F), lane_yaw + 90, false);
    const std::vector<std::string> chain{shacks + "Chainlink Fence.glb"};
    fence(kit, at(-62, -26), at(44, -26), chain, 2.46F, seed + 71);
    fence(kit, at(-62, 26), at(44, 26), chain, 2.46F, seed + 72);
    fence(kit, at(-62, -26), at(-62, 26), chain, 2.46F, seed + 73);
    fence(kit, at(44, -26), at(44, -lane_half - 2), chain, 2.46F, seed + 74);
    fence(kit, at(44, lane_half + 2), at(44, 26), chain, 2.46F, seed + 75);
    town->survivors += homes * 3 + 2;
    ++kit.report.hamlets;
}
}

void ruldomaro(Kit& kit, world::VerdaRegion& region) { trailer_hamlet(kit, region, world::layout::hamlets[0], 1700, 4); }
// Three more trailer stops by the highways: Vojrando on the Porto Luma road, Pinejo on the
// north-west ring road and Sablovalo on the south-west ring road - three trailers a side, a
// general store, a shed and a fire pit, with a gravel spur straight onto the highway. No bus stop.
void highway_hamlets(Kit& kit, world::VerdaRegion& region) {
    for (std::size_t i = 1; i < world::layout::hamlets.size(); ++i)
        trailer_hamlet(kit, region, world::layout::hamlets[i], 2000U + static_cast<std::uint32_t>(i) * 100U, 3);
}

// ------------------------------------------------------------------ loot in buildings
// Every enterable building gets a loot spot on its ground floor: the modular buildings the kit
// raised, the procedural houses, the house models and the Main Street rows. Mostly general
// supplies, some medical, a quarter weapons caches - so a raid on a street is worth the risk.
void building_loot(Kit& kit, world::VerdaRegion& region) {
    auto& towns = region.runtime_settlements();
    if (towns.empty()) return;
    std::vector<Vector3> spots;
    for (const auto& b : kit.report.enterable) spots.push_back(b.origin);
    for (const auto& town : towns) {
        for (const auto& b : town.buildings) if (b.enterable) spots.push_back(b.position);
        for (const auto& asset : town.assets)
            if (asset.model_path.find("residential/houses/") != std::string::npos || asset.model_path.find("/city/main_street/") != std::string::npos)
                spots.push_back(asset.position);
    }
    int n = 0;
    for (const auto& p : spots) {
        const auto r = hash(5000U + static_cast<std::uint32_t>(n));
        const int pick = static_cast<int>(r % 100);
        const char* table = pick < 55 ? "general" : pick < 75 ? "medical" : "weapons";
        const float terrain = world::terrain::TerrainHeight::sample(p.x, p.z);
        world::GameplayMarker marker;
        marker.id = std::string("loot_spawn_") + table + "_bldg_" + std::to_string(++n);
        marker.type = world::GameplayMarkerType::LootSpawn;
        marker.position = {p.x, world::physics::WorldCollision::ground_height({p.x, terrain + 1.2F, p.z}, terrain + 1.2F, region), p.z};
        marker.size = {.5F, .5F, .5F};
        // Filed under the nearest town.
        std::size_t best = 0;
        for (std::size_t t = 1; t < towns.size(); ++t)
            if (std::hypot(towns[t].center.x - p.x, towns[t].center.z - p.z) < std::hypot(towns[best].center.x - p.x, towns[best].center.z - p.z)) best = t;
        towns[best].gameplay_markers.push_back(marker);
        ++kit.report.loot_spots;
    }
}

// ------------------------------------------------------------------ Policio and Hospitalo
// A modular police station (city kit civic/police_station.glb) in every town (not the small
// trailer hamlets by the highways), facing one of its streets on level ground near the centre,
// with a patrol car in its carport (two in Verda City).
// Verda City also gets Hospitalo de Verda near its station: the hospital with a drive-under
// ambulance bay and three ambulances in it.
struct CivicSpot { Vector3 origin; float yaw; };
const std::string police_model = "assets/verda/city/civic/police_station.glb";
const std::string hospital_model = "assets/verda/city/civic/hospital.glb";

bool civic_lot(Kit& kit, world::VerdaRegion& region, Vector3 near, float min_distance, float max_distance, float lot_x, float lot_z,
               float half_x, float half_z, float front, float apron, CivicSpot& out,
               const std::function<bool(const CivicSpot&)>& accept = {}, bool dirt = false) {
    struct Candidate { float score; Vector3 origin; float yaw; };
    std::vector<Candidate> candidates;
    for (const auto& town : region.settlements())
        for (const auto& road : town.roads) {
            if (road.type == world::RoadType::Dirt && !dirt) continue;
            const float length = Vector3Distance(road.start, road.end);
            if (length < 20) continue;
            const Vector3 along = Vector3Normalize(Vector3Subtract(road.end, road.start));
            const Vector3 normal{-along.z, 0, along.x};
            for (float t = 10; t <= length - 10; t += 8)
                for (const float side : {-1.0F, 1.0F}) {
                    const Vector3 p = Vector3Add(road.start, Vector3Scale(along, t));
                    const Vector3 n = Vector3Scale(normal, side);
                    const Vector3 origin = Vector3Add(p, Vector3Scale(n, road.width * .5F + apron + front));
                    const float distance = std::hypot(origin.x - near.x, origin.z - near.z);
                    if (distance < min_distance || distance > max_distance) continue;
                    candidates.push_back({distance, origin, std::atan2(n.x, n.z) * RAD2DEG}); // front (-Z) faces the road
                }
        }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.score < b.score; });
    for (const auto& c : candidates) {
        const Vector3 centre = to_world(c.origin, c.yaw, lot_x, 0, lot_z);
        // Level ground, and no building, wall or tree in the way.
        float lo = 1e9F, hi = -1e9F;
        for (const float sx : {-1.0F, 0.0F, 1.0F}) for (const float sz : {-1.0F, 0.0F, 1.0F}) {
            const Vector3 q = to_world(centre, c.yaw, sx * half_x, 0, sz * half_z);
            const float h = world::terrain::TerrainHeight::sample(q.x, q.z);
            lo = std::min(lo, h); hi = std::max(hi, h);
        }
        if (hi - lo > 1.2F) continue;
        bool blocked = false;
        const float a = c.yaw * DEG2RAD;
        for (const auto& town : region.settlements()) {
            for (const auto& asset : town.assets) {
                if (!asset.collision) continue;
                const float dx = asset.position.x - centre.x, dz = asset.position.z - centre.z;
                const float lx = dx * std::cos(a) - dz * std::sin(a), lz = dx * std::sin(a) + dz * std::cos(a);
                const float r = std::max(asset.size.x, asset.size.z) * .5F;
                if (std::abs(lx) < half_x + r && std::abs(lz) < half_z + r) { blocked = true; break; }
            }
            if (blocked) break;
            // Parked cars (their markers reserve no lot): leave them room to drive out.
            for (const auto& marker : town.gameplay_markers) {
                if (marker.type != world::GameplayMarkerType::VehicleSpawn) continue;
                const float dx = marker.position.x - centre.x, dz = marker.position.z - centre.z;
                const float lx = dx * std::cos(a) - dz * std::sin(a), lz = dx * std::sin(a) + dz * std::cos(a);
                if (std::abs(lx) < half_x + 5 && std::abs(lz) < half_z + 5) { blocked = true; break; }
            }
            if (blocked) break;
        }
        if (blocked) continue;
        const CivicSpot spot{{c.origin.x, world::terrain::TerrainHeight::sample(c.origin.x, c.origin.z), c.origin.z}, c.yaw};
        if ((accept && !accept(spot)) || !kit.claim(centre, half_x, half_z, c.yaw)) continue;
        out = spot;
        return true;
    }
    return false;
}

void emergency_services(Kit& kit, world::VerdaRegion& region) {
    Vector3 capital_station{};
    bool capital_found = false;
    std::vector<std::string> ids;
    for (const auto& town : region.settlements()) ids.push_back(town.id);
    for (const auto& id : ids) {
        auto* town = settlement(region, id);
        if (!town) continue;
        if (id.starts_with("hamlet_")) continue;   // a few trailers by the highway: no station
        const bool capital = id == "capital_verda";
        const Vector3 centre = town->center;
        // The station: 14 x 12 m with a 6 m carport on +X (model bounds x -7..13, z -6..6).
        CivicSpot spot;
        // The patrol cars park along the kerb in front; each needs a clear run ahead to pull out
        // (no lamp post or tree on the kerb in its way).
        const auto way_out = [&](const CivicSpot& at) {
            for (const float x : {9.0F, -6.0F}) {
                const Vector3 from = to_world(at.origin, at.yaw, x, 0, -9.2F), to = to_world(at.origin, at.yaw, x + 14, 0, -9.2F);
                for (const auto& town : region.settlements()) for (const auto& asset : town.assets) {
                    if (!asset.collision || !asset.vehicle.definition.empty()) continue;
                    const float dx = to.x - from.x, dz = to.z - from.z, l2 = dx * dx + dz * dz;
                    const float t = std::clamp(((asset.position.x - from.x) * dx + (asset.position.z - from.z) * dz) / l2, 0.0F, 1.0F);
                    if (std::hypot(asset.position.x - from.x - dx * t, asset.position.z - from.z - dz * t) < 1.6F + std::max(asset.size.x, asset.size.z) * .5F) return false;
                }
            }
            return true;
        };
        if (!civic_lot(kit, region, centre, capital ? 60 : 15, capital ? 420 : 260, 3, 0, 10.5F, 7, 6, 5, spot, way_out)) continue;
        if (!kit.put(police_model, spot.origin, spot.yaw)) continue;
        kit.purpose(to_world(spot.origin, spot.yaw, 0, .35F, -4.8F), spot.yaw, world::BuildingPurpose::Police);
        // The patrol car on the apron in front, along the kerb (vehicles treat a model's whole
        // bounds as solid, so nothing parks under the carport roof); Verda City has a second.
        auto* owner = settlement(region, id);
        vehicle_marker(*owner, "vehicle_spawn_police_" + id + "_1", to_world(spot.origin, spot.yaw, 9, 0, -9.2F), spot.yaw + 90);
        ++kit.report.police_cars;
        if (capital) {
            vehicle_marker(*owner, "vehicle_spawn_police_" + id + "_2", to_world(spot.origin, spot.yaw, -6, 0, -9.2F), spot.yaw + 90);
            ++kit.report.police_cars;
            capital_station = spot.origin;
            capital_found = true;
        }
        ++kit.report.police_stations;
    }
    // Hospitalo de Verda near the capital's police station (model bounds x -28..18, z -13..10).
    if (!capital_found) return;
    auto* capital = settlement(region, "capital_verda");
    CivicSpot spot;
    if (!capital || !civic_lot(kit, region, capital_station, 25, 320, -5, -1.5F, 24, 12.5F, 13, 4, spot)) return;
    if (!kit.put(hospital_model, spot.origin, spot.yaw)) return;
    kit.purpose(to_world(spot.origin, spot.yaw, 0, .35F, -8.5F), spot.yaw, world::BuildingPurpose::Clinic);
    // Along the kerb in front of the hospital, clear of its bounds (the bay is for show).
    for (int i = 0; i < 3; ++i)
        vehicle_marker(*capital, "vehicle_spawn_ambulance_capital_" + std::to_string(i + 1),
                       to_world(spot.origin, spot.yaw, -24 + static_cast<float>(i) * 15, 0, -15.2F), spot.yaw + 90);
    kit.report.ambulances += 3;
    kit.report.hospital = true;
}

// ---------------------------------------------------------------- government
// Registaro de Verda, the island's parliament, on the nearest free, level lot to the centre of
// Verda City along a paved street - at the head of the south blocks, facing the city across the
// street, by the highway in from the south - and an Urbodomo - a county town hall - in the main town of each of the other four counties
// (Espera, Porto Luma, Suda Haveno, Roka). Both are Office places for now (clerks work there);
// their real government role comes later. Flat open land east of Orienta is kept free for a
// future baseball stadium.
const std::string parliament_model = "assets/verda/city/civic/parliament.glb";
const std::string town_hall_model = "assets/verda/city/civic/town_hall.glb";
const std::string stela_turo_model = "assets/verda/city/towers/stela_turo.glb";
constexpr Vector3 parliament_site{0, 0, 0}; // searched outward from the city centre

void government(Kit& kit, world::VerdaRegion& region) {
    // The stadium ground first, so nothing below (or added after) takes it.
    if (kit.claim(stadium_reserve, stadium_half, stadium_half, 0)) kit.report.stadium_reserved = true;
    // Parliament (model bounds x -24..24 with the flags at +-13.5, z -20.7 (steps) ..11): its
    // lawn (14 m) between the street and the steps.
    CivicSpot spot;
    if (!kit.report.parliament && civic_lot(kit, region, parliament_site, 0, 700, 0, -4.5F, 25, 17, 21, 14, spot) && kit.put(parliament_model, spot.origin, spot.yaw)) {
        kit.purpose(to_world(spot.origin, spot.yaw, 0, 1.85F, -8), spot.yaw, world::BuildingPurpose::Office);
        kit.report.parliament = true;
    }
    // Town halls (bounds x -10.5..10.5 with the flag inside, z -9.7 (steps) ..7).
    for (const auto& site : world::layout::sites) {
        if (site.id == "capital_verda") continue;
        auto* town = settlement(region, std::string(site.id));
        if (!town) continue;
        CivicSpot hall;
        if (!civic_lot(kit, region, town->center, 12, 280, 0, -1.3F, 11.5F, 9, 10, 6, hall)) continue;
        if (!kit.put(town_hall_model, hall.origin, hall.yaw)) continue;
        kit.purpose(to_world(hall.origin, hall.yaw, 0, .95F, -4.5F), hall.yaw, world::BuildingPurpose::Office);
        ++kit.report.town_halls;
    }
    // Stela Turo, the art deco skyscraper downtown (placed in downtown(); bounds x -15.3..15.3,
    // z -14.6..14.6 with the marquees, 86 m to the spire): its lobby is an office (the building's
    // staff); the penthouse on top is for sale in Explore (game::economy::Penthouse). Loot on every
    // other floor for Battle Royale.
    {
        CivicSpot tower;
        bool placed = false;
        for (const auto& town : region.settlements())
            for (const auto& asset : town.assets)
                if (!placed && asset.model_path == stela_turo_model) { tower = {asset.position, asset.rotation_y}; placed = true; }
        if (placed) {
            kit.purpose(to_world(tower.origin, tower.yaw, 0, .25F, -9), tower.yaw, world::BuildingPurpose::Office);
            ++kit.report.retagged; // the tower is a building island life knows; the marker makes it offices
            kit.report.stela_turo = true;
            kit.report.stela_turo_at = tower.origin;
            auto* capital = settlement(region, "capital_verda");
            int n = 0;
            for (int floor = 1; capital && floor <= 15; floor += 2) {
                const float y = floor * 3.6F + .25F;
                for (const auto& [x, z, table] : {std::tuple{-1.5F, 5.0F, "general"}, std::tuple{11.0F, 6.0F, floor % 4 == 1 ? "weapons" : "medical"}}) {
                    if (floor >= 11 && std::abs(x) > 9) continue; // inside the setbacks
                    world::GameplayMarker marker;
                    marker.id = std::string("loot_spawn_") + table + "_stela_" + std::to_string(++n);
                    marker.type = world::GameplayMarkerType::LootSpawn;
                    marker.position = to_world(tower.origin, tower.yaw, x, y, z);
                    marker.size = {.5F, .5F, .5F};
                    capital->gameplay_markers.push_back(marker);
                    ++kit.report.loot_spots;
                }
            }
        }
    }
}

namespace {
// ------------------------------------------------------------------ Vakero ranches
// Five family ranches, one in each county, each beside a highway on level open land (see
// world/Ranches.hpp for the layout): a farmhouse, a red barn, grain bins, a windmill, a fenced
// pasture, a corral with a loading chute by the road, a corn field and a wheat field, and the
// family's side-by-side in the yard. And the stockyard of Porto Luma, where the cattle truck unloads
// and the two big rigs are parked.
namespace rr = world::ranches;
const std::string ranch_dir = "assets/verda/ranch/";

// The nearest point on a highway's centre line (where a farm track joins it).
Vector3 highway_point(const world::VerdaRegion& region, Vector3 p) {
    Vector3 best = p;
    float nearest = 1e9F;
    for (const auto& town : region.settlements())
        for (const auto& road : town.roads) {
            if (road.width < 5) continue;
            const float dx = road.end.x - road.start.x, dz = road.end.z - road.start.z, l2 = dx * dx + dz * dz;
            if (l2 < 1) continue;
            const float t = std::clamp(((p.x - road.start.x) * dx + (p.z - road.start.z) * dz) / l2, 0.0F, 1.0F);
            const Vector3 q{road.start.x + dx * t, 0, road.start.z + dz * t};
            const float d = std::hypot(q.x - p.x, q.z - p.z);
            if (d < nearest && d > .5F) { nearest = d; best = q; }
        }
    return best;
}

world::Settlement& site_settlement(world::VerdaRegion& region, std::string_view id, std::string_view name, const rr::Frame& frame, Vector2 centre) {
    world::Settlement fresh;
    fresh.id = std::string(id);
    fresh.name = std::string(name);
    fresh.center = frame.at(centre);
    fresh.center.y = world::terrain::TerrainHeight::sample(fresh.center.x, fresh.center.z);
    fresh.state = world::SettlementState::Peaceful;
    region.runtime_settlements().push_back(std::move(fresh));
    return region.runtime_settlements().back();
}

void fence_box(Kit& kit, const rr::Frame& f, const rr::Area& a, std::uint32_t seed,
               std::initializer_list<std::tuple<int, float, float>> gaps) {
    // Edges: 0 near (v0), 1 far (v1), 2 left (u0), 3 right (u1). A gap is (edge, from, to) along it.
    const std::vector<std::string> rail{ranch_dir + "fence_8m.glb"};
    const auto run = [&](int edge, Vector2 from, Vector2 to) {
        const bool along_u = edge < 2;
        float s0 = along_u ? from.x : from.y;
        const float s1 = along_u ? to.x : to.y;
        std::vector<std::pair<float, float>> open;
        for (const auto& [e, g0, g1] : gaps) if (e == edge) open.emplace_back(g0, g1);
        std::sort(open.begin(), open.end());
        open.emplace_back(s1, s1);
        for (const auto& [g0, g1] : open) {
            const float e0 = std::min(g0, s1);
            if (e0 - s0 > 2) {
                const Vector2 a = along_u ? Vector2{s0, from.y} : Vector2{from.x, s0};
                const Vector2 b = along_u ? Vector2{e0, from.y} : Vector2{from.x, e0};
                fence(kit, f.at(a), f.at(b), rail, 8, seed + static_cast<std::uint32_t>(edge * 31));
            }
            s0 = std::max(s0, g1);
        }
    };
    run(0, {a.u0, a.v0}, {a.u1, a.v0});
    run(1, {a.u0, a.v1}, {a.u1, a.v1});
    run(2, {a.u0, a.v0}, {a.u0, a.v1});
    run(3, {a.u1, a.v0}, {a.u1, a.v1});
}

void ranches(Kit& kit, world::VerdaRegion& region) {
    for (std::size_t r = 0; r < rr::ranches.size(); ++r) {
        const auto& spec = rr::ranches[r];
        const rr::Frame f(spec.gate, spec.up_track);
        const float yaw = f.yaw();
        const std::string slug(spec.id.substr(6));
        auto& ranch = site_settlement(region, spec.id, spec.name, f, rr::yard);
        ranch.survivors = 6;
        // The farm track, gate to yard (the game finds the ranch's frame from it), and its mouth
        // onto the highway.
        ranch.roads.push_back({f.gate, f.at(rr::yard), 5, world::RoadType::Gravel});
        ranch.roads.push_back({highway_point(region, f.gate), f.gate, 5, world::RoadType::Gravel});
        kit.add_roads(ranch.roads);
        const std::uint32_t seed = 9100U + static_cast<std::uint32_t>(r) * 97U;
        // Farmhouse (door toward the track), barn (doors up and down the track), bins, windmill.
        const Vector3 right = f.right();
        const float house_yaw = std::atan2(-right.x, -right.z) * RAD2DEG;
        modular_building(kit, f.at(rr::house), house_yaw, 4, 3, r % 2 ? "brick" : "stuco", 2, seed, world::BuildingPurpose::Home);
        kit.put(ranch_dir + "barn.glb", f.at(rr::barn), yaw);
        kit.purpose(f.at(rr::barn.x, rr::barn.y - 10.6F), yaw + 180, world::BuildingPurpose::Farm);
        for (const auto& bin : rr::bins) kit.put(ranch_dir + "grain_bin.glb", f.at(bin), yaw);
        kit.put(ranch_dir + "windmill.glb", f.at(rr::windmill), yaw + 90);
        // Pasture and corral (open between them), the chute at the corral's road side.
        fence_box(kit, f, rr::pasture, seed + 1, {{0, rr::lane.u0, rr::lane.u1}});
        fence_box(kit, f, rr::corral, seed + 2, {{1, rr::lane.u0, rr::lane.u1}, {0, rr::chute.x - 2, rr::chute.x + 2}});
        kit.put(ranch_dir + "loading_chute.glb", f.at(rr::chute.x, rr::chute.y + 2.2F), yaw + 180);
        kit.put(ranch_dir + "feed_bunk.glb", f.at(-48, 40), yaw + 90);
        kit.put(ranch_dir + "water_trough.glb", f.at(-40, 46), yaw);
        kit.put(ranch_dir + "water_trough.glb", f.at(-30, 12), yaw);
        for (int i = 0; i < 4; ++i) kit.put(ranch_dir + "hay_bale.glb", f.at(38.5F + (i % 2) * 1.7F, 60 + (i / 2) * 1.8F), yaw + 90);
        // Fields: corn 3 x 3 tiles, wheat 3 x 2.
        for (float u = rr::corn.u0 + 6; u < rr::corn.u1; u += 12)
            for (float v = rr::corn.v0 + 6; v < rr::corn.v1; v += 12) kit.put(ranch_dir + "crop_corn.glb", f.at(u, v), yaw);
        for (float u = rr::wheat.u0 + 6; u < rr::wheat.u1; u += 12)
            for (float v = rr::wheat.v0 + 6; v < rr::wheat.v1; v += 12) kit.put(ranch_dir + "crop_wheat.glb", f.at(u, v), yaw);
        // The side-by-side in the yard; something worth finding in the barn and the house.
        vehicle_marker(ranch, "vehicle_spawn_sxs_" + slug + "_1", f.at(rr::sxs), yaw);
        int loot = 0;
        for (const auto& [table, at] : {std::pair{"weapons", Vector2{rr::barn.x + 3, rr::barn.y - 4}}, std::pair{"general", Vector2{rr::barn.x - 3, rr::barn.y + 5}},
                                         std::pair{"camp", Vector2{rr::house.x, rr::house.y}}}) {
            world::GameplayMarker marker;
            marker.id = std::string("loot_spawn_") + table + "_" + slug + "_" + std::to_string(++loot);
            marker.type = world::GameplayMarkerType::LootSpawn;
            const Vector3 p = f.at(at);
            marker.position = {p.x, world::terrain::TerrainHeight::sample(p.x, p.z) + .4F, p.z};
            marker.size = {.5F, .5F, .5F};
            ranch.gameplay_markers.push_back(marker);
        }
        // Keep the rest of the footprint clear of anything laid later.
        kit.claim(f.at((rr::footprint.u0 + rr::footprint.u1) * .5F, (rr::footprint.v0 + rr::footprint.v1) * .5F),
                  (rr::footprint.u1 - rr::footprint.u0) * .5F - 30, (rr::footprint.v1 - rr::footprint.v0) * .5F - 30, yaw);
        kit.report.ranches.push_back(f.at(rr::yard));
    }
    // The stockyard of Porto Luma, on the ring road at the edge of the port city.
    {
        const rr::Frame f(rr::stockyard_gate, rr::stockyard_up);
        const float yaw = f.yaw();
        auto& yard = site_settlement(region, rr::stockyard_id, rr::stockyard_name, f, {0, 34});
        yard.survivors = 4;
        yard.roads.push_back({f.gate, f.at(0, 7), 6, world::RoadType::Gravel});   // the apron by the chute
        yard.roads.push_back({highway_point(region, f.gate), f.gate, 6, world::RoadType::Gravel});
        yard.roads.push_back({f.at(48, 0), f.at(52, 44), 8, world::RoadType::Gravel}); // the truck park
        yard.roads.push_back({highway_point(region, f.at(48, 0)), f.at(48, 0), 8, world::RoadType::Gravel});
        kit.add_roads(yard.roads);
        // Receiving pen behind the chute, opening each side into a big holding pen.
        const auto& in = rr::stockyard_receiving;
        const auto& left = rr::stockyard_pens[0];
        const auto& right = rr::stockyard_pens[1];
        fence_box(kit, f, in, 9600, {{0, rr::stockyard_chute.x - 2, rr::stockyard_chute.x + 2}, {2, 14, 20}, {3, 14, 20}});
        fence_box(kit, f, left, 9601, {{3, 14, 20}});
        fence_box(kit, f, right, 9602, {{2, 14, 20}});
        kit.put(ranch_dir + "loading_chute.glb", f.at(rr::stockyard_chute.x, rr::stockyard_chute.y + 2.2F), yaw + 180);
        for (const auto& pen : rr::stockyard_pens) {
            const float cu = (pen.u0 + pen.u1) * .5F;
            for (const float v : {pen.v1 - 4, pen.v1 - 14}) kit.put(ranch_dir + "feed_bunk.glb", f.at(cu, v), yaw);
            kit.put(ranch_dir + "water_trough.glb", f.at(cu, pen.v0 + 6), yaw);
        }
        for (int i = 0; i < 8; ++i) kit.put(ranch_dir + "hay_bale.glb", f.at(-10 + (i % 4) * 1.7F, 34 + (i / 4) * 1.8F), yaw);
        // The sale barn (the auction ring) and the office behind the pens.
        kit.put(ranch_dir + "barn.glb", f.at(0, 54), yaw + 90);
        modular_building(kit, f.at(-30, 54), yaw + 180, 4, 2, "brick", 1, 9603, world::BuildingPurpose::Farm);
        // The two big rigs park beside it: the freight hauler and the cattle hauler.
        vehicle_marker(yard, "vehicle_spawn_semi_freight_1", f.at(52, 16), yaw);
        vehicle_marker(yard, "vehicle_spawn_semi_cattle_1", f.at(52, 34), yaw);
        kit.report.stockyard = true;
    }
}
}

namespace {
// ------------------------------------------------------------------ highways
// The old two-lane links between the towns (what is left of the capital's roads once downtown is
// wiped) give way to the divided highways of world/Highways.hpp: the ring road and four spokes,
// laid before anything else so every lot keeps off them.
void lay_highways(world::VerdaRegion& region) {
    auto* capital = settlement(region, "capital_verda");
    if (!capital) return;
    capital->roads.clear();
    for (const auto& line : world::highways::lines()) {
        const auto n = line.points.size();
        for (std::size_t i = 0; i + (line.loop ? 0 : 1) < n; ++i)
            capital->roads.push_back({line.points[i], line.points[(i + 1) % n], world::highways::width, world::RoadType::Highway});
    }
}
}

// ------------------------------------------------------------------ the state prison
// Ŝtata Malliberejo (city kit civic/state_prison.glb) on its pad south-east of the ring road,
// gate toward the ring, and an access road from the ring to the gate (layout::prison_*).
void state_prison(Kit& kit, world::VerdaRegion& region) {
    const Vector3 c = world::layout::prison_center;
    const float yaw = world::layout::prison_yaw;
    auto* owner = settlement(region, "south_haven");
    if (!owner) return;
    // Nothing else on the pad.
    for (auto& town : region.runtime_settlements())
        kit.report.cleared += static_cast<int>(std::erase_if(town.assets, [&](const world::WorldAsset& a) {
            return a.vehicle.definition.empty() && std::hypot(a.position.x - c.x, a.position.z - c.z) < 60; }));
    kit.clear_area(c, 60);
    kit.claim(c, 43, 34, yaw);
    if (!kit.put(city + "civic/state_prison.glb", {c.x, world::terrain::TerrainHeight::sample(c.x, c.z), c.z}, yaw)) return;
    // The access road: out from the ring road (its point at the prison's bearing) to the gate.
    const float bearing = std::atan2(c.z, c.x), ring = 1500;
    const Vector3 from{ring * std::cos(bearing), 0, ring * std::sin(bearing)};
    const Vector3 gate = to_world(c, yaw, 0, 0, -34);
    owner->roads.push_back({from, gate, 7, world::RoadType::Asphalt});
    kit.add_roads({owner->roads.back()});
}

// ------------------------------------------------------------------ smugglers' jetties
// Where contraband comes ashore (Explore's SMUGGLE job): a jetty on the open shore with a coaster
// alongside (city kit civic/smugglers_jetty.glb), at the end of a gravel track down from Suda
// Haveno's south street. (At Porto Luma they land at the fishers' jetty: cargo_terminal.) The deck stands 2.2 m over
// the sea; the model's land end is its -Z.
void smugglers_jetties(Kit& kit, world::VerdaRegion& region) {
    struct Spot { const char* town; Vector3 start, dir; };
    const Vector3 port = site("port_luma"), haven = site("south_haven");
    const float edge = pitch + street * .5F + 40;
    (void)port;   // (Porto Luma's is the fishers' jetty on the quay now: cargo_terminal)
    for (const Spot& spot : {Spot{"south_haven", {haven.x + 40, 0, haven.z + edge}, {0, 0, 1}}}) {
        auto* town = settlement(region, spot.town);
        if (!town) continue;
        const float deck = world::layout::sea_level + 2.2F;
        Vector3 shore = spot.start;
        for (float t = 0; t < 600; t += 1) {
            const Vector3 p = Vector3Add(spot.start, Vector3Scale(spot.dir, t));
            if (world::terrain::TerrainHeight::sample(p.x, p.z) <= deck) { shore = p; break; }
        }
        if (Vector3Distance(shore, spot.start) < 20) continue;
        const float yaw = std::atan2(spot.dir.x, spot.dir.z) * RAD2DEG;
        const Vector3 track_end = Vector3Subtract(shore, Vector3Scale(spot.dir, 3));
        for (auto& owner : region.runtime_settlements())
            kit.report.cleared += static_cast<int>(std::erase_if(owner.assets, [&](const world::WorldAsset& a) {
                if (!a.vehicle.definition.empty()) return false;
                const Vector3 d = Vector3Subtract(a.position, spot.start);
                const float along = d.x * spot.dir.x + d.z * spot.dir.z, across = std::abs(d.x * spot.dir.z - d.z * spot.dir.x);
                return along > 2 && along < Vector3Distance(spot.start, shore) + 30 && across < 9;
            }));
        town->roads.push_back({spot.start, track_end, 6, world::RoadType::Dirt});
        kit.add_roads({town->roads.back()});
        const Vector3 centre = Vector3Add(shore, Vector3Scale(spot.dir, 22));
        kit.put(city + "civic/smugglers_jetty.glb", {centre.x, deck - 10, centre.z}, yaw, false);   // (the game finds the pickup from it)
    }
}

// ------------------------------------------------------------------ Porto Luma's quay
// Kargohaveno de Porto Luma (layout::cargo_*): a ramp from the quay road down to the quay at the
// shore, a lane along it, two ship-to-shore cranes on the quay edge over Stella Maris (a feeder
// container ship at the berth - she brings the island its supplies), stacks of containers either
// side of the ramp's mouth, and the cattle quay (world/Ranches.hpp): a pen behind a loading chute,
// and the gangway up the ship's side. The quay runs on south to the fishers' quay (layout::fish_z,
// quay_z1): the locals' end, the fishers' jetty off its face with the trawlers alongside and a
// parking area behind; a second ramp climbs from there back to the quay road. The lane joins the
// two ramps, so the quay is a loop: down one ramp, along the quay, up the other.
void cargo_terminal(Kit& kit, world::VerdaRegion& region) {
    namespace L = world::layout;
    auto* port = settlement(region, "port_luma");
    if (!port) return;
    const float z = L::cargo_z, quay = L::cargo_quay_height, lane = L::quay_lane_x;
    // The terminal road runs straight in from the town's east street (x = town_east), across the
    // quay road and down the ramp: a rig turns once, not a dog-leg along the quay road.
    const float town_east = port->center.x + 126;
    // Nothing (bollards, lamps, props, the waterfront shed in the way) on the roads, the ramps or the
    // quay (the water off the cargo end too, where the ship lies).
    for (auto& town : region.runtime_settlements())
        kit.report.cleared += static_cast<int>(std::erase_if(town.assets, [&](const world::WorldAsset& a) {
            const float x = a.position.x;
            if (!a.vehicle.definition.empty() || x < town_east || x > L::cargo_quay_x1 + 80) return false;
            if (x < L::cargo_ramp_x0 - 6) return std::abs(a.position.z - z) < 10;
            for (const float ramp : {z, L::fish_z})
                if (x < L::cargo_ramp_x1 && std::abs(a.position.z - ramp) < L::cargo_ramp_half + 3) return true;
            if (x < L::cargo_ramp_x1) return false;
            return std::abs(a.position.z - z) < L::cargo_quay_half + 4 ||
                   (x < L::cargo_quay_x1 + 2 && a.position.z > L::quay_z0 - 4 && a.position.z < L::quay_z1 + 4);
        }));
    port->roads.push_back({{town_east, 0, z}, {L::cargo_ramp_x0, 0, z}, truck_road, world::RoadType::Asphalt});
    port->roads.push_back({{L::cargo_ramp_x0, 0, z}, {lane, 0, z}, 13, world::RoadType::Asphalt});   // (between the ramp's walls)
    // The fishers' ramp, from the quay road down to the south end of the quay; the lane joins them.
    port->roads.push_back({{port->center.x + 196, 0, L::fish_z}, {lane, 0, L::fish_z}, 13, world::RoadType::Asphalt});
    port->roads.push_back({{lane, 0, z - L::cargo_quay_half + 4}, {lane, 0, L::fish_z}, truck_road, world::RoadType::Asphalt});
    kit.add_roads(port->roads);
    kit.put(city + "civic/quay_apron.glb", {(L::cargo_ramp_x1 + L::cargo_quay_x1) * .5F, quay + .02F, z}, 0, false, false);
    // The fishers' quay: paved (a parking area behind the jetty, for the locals), the jetty off its
    // face with the trawlers alongside.
    const float fish_mid = (L::cargo_z + L::cargo_quay_half + L::quay_z1) * .5F;
    kit.put(city + "ground/parking_lot_68.glb", {L::cargo_quay_x1 - 34, quay + .01F, fish_mid}, 0, false, false);
    kit.put(city + "civic/fishers_jetty.glb", {L::fishers_jetty.x + 22, quay - 10, L::fishers_jetty.z}, 90, false);   // (solid: walk out on it)
    for (int i = 0; i < 4; ++i) {
        const auto r = hash(9300U + static_cast<std::uint32_t>(i));
        prop(kit, shacks + (i % 2 ? "Pallet.glb" : "Barrel.glb"), {L::cargo_quay_x1 - 3 - static_cast<float>(r % 3), quay, L::fish_z - 18 - i * 2.5F},
             static_cast<float>(r % 360), 1);
    }
    // Marnafto's diesel lane for the rigs, beside the quay lane (layout::quay_diesel).
    kit.put(city + "civic/diesel_island.glb", {L::quay_diesel.x, quay, L::quay_diesel.z}, 0, false);
    // The ship at the berth, her quay side 1.2 m off the quay face; the keel 5 m under the sea.
    if (!kit.put(city + "civic/cargo_ship.glb", {L::cargo_quay_x1 + 9.2F, L::sea_level - 5, z}, 0, false, false)) return;
    for (const float cz : {z - 22, z})
        kit.put(city + "civic/harbour_crane.glb", {L::cargo_quay_x1 - 9, quay, cz}, 0, false, false);
    // Containers waiting on the quay, either side of the ramp's mouth (the cattle pen north).
    kit.put(city + "civic/container_stack.glb", {lane - 23, quay, z - 33}, 0, false);
    kit.put(city + "civic/container_stack.glb", {lane - 36, quay, z - 33}, 0, false);
    kit.put(city + "civic/container_stack.glb", {lane - 36, quay, z + 27}, 180, false);   // (clear of the 12 m lane down the ramp)
    // The cattle quay.
    const rr::Frame f(rr::cattle_quay_gate, rr::cattle_quay_up);
    fence_box(kit, f, rr::cattle_quay_pen, 9700, {{0, rr::cattle_quay_chute.x - 2, rr::cattle_quay_chute.x + 2}, {3, 10, 14}});
    kit.put(ranch_dir + "loading_chute.glb", f.at(rr::cattle_quay_chute.x, rr::cattle_quay_chute.y + 2.2F), f.yaw() + 180);
    kit.put(ranch_dir + "water_trough.glb", f.at(-6, 22), f.yaw());
    kit.put(city + "civic/cattle_gangway.glb", {rr::cattle_gangway_foot.x, quay, rr::cattle_gangway_foot.y}, 0, false, false);
    // Lamps and bollards along the quay edge, end to end (clear of the jetty's foot).
    for (float lz = z - 50; lz < L::quay_z1 - 2; lz += 25)
        if (std::abs(lz - L::fish_z) > 6) kit.put(urban + "Traffic lights/street_light_clean.glb", {L::cargo_quay_x1 - 2, quay, lz}, 90, false);
    for (float bz = z - 48; bz < L::quay_z1 - 2; bz += 8)
        if (std::abs(bz - L::fish_z) > 5) kit.put(urban + "Cones & Barriers/Bollards/square_bollard.glb", {L::cargo_quay_x1 - .6F, quay, bz}, 0, false);
    kit.report.cargo_terminal = true;
}

// ------------------------------------------------------------------ Verda Transporto
// Verda City's industrial quarter (layout::freight_*), west of Okcidenta: the suburb street on
// z = -84 runs on into the yard of Verda Transporto, the island's trucking company - the depot
// (sheds and the office, where the boss gives out loads) on its north side, the truck park on the
// south: the company's rigs for hire (vehicle_spawn_semi_depot_...) and its own drivers' rigs
// (vehicle_spawn_semi_haul_..., game::freight), with containers round about.
void freight_depot(Kit& kit, world::VerdaRegion& region) {
    namespace L = world::layout;
    auto* capital = settlement(region, "capital_verda");
    if (!capital) return;
    constexpr float road_z = -84;
    for (auto& town : region.runtime_settlements())
        kit.report.cleared += static_cast<int>(std::erase_if(town.assets, [&](const world::WorldAsset& a) {
            return a.vehicle.definition.empty() && a.position.x > L::freight_x0 && a.position.x < L::freight_x1 - 10 &&
                   a.position.z > L::freight_z0 && a.position.z < L::freight_z1; }));
    capital->roads.push_back({{suburb_west_x, 0, road_z}, {L::freight_x0 + 14, 0, road_z}, truck_road, world::RoadType::Asphalt});
    kit.add_roads({capital->roads.back()});
    const float ground = world::terrain::TerrainHeight::sample(-560, -54);
    if (!kit.put(city + "civic/truck_depot.glb", {-560, ground, -52}, 0, false)) return;
    kit.purpose({-560, ground, -52 - 11 - 3.5F}, 0, world::BuildingPurpose::Office);   // the office, inside its door
    for (const float x : {-524.0F, -592.0F})
        kit.put(city + "ground/parking_lot_68.glb", {x, ground, -126}, 0, false);
    kit.put(city + "civic/container_stack.glb", {-640, ground, -128}, 0, false);
    for (int i = 0; i < 6; ++i) {   // pallets and drums against the shed's flank
        const auto r = hash(9800U + static_cast<std::uint32_t>(i));
        kit.put(shacks + (i % 2 ? "Pallet.glb" : "Barrel.glb"), {-582 - static_cast<float>(i % 3) * 1.6F, 0, -48 + static_cast<float>(i / 3) * 1.6F}, static_cast<float>(r % 360));
    }
    for (int i = 0; i < 4; ++i)
        kit.put(urban + "Traffic lights/street_light.glb", {-500 - i * 40.0F, 0, road_z - truck_road * .5F - 1.4F}, 0);
    // The company's diesel lane, on the yard road's north side west of the office (layout::depot_diesel).
    kit.put(city + "civic/diesel_island.glb", L::depot_diesel, 90);
    // The truck park: noses to the road.
    int n = 0;
    for (const float x : {-512.0F, -526.0F, -540.0F})
        vehicle_marker(*capital, "vehicle_spawn_semi_depot_" + std::to_string(++n), {x, 0, -100}, 0);
    vehicle_marker(*capital, "vehicle_spawn_semi_depot_cattle_1", {-554, 0, -100}, 0);
    n = 0;
    for (const float x : {-580.0F, -594.0F, -608.0F})
        vehicle_marker(*capital, "vehicle_spawn_semi_haul_" + std::to_string(++n), {x, 0, -100}, 0);
    kit.report.freight_depot = true;
}

// ------------------------------------------------------------------ radio stations
// Verda's three radio stations (city kit civic/radio_station_<station>.glb, game::radio): each
// station's studio and transmitter mast in its own town, facing a street near the centre -
// Verda Popolko in Verda City, Radio Sovaĝejo on Sovaĝejo's dirt road, Espera Roko in Espera.
// The studio is an Office place (its staff work there; the sign names the station).
void radio_stations(Kit& kit, world::VerdaRegion& region) {
    struct Station { const char* style; Vector3 near; float min, max; bool dirt, porch; };
    const auto site = [&](std::string_view id) { const auto* t = settlement(region, id); return t ? t->center : Vector3{}; };
    const Station stations[]{
        {"popolko", site("capital_verda"), 70, 460, false, false},
        {"kantrio", world::layout::sovagejo_center, 0, 220, true, true},
        {"roko", site("village_espera"), 15, 300, false, false},
    };
    for (const auto& st : stations) {
        // Model bounds x -6.3..9.8 (the mast on +X), z -4.6..4.6 (-6.7 with the porch).
        const float front = st.porch ? 6.8F : 4.7F;
        CivicSpot spot;
        if (!civic_lot(kit, region, st.near, st.min, st.max, 1.75F, st.porch ? -1.1F : 0, 8.4F, st.porch ? 5.9F : 4.9F, front, 3.5F, spot, {}, st.dirt))
            continue;
        if (!kit.put(city + "civic/radio_station_" + st.style + ".glb", spot.origin, spot.yaw)) continue;
        const Vector3 door = to_world(spot.origin, spot.yaw, -3.9F, .35F, -3.3F);
        kit.purpose(door, spot.yaw, world::BuildingPurpose::Office);
        kit.report.radio_stations.push_back(door);
    }
}

// ------------------------------------------------------------------ gun shops
// Armilejo, Verda's gun shop chain (city kit civic/gun_shop.glb, game::economy's "guns" shops):
// a shop facing a paved street near the middle of each of the five main cities - Verda City,
// Espera, Porto Luma, Suda Haveno and Roka. Each is a Shop place; the sign names the chain.
void gun_shops(Kit& kit, world::VerdaRegion& region) {
    for (const char* id : {"capital_verda", "village_espera", "port_luma", "south_haven", "west_roka"}) {
        const auto* town = settlement(region, id);
        if (!town) continue;
        const bool capital = std::string_view(id) == "capital_verda";
        // Model bounds x -6.1..6.1, z -4.7..4.6.
        CivicSpot spot;
        if (!civic_lot(kit, region, town->center, capital ? 40 : 15, capital ? 380 : 260, 0, 0, 6.6F, 5.1F, 4.7F, 3.5F, spot)) continue;
        if (!kit.put(city + "civic/gun_shop.glb", spot.origin, spot.yaw)) continue;
        const Vector3 door = to_world(spot.origin, spot.yaw, 0, .35F, -3.3F);
        kit.purpose(door, spot.yaw, world::BuildingPurpose::Shop);
        kit.report.gun_shops.push_back(door);
    }
}

// ------------------------------------------------------------------ the gangs' houses
// The county gangs' hideouts in the four big towns the Green Hand don't hold (game::mafia): a plain
// detached house (the residential house model) on a street a little out from the middle, well away
// from the police station. Its purpose marker is named gang_house_<town>: nobody lives there
// (game::life), and the gang's crew moves in, its tag on the front wall.
void gang_houses(Kit& kit, world::VerdaRegion& region) {
    static constexpr const char* schemes[]{"white", "blue", "sage", "cream"};
    int k = 0;
    for (const char* id : {"village_espera", "port_luma", "south_haven", "west_roka"}) {
        const auto* town = settlement(region, id);
        if (!town) continue;
        std::vector<Vector3> stations;
        for (const auto& t : region.settlements())
            for (const auto& a : t.assets) if (a.model_path == police_model) stations.push_back(a.position);
        const auto away = [&](const CivicSpot& spot) {
            for (const auto& p : stations) if (std::hypot(p.x - spot.origin.x, p.z - spot.origin.z) < 140) return false;
            return true;
        };
        // Model bounds x -7.21..7.21, z -10.09..10.09; the porch (local -z) faces the street.
        CivicSpot spot;
        if (!civic_lot(kit, region, town->center, 50, 340, 0, 0, 8.2F, 11.1F, 10.1F, 3.0F, spot, away)) continue;
        if (!kit.put(houses + "house_" + schemes[k++ % 4] + ".glb", spot.origin, spot.yaw)) continue;
        ++kit.report.houses;
        // The front door (tools/extract_house.py): beside the garage, the front wall at z = -5.85.
        constexpr float door_x = -2.41F, front = 5.85F, floor = .06F;
        kit.report.enterable.push_back({spot.origin, spot.yaw, 13.6F, front * 2, 2, door_x, {-2.85F, 0, -2.6F}, {0, 0, 1}, 6, floor, 3.3F, {1.5F, 0, 0}});
        kit.purpose(to_world(spot.origin, spot.yaw, door_x, floor, -front + 1.2F), spot.yaw, world::BuildingPurpose::Home,
                    std::string("gang_house_") + id);
        ++kit.report.buildings;
        kit.report.gang_houses.push_back(spot.origin);
    }
}

// ------------------------------------------------------------------ the motor trade
// Marnafto (the island's oil company: its rigs out at sea, its filling stations on the roads),
// the Meĥanikejo workshops and Aŭtovendejo Verda's car lot (world/MotorTrade.hpp). Each building
// faces a street with room in front to drive in; the stations and the lot keep clear of downtown
// (and of the stadium ground, which is claimed).
void motor_trade(Kit& kit, world::VerdaRegion& region) {
    namespace M = world::motor;
    const auto town_at = [&](const char* id) { const auto* t = settlement(region, id); return t ? t->center : Vector3{}; };
    const auto clear_of_downtown = [](const CivicSpot& s) { return !world::layout::in_downtown(s.origin.x, s.origin.z); };
    // Filling stations: model bounds x -12..12, z -11..11; the forecourt fronts the road.
    const auto station = [&](Vector3 near, float min_d, float max_d, const std::function<bool(const CivicSpot&)>& accept) {
        CivicSpot spot;
        if (!civic_lot(kit, region, near, min_d, max_d, 0, 0, 12.4F, 11.4F, 11.2F, 3.5F, spot, accept)) return;
        if (!kit.put(M::gas_model, spot.origin, spot.yaw)) return;
        kit.purpose(to_world(spot.origin, spot.yaw, 0, .15F, 6.2F), spot.yaw, world::BuildingPurpose::Shop);   // the kiosk shop
        kit.report.gas_stations.push_back(spot.origin);
    };
    // Verda City: one by Verda Transporto's yard (the truckers' road out west), one out east on
    // the Porto Luma road.
    station(world::layout::freight_depot_gate, 20, 220, clear_of_downtown);
    station({476, 0, 0}, 0, 200, clear_of_downtown);
    for (const char* id : {"village_espera", "port_luma", "south_haven", "west_roka"})
        station(town_at(id), 30, 300, {});
    // One on the ring road (off its shoulder: drive straight on), on the first level ground going
    // round from between Suda Haveno and Sablovalo.
    for (const float degrees : {110.0F, 120.0F, 100.0F, 135.0F, 150.0F, 200.0F, 250.0F, 300.0F, 330.0F, 20.0F}) {
        const auto before = kit.report.gas_stations.size();
        const float a = degrees * DEG2RAD;
        station({1500 * std::cos(a), 0, 1500 * std::sin(a)}, 0, 120, [](const CivicSpot&) { return true; });
        if (kit.report.gas_stations.size() > before) break;
    }
    // Meĥanikejo: model bounds x -9..9, z -6.2..6; room in front to drive into the bays.
    const auto workshop = [&](Vector3 near, float min_d, float max_d, const std::function<bool(const CivicSpot&)>& accept) {
        CivicSpot spot;
        if (!civic_lot(kit, region, near, min_d, max_d, 0, 0, 9.4F, 6.6F, 6.4F, 6.0F, spot, accept)) return;
        if (!kit.put(M::mechanic_model, spot.origin, spot.yaw)) return;
        kit.purpose(to_world(spot.origin, spot.yaw, 6.6F, .1F, -4.8F), spot.yaw, world::BuildingPurpose::Garage);   // the office
        kit.report.mechanics.push_back(spot.origin);
    };
    // Aŭtovendejo Verda: model bounds x -18..18, z -13.6..13; the lot's front on the street.
    {
        CivicSpot spot;
        if (civic_lot(kit, region, {0, 0, 0}, 200, 620, 0, 0, 18.4F, 13.9F, 13.8F, 3.5F, spot, clear_of_downtown) &&
            kit.put(M::car_lot_model, spot.origin, spot.yaw)) {
            kit.purpose(to_world(spot.origin, spot.yaw, 12.8F, .12F, 7.2F), spot.yaw, world::BuildingPurpose::Office);   // the sales office
            kit.report.car_lot = true;
            kit.report.car_lot_at = spot.origin;
            // The cars on show, noses to the street: one of each the lot sells.
            auto* capital = settlement(region, "capital_verda");
            static constexpr std::array<const char*, 5> stock{"hatchback", "sxs", "sedan", "truck", "green_hand_gt"};
            for (std::size_t i = 0; i < stock.size() && capital; ++i)
                vehicle_marker(*capital, std::string("vehicle_spawn_") + stock[i] + "_forsale_" + std::to_string(i + 1),
                               to_world(spot.origin, spot.yaw, M::car_lot_spots[i], 0, M::car_lot_rows[0]), spot.yaw + 180);
            // The workshop next to it.
            workshop(spot.origin, 30, 260, clear_of_downtown);
        }
    }
    for (const char* id : {"village_espera", "port_luma", "south_haven", "west_roka"})
        workshop(town_at(id), 30, 320, {});
    // Platformoj Marnafto: three rigs out at sea, off the north-east, east and south coasts.
    for (const float degrees : {-60.0F, 10.0F, 75.0F}) {
        // Out past the shore (the first deep water on that bearing), 300 m further.
        const float a = degrees * DEG2RAD;
        float r = 1700;
        while (r < 3600 && world::terrain::TerrainHeight::sample(r * std::cos(a), r * std::sin(a)) > world::layout::sea_level - 6) r += 20;
        if (r >= 3600) continue;
        r += 300;
        const Vector3 at{r * std::cos(a), world::layout::sea_level, r * std::sin(a)};
        if (kit.put(M::oil_rig_model, at, degrees + 30, false, false)) kit.report.oil_rigs.push_back(at);
    }
}

// ------------------------------------------------------------------ freight yards
// Verda Transporto's drops in the big towns (game::freight's deliveries): just off the highway,
// before it narrows into town, a 16 m road out to a 14 m loop round a gravel yard - pull in, go
// round, unload, head back out the way you came; no squeezing a trailer through the town's
// streets. A shed and containers stand beyond the loop; the shed's purpose marker is named
// freight_yard_<town> (the game delivers there).
void freight_yards(Kit& kit, world::VerdaRegion& region) {
    constexpr float ring = 24, lane = 14, stem = 28;
    for (const char* id : {"village_espera", "port_luma", "south_haven", "west_roka"}) {
        auto* town = settlement(region, id);
        if (!town) continue;
        // The highway's end at this town (its point nearest the centre), and the way back out.
        const world::Road* in = nullptr;
        world::Settlement* owner = nullptr;
        Vector3 end{};
        float best = 400;
        for (auto& t : region.runtime_settlements())
            for (const auto& road : t.roads) {
                if (road.type != world::RoadType::Highway) continue;
                for (const Vector3 p : {road.start, road.end})
                    if (const float d = std::hypot(p.x - town->center.x, p.z - town->center.z); d < best) { best = d; end = p; in = &road; owner = &t; }
            }
        if (!in || !owner) continue;
        // Walk back along the highway, past where it narrows, to a stretch with room beside it.
        bool built = false;
        for (const float back : {210.0F, 250.0F, 290.0F, 330.0F, 380.0F})
            for (const float side : {1.0F, -1.0F}) {
                if (built) break;
                // The highway point `back` metres from the end, following the chain of pieces.
                Vector3 at = end, heading{};
                float left = back;
                const world::Road* piece = in;
                Vector3 from = end;
                for (int guard = 0; guard < 64 && piece && left > 0; ++guard) {
                    const Vector3 to = std::hypot(piece->start.x - from.x, piece->start.z - from.z) < .5F ? piece->end : piece->start;
                    const float l = std::hypot(to.x - from.x, to.z - from.z);
                    heading = Vector3Normalize({to.x - from.x, 0, to.z - from.z});
                    if (l >= left) { at = {from.x + heading.x * left, 0, from.z + heading.z * left}; left = 0; break; }
                    left -= l; from = to; at = to;
                    const world::Road* next = nullptr;
                    for (const auto& t : region.settlements())
                        for (const auto& road : t.roads)
                            if (road.type == world::RoadType::Highway && &road != piece &&
                                (std::hypot(road.start.x - to.x, road.start.z - to.z) < .75F || std::hypot(road.end.x - to.x, road.end.z - to.z) < .75F)) next = &road;
                    piece = next;
                }
                if (left > 0) continue;
                const Vector3 out{-heading.z * side, 0, heading.x * side};
                const Vector3 junction = at;   // on the highway's centre line: a side road joining it (a median opening)
                const Vector3 stem_end{junction.x + out.x * (world::highways::paved_half + stem), 0, junction.z + out.z * (world::highways::paved_half + stem)};
                const Vector3 centre{stem_end.x + out.x * ring, 0, stem_end.z + out.z * ring};
                // Dry, not too steep, and nothing in the way.
                float lo = 1e9F, hi = -1e9F;
                for (int k = 0; k < 12; ++k) {
                    const float a = static_cast<float>(k) * PI / 6;
                    const float h = world::terrain::TerrainHeight::sample(centre.x + std::cos(a) * ring, centre.z + std::sin(a) * ring);
                    lo = std::min(lo, h); hi = std::max(hi, h);
                }
                if (lo < world::layout::sea_level + 2 || hi - lo > 7) continue;
                bool open = true;
                for (const auto& t : region.settlements()) {
                    for (const auto& asset : t.assets)
                        if (std::hypot(asset.position.x - centre.x, asset.position.z - centre.z) < ring + lane + 12 ||
                            std::hypot(asset.position.x - (junction.x + stem_end.x) * .5F, asset.position.z - (junction.z + stem_end.z) * .5F) < stem * .5F + 6)
                            open = false;
                    for (const auto& road : t.roads) {
                        if (road.type == world::RoadType::Highway) continue;
                        for (const Vector3 p : {road.start, road.end, Vector3{(road.start.x + road.end.x) * .5F, 0, (road.start.z + road.end.z) * .5F}})
                            if (std::hypot(p.x - centre.x, p.z - centre.z) < ring + lane + 10) open = false;
                    }
                }
                if (!open || !kit.claim(centre, ring + lane * .5F + 2, ring + lane * .5F + 2, std::atan2(out.x, out.z) * RAD2DEG)) continue;
                owner->roads.push_back({junction, stem_end, 16, world::RoadType::Asphalt});
                for (int k = 0; k < 12; ++k) {
                    const auto corner = [&](int i) {
                        const float a = std::atan2(-out.z, -out.x) + static_cast<float>(i) * PI / 6;   // corner 0 on the stem
                        return Vector3{centre.x + std::cos(a) * ring, 0, centre.z + std::sin(a) * ring};
                    };
                    owner->roads.push_back({corner(k), corner(k + 1), lane, world::RoadType::Asphalt});
                }
                kit.add_roads(std::vector<world::Road>(owner->roads.end() - 13, owner->roads.end()));
                // Beyond the loop: the yard's drop point (facing the yard) and the containers.
                const float yaw = std::atan2(-out.x, -out.z) * RAD2DEG;   // facing back toward the highway
                const Vector3 shed{centre.x + out.x * (ring + lane + 4), 0, centre.z + out.z * (ring + lane + 4)};
                kit.purpose({shed.x - out.x * 4, world::terrain::TerrainHeight::sample(shed.x, shed.z) + .1F, shed.z - out.z * 4}, yaw,
                            world::BuildingPurpose::Industrial, std::string("freight_yard_") + id);
                const Vector3 across{-out.z, 0, out.x};
                // A Marnafto diesel lane inside the loop, on its far side: pull up beside it going round.
                // (Its centre 4.5 m inside the loop's inner edge: 2.5 m clear of the lane.)
                const float diesel = ring - lane * .5F - 4.5F;
                kit.put(city + "civic/diesel_island.glb", {centre.x + out.x * diesel, 0, centre.z + out.z * diesel}, std::atan2(across.x, across.z) * RAD2DEG);
                for (const float s : {-14.0F, 14.0F})
                    kit.put(city + "civic/container_stack.glb", {shed.x + across.x * s, world::terrain::TerrainHeight::sample(shed.x + across.x * s, shed.z + across.z * s),
                                                                  shed.z + across.z * s}, yaw + 90, false);
                kit.report.freight_yards.push_back(centre);
                built = true;
            }
    }
}

WorldKitReport build_verda_towns(world::VerdaRegion& region, const creator::CreatorAssetRegistry& catalog) {
    wipe_downtown(region);
    lay_highways(region);
    Kit kit(region, catalog);
    downtown(kit, region);
    downtown_south(kit, region);
    roka_city(kit, region);
    trailer_park(kit, region);
    suda_haveno(kit, region);
    espera(kit, region);
    driveways(kit, region);
    // Last, so every older piece keeps its id (saves refer to them).
    sovagejo_homesteads(kit, region);
    west_suburbs(kit, region);
    porto_luma_city(kit, region);
    east_suburbs(kit, region);
    ruldomaro(kit, region);
    highway_hamlets(kit, region);
    bus_stops(kit, region);
    trucks(kit, region);
    emergency_services(kit, region);
    building_loot(kit, region);
    // Last again: the loot keeps its ids.
    government(kit, region);
    // Last of all: the Vakeroj's ranches and the stockyard (new settlements and pieces only).
    ranches(kit, region);
    // After them (new pieces only): the state prison and the smugglers' jetties.
    state_prison(kit, region);
    smugglers_jetties(kit, region);
    // After those (new pieces only): Porto Luma's cargo terminal and Verda Transporto's depot.
    cargo_terminal(kit, region);
    freight_depot(kit, region);
    // After those (new pieces only): the radio stations.
    radio_stations(kit, region);
    // After those (new pieces only): Armilejo's gun shops.
    gun_shops(kit, region);
    // After those (new pieces only): the county gangs' houses.
    gang_houses(kit, region);
    // After those (new pieces only): Marnafto's stations and rigs, the workshops, the car lot.
    motor_trade(kit, region);
    // After those (new pieces only): Verda Transporto's freight yards off the highways.
    freight_yards(kit, region);
    // After those (new pieces only): La Verda Basballigo's ballparks, one in each county.
    ballparks(kit, region);
    return kit.report;
}
}
