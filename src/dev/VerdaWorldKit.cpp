#include "outland/dev/VerdaWorldKit.hpp"
#include "outland/creator/CreatorAssetRegistry.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/ParkingGarage.hpp"
#include "outland/world/Ballparks.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/Harbour.hpp"
#include "outland/world/MotorTrade.hpp"
#include "outland/world/MountainRoad.hpp"
#include "outland/world/NorthQuarter.hpp"
#include "outland/world/SovagejoCounty.hpp"
#include "outland/world/SkyTower.hpp"
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
    // Reserve a lot outright (no checks): ground already spoken for, like a roundabout's island.
    void reserve(Lot lot) { taken_.push_back(lot); }
    void release(Lot lot) {
        std::erase_if(taken_, [&](const Lot& l) { return l.x0 == lot.x0 && l.z0 == lot.z0 && l.x1 == lot.x1 && l.z1 == lot.z1; });
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
    // The east-west highway's run through downtown the same way, down z = 0.
    for (std::size_t i = 0; i + 1 < dt_street_x.size(); ++i) {
        const float x0 = dt_street_x[i] + 20, x1 = dt_street_x[i + 1] - 20;
        for (int k = 0; k <= 3; ++k) {
            const float x = x0 + (x1 - x0) * static_cast<float>(k) / 3.0F;
            if (k % 3 == 0) {
                kit.put(urban + "Traffic lights/street_light.glb", at(x, 0), 0, false);
                kit.put(urban + "Traffic lights/street_light.glb", at(x, 0), 180, false);
            } else kit.tree(at(x, 0));
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
    // (z = 0 is the east-west highway: the city's main avenue, a boulevard.)
    for (const float z : dt_street_z)
        if (z != 0) capital->roads.push_back({{-grid_edge_x, 0, z}, {grid_edge_x, 0, z}, dt_street, world::RoadType::Asphalt});
    for (const float x : dt_street_x) for (const float z : dt_street_z)
        if (x != 0 && z != 0) kit.put(city + "ground/intersection_16.glb", {x, ground, z}, 0, false);
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
            // Mostly hatchbacks; a sedan, an estate, a van or a muscle car between them.
            static constexpr std::array<const char*, 7> parked_kinds{"hatchback", "sedan", "hatchback", "wagon", "van", "hatchback", "muscle"};
            marker.id = std::string("vehicle_spawn_") + parked_kinds[parked % parked_kinds.size()] + "_downtown_" + std::to_string(parked + 1);
            ++parked;
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
// The island's everyday cars, the n-th parked one: mostly hatchbacks, with sedans, estates, vans
// and the odd muscle car among them.
const char* everyday_car(int n) {
    static constexpr std::array<const char*, 8> kinds{"hatchback", "wagon", "hatchback", "sedan", "hatchback", "van", "hatchback", "muscle"};
    return kinds[static_cast<std::size_t>(n) % kinds.size()];
}
// ------------------------------------------------------------------ Norda Kvartalo
// Verda City's north side (world::north): the avenues carry on north past downtown's edge to two
// new streets across the highway; two rows of blocks, each split by an alley, built up like a
// Chicago neighbourhood - greystone three-flats, brick walk-ups (some with a corner store, some
// with a mural on the end wall), a corner tavern on some rows - and La Levita, the elevated
// railway, over the second row's alley. (The first row's two blocks beside the highway are left
// to the police station and the hospital, which emergency_services() puts there.)
void north_quarter(Kit& kit, world::VerdaRegion& region) {
    namespace nq = world::north;
    world::Settlement* capital = nullptr;
    for (auto& settlement : region.runtime_settlements()) if (settlement.id == "capital_verda") capital = &settlement;
    if (!capital) return;
    const float ground = world::terrain::TerrainHeight::sample(0, 0);
    const auto at = [&](float x, float z, float up = 0) { return Vector3{x, ground + pad_top + up, z}; };
    const std::string north = city + "north/";
    // Streets: the avenues on north from the grid's edge, the two new streets across the highway.
    std::vector<world::Road> roads;
    const float far = -nq::streets[1] + dt_street * .5F;   // (to the far street's outer edge)
    for (const float x : nq::avenues) roads.push_back({{x, 0, -grid_edge_z}, {x, 0, -far}, dt_street, world::RoadType::Asphalt});
    for (const float z : nq::streets) roads.push_back({{-grid_edge_x, 0, z}, {grid_edge_x, 0, z}, dt_street, world::RoadType::Asphalt});
    for (const auto& road : roads) capital->roads.push_back(road);
    kit.add_roads(roads);
    for (const float x : nq::avenues) for (const float z : nq::streets) kit.put(city + "ground/intersection_16.glb", {x, ground, z}, 0, false);
    // A row of buildings along one side of a block: kinds left to right as the row faces (yaw 0
    // fronts -z; yaw 180 fronts +z). F three-flat, W walk-up, S walk-up with a corner store, M
    // walk-up with a mural on its left end wall, T corner tavern; '_' a 4 m gangway.
    static constexpr std::array<const char*, 5> patterns{"MFFFFT", "WFFST", "FFFWFF_", "SWFFF_", "MFFWF"};
    int parked = 0, homes = 0;
    const auto row = [&](float bx, float bz, bool north_side, std::uint32_t seed) {
        const float yaw = north_side ? 0.0F : 180.0F, sign = north_side ? 1.0F : -1.0F;
        const char* pattern = patterns[seed % patterns.size()];
        const bool red = (seed / 5) % 2;
        float along = -nq::front;
        for (const char* c = pattern; *c; ++c) {
            const char kind = *c;
            const float width = kind == 'F' ? 8.0F : kind == 'T' ? 12.3F : kind == '_' ? 4.0F : 16.3F;
            const float mid = along + width * .5F;
            along += width;
            if (kind == '_') continue;
            const float setback = kind == 'T' ? 8.0F : 9.0F;   // the front wall from the model's centre
            const Vector3 p = at(bx + sign * mid, bz - sign * (nq::front - setback));
            std::string model;
            const auto pick = hash(seed * 31U + static_cast<std::uint32_t>(c - pattern));
            switch (kind) {
                case 'F': model = std::string("three_flat_") + (pick % 3 ? "grey_" : "buff_") + std::to_string(1 + (pick / 3) % 2); break;
                case 'W': model = std::string("walkup_") + (red ? "red" : "brown"); break;
                case 'S': model = std::string("walkup_") + (red ? "brown" : "red") + "_shop"; break;
                case 'M': model = std::string("walkup_") + (red ? "red" : "brown") + "_mural_" + (pick % 2 ? "verda" : "geometrio"); break;
                default: model = "corner_tavern_" + std::to_string(1 + pick % 2); break;
            }
            if (!kit.claim(p, width * .5F - .05F, 9.0F, yaw)) continue;
            if (!kit.put(north + model + ".glb", p, yaw, false)) continue;
            ++kit.report.north_buildings;
            ++kit.report.buildings;
            // Walk in (tools/generate_city_kit.py walk_in()): the door in the front wall, the floors,
            // the stairs up the +X wall (0.2 m risers, 0.28 m treads, from 2.5 m inside the front).
            // A three-flat's front is up its stoop (its foot 1.8 m out), its first floor 1.2 m up.
            const bool flat3 = kind == 'F', tavern = kind == 'T';
            const float half_d = tavern ? 8.0F : 9.0F, base = flat3 ? 1.2F : 0.0F, first = tavern ? 4.0F : kind == 'S' ? 3.8F : 3.4F;
            const float half_w = flat3 ? 3.8F : tavern ? 6.0F : 8.0F, slab = .2F, run = std::round(first / .2F) * .28F;
            dev::KitBuilding inside;
            inside.origin = p;
            inside.yaw = yaw;
            inside.width = half_w * 2;
            inside.depth = (half_d + (flat3 ? 1.8F : 0.0F)) * 2;
            inside.storeys = flat3 ? 3 : tavern ? 2 : 4;
            inside.door_x = flat3 ? 1.75F : 0.0F;
            inside.stairs_start = {half_w - .25F - .65F, base + slab, -half_d + 1.5F};
            inside.stairs_direction = {0, 0, 1};
            inside.stairs_length = run + 2.2F;
            inside.ground_floor = base + slab;
            inside.upper_floor = base + first + slab;
            inside.upstairs_walk = {-3, 0, 0};
            kit.report.enterable.push_back(inside);
            // Who it's for: flats are homes, the corner store a shop, the tavern a pub.
            const auto purpose = tavern ? world::BuildingPurpose::Pub : kind == 'S' ? world::BuildingPurpose::Shop : world::BuildingPurpose::Home;
            kit.purpose(to_world(p, yaw, inside.door_x - (flat3 ? 0.0F : 2.5F), base + slab + .1F, -half_d + 1.2F), yaw, purpose);
            homes += purpose == world::BuildingPurpose::Home;
            if (kind == 'S') {kit.purpose(to_world(p, yaw, -5.0F, first + slab + .1F, 2.0F), yaw, world::BuildingPurpose::Home); ++homes;}   // the flats over the shop
        }
        // Cars at the kerb along the street in front: mostly hatchbacks, now and then a motorcycle.
        static constexpr std::array<const char*, 6> kinds{"hatchback", "sedan", "hatchback", "motorcycle", "wagon", "hatchback"};
        for (const float a : {-22.0F, 4.0F, 24.0F}) {
            if (hash(seed * 7U + static_cast<std::uint32_t>(a + 40)) % 3 == 0) continue;
            const std::string kind = kinds[static_cast<std::size_t>(parked) % kinds.size()];
            vehicle_marker(*capital, "vehicle_spawn_" + kind + "_norda_" + std::to_string(++parked),
                           {bx + a, ground, bz - sign * (pad_size * .5F + 1.2F)}, north_side ? 90.0F : 270.0F);
        }
    };
    std::vector<std::pair<float, float>> blocks;
    for (const float bx : dt_block_x) for (const float bz : nq::rows)
        if (bz == nq::rows[1] || std::abs(bx) > 90) blocks.push_back({bx, bz});
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const auto [bx, bz] = blocks[i];
        kit.put(city + "ground/alley_block_68.glb", {bx, ground, bz}, 0, false);
        const auto seed = static_cast<std::uint32_t>(4100 + i * 13);
        row(bx, bz, true, seed);
        row(bx, bz, false, seed + 6);
        // Street trees and lamps on the sidewalks; the alley's bins and a tree or two in the yards.
        for (int side = 0; side < 4; ++side) {
            const float e = pad_size * .5F - 1.6F;
            const auto spot = [&](float a) {
                return side == 0 ? at(bx + a, bz - e) : side == 1 ? at(bx + e, bz + a) : side == 2 ? at(bx - a, bz + e) : at(bx - e, bz - a);
            };
            // (On the avenues' sides only: the street fronts are the buildings' doors.)
            if (side % 2 == 0) continue;
            for (const float a : {-16.0F, 16.0F}) kit.tree(spot(a));
            kit.put(urban + "Traffic lights/street_light.glb", spot(12.0F), side == 1 ? 270.0F : 90.0F, false);
        }
        for (const float x : {-24.0F, 2.0F, 26.0F})
            for (const float z : {-1.0F, 1.0F})
                kit.put(urban + "Litter/TrashCan/trash_can_alt1.glb", {bx + x + z * 3, ground, bz + z * (nq::alley_half + .5F)}, z > 0 ? 180.0F : 0.0F, true, false);
        if (bz == nq::rows[0]) for (const float x : {-18.0F, 18.0F}) for (const float z : {-7.0F, 7.0F}) kit.tree(at(bx + x, bz + z));
        ++kit.report.north_blocks;
    }
    // La Levita (world::north), the whole line: the deck in 20 m pieces down the straights and 4.4 m
    // pieces round the curves; a bent every 16 m or so - wide ones, posts out on the verges, where
    // the line runs down a suburb street - except in the streets it crosses; and its five stations.
    {
        const float total = nq::length();
        const auto yaw_at = [](float s) { Vector2 h{}; nq::at(s, &h); return std::atan2(h.x, h.y) * RAD2DEG; };
        const auto straight_ahead = [&](float s, float len) {
            // (s .. s + len within one straight piece of the line)
            float base = 0;
            for (const auto& piece : nq::pieces) {
                const float l = nq::detail::piece_length(piece);
                if (s >= base - .01F && s + len <= base + l + .01F) return !piece.arc;
                base += l;
            }
            return false;
        };
        for (float s = 0; s < total - .5F;) {
            const bool long_piece = straight_ahead(s, nq::l_piece);
            const float len = long_piece ? nq::l_piece : 4.0F;
            const Vector2 c = nq::at(std::min(s + len * .5F, total));
            kit.put(north + (long_piece ? "l_deck.glb" : "l_deck_4.glb"), {c.x, ground, c.y}, yaw_at(s + len * .5F), false);
            s += len;
        }
        // Where a bent's posts may not stand: in the streets the line runs over or crosses (with a
        // metre to spare). A bent goes up where both its posts stand clear - a standard one, else a
        // wide one - or not at all.
        const auto in_a_street = [](Vector2 q) {
            if (q.y < -176 && std::abs(q.x) < 11) return true;                                          // the highway
            for (const float ax : nq::avenues) if (q.y < -176 && std::abs(q.x - ax) < 9) return true;  // Norda Kvartalo's avenues
            if (std::abs(std::abs(q.x) - nq::suburb_street) < 5 && q.y > -170) return true;               // the suburb streets
            for (const float sz : {-168.0F, -84.0F})                                                     // and their cross streets
                if (std::abs(q.y - sz) < 7.5F && std::abs(q.x) > 179 && std::abs(q.x) < 480) return true;
            return false;
        };
        for (float s = 2; s < total; s += 16) {
            const Vector2 c = nq::at(s);
            for (const float half : {3.0F, 6.2F}) {
                if (in_a_street(nq::beside(s, half)) || in_a_street(nq::beside(s, -half))) continue;
                kit.put(north + (half < 4 ? "l_bent.glb" : "l_bent_wide.glb"), {c.x, ground, c.y}, yaw_at(s), false);
                ++kit.report.l_bents;
                break;
            }
        }
        for (std::size_t i = 0; i < nq::stations.size(); ++i) {
            const float s = nq::station_s(i);
            const Vector2 c = nq::at(s);
            kit.claim({c.x, 0, c.y}, 8.4F, nq::station_half, yaw_at(s));
            if (kit.put(north + "l_station.glb", {c.x, ground, c.y}, yaw_at(s), false)) ++kit.report.l_stations;
        }
        // Okcidenta Parko: the park round the west terminal - trees, benches under the station.
        const Vector2 park = nq::stations.front().at;
        for (const float dx : {-30.0F, -12.0F, 12.0F, 30.0F})
            for (const float dz : {-16.0F, 16.0F}) kit.tree({park.x + dx, 0, park.y + dz});
        for (const float dx : {-8.0F, 8.0F})
            kit.put(city + "civic/waiting_shelter.glb", {park.x + dx, 0, park.y + 11}, 180);
    }
    // The flats bring their people (Verda City's stated population caps island life).
    capital->survivors += homes * 2;
    // Keep the rest of the kit off the new blocks (the first row's inner two stay free).
    for (const auto& [bx, bz] : blocks) kit.claim({bx, 0, bz}, 36, 36, 0);
}

// ------------------------------------------------------------------ Kristala Turo
// Verda's tallest building (world::tower) on the block east of the highway at the foot of Norda
// Kvartalo, its plaza and its way in to the south, toward downtown: a lobby anyone can walk into,
// a concierge at the desk, an elevator up to the penthouse that is for sale.
void sky_tower(Kit& kit, world::VerdaRegion& region) {
    namespace tw = world::tower;
    if (!settlement(region, "capital_verda")) return;
    const Vector3 at{tw::site.x, world::terrain::TerrainHeight::sample(tw::site.x, tw::site.z), tw::site.z};
    // (The plaza reaches 25 m out in front and 15 behind; the tower is 26 m square.)
    if (!kit.claim(to_world(at, tw::site_yaw, 0, 0, -5), 18.5F, 20.5F, tw::site_yaw)) return;
    if (!kit.put(tw::model, at, tw::site_yaw, false)) return;
    dev::KitBuilding lobby;
    lobby.origin = at;
    lobby.yaw = tw::site_yaw;
    lobby.width = lobby.depth = tw::half * 2;
    lobby.storeys = 1;
    lobby.door_x = 0;
    lobby.ground_floor = tw::lobby_floor;
    kit.report.enterable.push_back(lobby);
    // (A tower is a place of its own - offices - so it needs no purpose marker.)
    kit.report.sky_tower = true;
}

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
        vehicle_marker(*roka, std::string(trucks % 3 == 2 ? "vehicle_spawn_flatbed_roka_" : "vehicle_spawn_truck_roka_") + std::to_string(trucks + 1), p, yaw);
        ++trucks;
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
            ++parked;
            vehicle_marker(*roka, std::string("vehicle_spawn_") + everyday_car(parked - 1) + "_roka_" + std::to_string(parked), {c.x + along, 0, c.z + bz + (bz > 0 ? -curb : curb)}, 90);
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
    for (const float x : {-150.0F, -175.0F, -200.0F}) kit.put(urban + "Traffic lights/street_light.glb", {c.x + x, 0, c.z - 6.4F}, 180);   // (on the land: these spots carry no height of their own)
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
    for (const float a : {-26.0F, 0.0F, 26.0F}) kit.put(urban + "Traffic lights/street_light.glb", at(a, lane_half + 1.2F), lane_yaw + 90);   // (on the land: these spots carry no height of their own)
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
            ++parked;
            vehicle_marker(*town, std::string("vehicle_spawn_") + everyday_car(parked + 2) + "_" + std::string(plan.tag) + "_" + std::to_string(parked), p, along_z ? 0.0F : 90.0F);
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
                // The family's car: mostly hatchbacks, some estates, the odd van or sedan.
                static constexpr std::array<const char*, 6> drive_kinds{"hatchback", "wagon", "hatchback", "sedan", "hatchback", "van"};
                car.id = std::string("vehicle_spawn_") + drive_kinds[(r / 4) % drive_kinds.size()] + "_" + town.id + "_drive_" + std::to_string(++parked);
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
// Each ballpark's plot, kept free from the start (whichever way the park ends up facing).
Lot ballpark_plot(Vector3 centre) {
    constexpr float reach = 76;   // (the model's half-length: a park facing along the grid, as they all do)
    return {centre.x - reach, centre.z - reach, centre.x + reach, centre.z + reach};
}

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
                if (road.type == world::RoadType::Highway || world::one_way(road.type)) continue;   // (a street, not a highway or its slip roads)
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
        // (The others on the plots kept for them from the start: ballpark_plot.)
        const Lot reserve{stadium_reserve.x - stadium_half, stadium_reserve.z - stadium_half, stadium_reserve.x + stadium_half, stadium_reserve.z + stadium_half};
        const bool on_reserve = std::hypot(park.centre.x - stadium_reserve.x, park.centre.z - stadium_reserve.z) < 1;
        const Lot plot = ballpark_plot(park.centre);
        if (!kit.claim(middle, (B::model_max.x - B::model_min.x) * .5F, (B::model_max.z - B::model_min.z) * .5F, yaw, on_reserve ? &reserve : &plot)) continue;
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
// The avenue on z = 0 through both suburbs: the highways from Roka and Porto Luma arrive at its
// ends and it runs on into downtown at downtown's own width - a main road all the way, not a
// suburban street between the highway and the city.
constexpr float avenue = 16;

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
    // (The avenue on z = 0 is the east-west highway, laid with the others.)
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
            kit.put(urban + "Traffic lights/street_light.glb", {x, 0, street_z + side * (street_z == 0 ? avenue * .5F + 1.4F : street_z == -84 ? truck_road * .5F + 1.4F : 5.4F)}, side > 0 ? 0.0F : 180.0F);
        }
    // On the boulevard: the park (north-east), shops (south-east), a church and a clinic (west).
    for (int i = 0; i < 5; ++i) for (int j = 0; j < 3; ++j) {
        const auto r = hash(7500U + static_cast<std::uint32_t>(i * 3 + j));
        const Vector3 t{-196 - i * 27.0F + static_cast<float>(r % 7), 0, -18 - j * 14.0F - static_cast<float>((r / 7) % 5)};
        if (world::north::distance(t.x, t.z) > 10) kit.tree(t);   // (clear of La Levita and Okcidenta Parko)
    }
    for (int i = 0; i < 4; ++i)
        kit.put(urban + "Bus stops/busstop_bench.glb", {-210 - i * 32.0F, 0, -(avenue * .5F + 1.6F)}, 180);
    modular_building(kit, {-212, 0, 20}, 0, 8, 4, "stuco", 1, 7601, world::BuildingPurpose::Shop);
    modular_building(kit, {-270, 0, 20}, 0, 8, 4, "brick", 1, 7602, world::BuildingPurpose::Shop);
    // (The church on the boulevard is the city's Baptist chapel: churches() puts it there.)
    modular_building(kit, {-420, 0, 20}, 0, 8, 4, "stuco", 1, 7604, world::BuildingPurpose::Clinic);
    for (int i = 0; i < 3; ++i)
        vehicle_marker(*capital, "vehicle_spawn_hatchback_okcidento_shops_" + std::to_string(i + 1), {-226 - i * 22.0F, 0, avenue * .5F + 1.6F}, 270);
    // Two motorcycles at the kerb between them (the Verda Fulmo).
    for (int i = 0; i < 2; ++i)
        vehicle_marker(*capital, "vehicle_spawn_motorcycle_okcidento_shops_" + std::to_string(i + 1), {-237 - i * 22.0F, 0, avenue * .5F + 1.2F}, 270);
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
            ++parked;
            vehicle_marker(*port, std::string("vehicle_spawn_") + everyday_car(parked) + "_porto_" + std::to_string(parked), p, along_z ? 0.0F : 90.0F);
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
    } else if ((r / 4) % 13 == 1) {
        vehicle_marker(capital, "vehicle_spawn_motorcycle_orienta_" + std::to_string(++cars),
                       to_world(ground, yaw, -4.4F, 0, -half_d - 1.6F), yaw + 180);
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
            kit.tree({x, 0, side * (avenue * .5F + 2.5F)});
        }
    for (float x = east_near_x + 22; x < east_far_x - 8; x += 40)
        kit.put("assets/verda/transit/lamppost_01.glb", {x, 0, static_cast<int>(x) % 80 < 40 ? avenue * .5F + 1.4F : -(avenue * .5F + 1.4F)}, static_cast<int>(x) % 80 < 40 ? 0.0F : 180.0F);
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 2; ++j) {
        const auto r = hash(8500U + static_cast<std::uint32_t>(i * 2 + j));
        kit.tree({196 + i * 32.0F + static_cast<float>(r % 7), 0, -24 - j * 16.0F - static_cast<float>((r / 7) % 4)});
    }
    for (int i = 0; i < 4; ++i) kit.put(urban + "Bus stops/busstop_clean_bench.glb", {210 + i * 32.0F, 0, -(avenue * .5F + 4.5F)}, 180);
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
    for (const float a : {-36.0F, -8.0F, 20.0F}) kit.put(urban + "Traffic lights/street_light.glb", at(a, lane_half + 1.2F), lane_yaw + 90);   // (on the land: these spots carry no height of their own)
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
// ambulance bay and three ambulances in it. Every other town gets a Kliniko near its centre,
// with an ambulance at the kerb in front.
struct CivicSpot { Vector3 origin; float yaw; };
const std::string police_model = "assets/verda/city/civic/police_station.glb";
const std::string hospital_model = "assets/verda/city/civic/hospital.glb";
const std::string clinic_model = "assets/verda/city/civic/clinic.glb";

bool civic_lot(Kit& kit, world::VerdaRegion& region, Vector3 near, float min_distance, float max_distance, float lot_x, float lot_z,
               float half_x, float half_z, float front, float apron, CivicSpot& out,
               const std::function<bool(const CivicSpot&)>& accept = {}, bool dirt = false, float level = 1.2F) {
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
        if (hi - lo > level) continue;
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

// A drive-in's spot, seated on the highest ground under its lot (half_x by half_z about its
// origin): its forecourt never sunk into a slope - at most standing a little proud on the low
// side, where the apron in front of it and its concrete skirt meet the land (world::highways::Apron).
Vector3 seated(const CivicSpot& spot, float half_x, float half_z) {
    float high = -1e9F;
    for (const float sx : {-1.0F, -.5F, 0.0F, .5F, 1.0F})
        for (const float sz : {-1.0F, -.5F, 0.0F, .5F, 1.0F}) {
            const Vector3 q = to_world(spot.origin, spot.yaw, sx * half_x, 0, sz * half_z);
            high = std::max(high, world::terrain::TerrainHeight::sample(q.x, q.z));
        }
    return {spot.origin.x, high, spot.origin.z};
}

// ------------------------------------------------------------------ Preĝejoj
// A church in every town (game::culture::Faith): the west of the island is Baptist - white
// clapboard chapels with a spire (city kit civic/church_baptist.glb) - and the east Esperanto
// Orthodox, onion domes over white walls (civic/church_orthodox.glb). Verda City has both: the
// Baptist chapel on the west boulevard (La Verda Mano's: they are Baptists) and the Orthodox
// church in the east suburbs.
const std::string baptist_model = "assets/verda/city/civic/church_baptist.glb";
const std::string orthodox_model = "assets/verda/city/civic/church_orthodox.glb";

void churches(Kit& kit, world::VerdaRegion& region) {
    // Model bounds: the chapel x -5.35..5.35, z -11.2 (portico) ..9.35; the church x -7.55..7.55,
    // z -10.6 (porch) ..8.3. Front (-Z) to the street.
    // (A forest town's lanes are dirt: a chapel may face one.)
    const auto build = [&](Vector3 near, float from, float to, bool baptist, bool dirt = false) {
        CivicSpot spot;
        const bool found = baptist ? civic_lot(kit, region, near, from, to, 0, -.9F, 5.9F, 10.6F, 11.4F, 3, spot, {}, dirt)
                                   : civic_lot(kit, region, near, from, to, 0, -1.15F, 8.1F, 9.8F, 10.8F, 3, spot, {}, dirt);
        if (!found || !kit.put(baptist ? baptist_model : orthodox_model, spot.origin, spot.yaw)) return false;
        kit.purpose(to_world(spot.origin, spot.yaw, 0, .3F, baptist ? -6.5F : -5.5F), spot.yaw, world::BuildingPurpose::Church);
        ++(baptist ? kit.report.baptist_churches : kit.report.orthodox_churches);
        return true;
    };
    std::vector<std::pair<std::string, Vector3>> towns;
    for (const auto& town : region.settlements())
        if (!town.id.starts_with("hamlet_") && !town.id.starts_with("ranch_")) towns.emplace_back(town.id, town.center);
    for (const auto& [id, centre] : towns) {
        if (id == "capital_verda") {
            build({-376, 0, -20}, 0, 140, true);       // the west boulevard
            build({330, 0, 0}, 0, 200, false);         // the east suburbs
            continue;
        }
        if (!build(centre, 20, 150, centre.x < 0, true)) build(centre, 20, 300, centre.x < 0, true);
    }
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
        if (capital) continue;
        // The town's clinic: 12 x 10 m with a 2 m canopy in front (model bounds x -6..6, z -7..5),
        // its ambulance along the kerb with a clear run to pull out.
        const auto ambulance_out = [&](const CivicSpot& at) {
            const Vector3 from = to_world(at.origin, at.yaw, -3, 0, -10.2F), to = to_world(at.origin, at.yaw, 12, 0, -10.2F);
            for (const auto& town : region.settlements()) for (const auto& asset : town.assets) {
                if (!asset.collision || !asset.vehicle.definition.empty()) continue;
                const float dx = to.x - from.x, dz = to.z - from.z, l2 = dx * dx + dz * dz;
                const float t = std::clamp(((asset.position.x - from.x) * dx + (asset.position.z - from.z) * dz) / l2, 0.0F, 1.0F);
                if (std::hypot(asset.position.x - from.x - dx * t, asset.position.z - from.z - dz * t) < 1.6F + std::max(asset.size.x, asset.size.z) * .5F) return false;
            }
            return true;
        };
        CivicSpot clinic;
        if (!civic_lot(kit, region, centre, 15, 260, 0, -1, 7, 7, 7, 5, clinic, ambulance_out)) continue;
        if (!kit.put(clinic_model, clinic.origin, clinic.yaw)) continue;
        kit.purpose(to_world(clinic.origin, clinic.yaw, 0, .35F, -3.8F), clinic.yaw, world::BuildingPurpose::Clinic);
        vehicle_marker(*settlement(region, id), "vehicle_spawn_ambulance_" + id + "_1", to_world(clinic.origin, clinic.yaw, 2, 0, -10.2F), clinic.yaw + 90);
        ++kit.report.ambulances;
        ++kit.report.clinics;
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
// pasture, a corral with a loading chute behind it on the farm's own dirt loop, a corn field and a wheat field, and the
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
        // The cattle track: a dirt loop round the back of the farm past the chute, out onto the
        // highway again beyond the pasture.
        for (std::size_t k = 0; k + 1 < std::size(rr::cattle_track); ++k)
            ranch.roads.push_back({f.at(rr::cattle_track[k]), f.at(rr::cattle_track[k + 1]), 6, world::RoadType::Dirt});
        ranch.roads.push_back({f.at(rr::cattle_track[std::size(rr::cattle_track) - 1]),
                               highway_point(region, f.at(rr::cattle_track[std::size(rr::cattle_track) - 1])), 6, world::RoadType::Dirt});
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
        // Pasture and corral (open between them), the chute at the corral's back, by the cattle track.
        const rr::Frame load = rr::load_frame(f);
        const float chute_u = f.local(load.gate).x;
        fence_box(kit, f, rr::pasture, seed + 1, {{1, rr::lane.u0, rr::lane.u1}});
        fence_box(kit, f, rr::corral, seed + 2, {{0, rr::lane.u0, rr::lane.u1}, {1, chute_u - 2, chute_u + 2}});
        kit.put(ranch_dir + "loading_chute.glb", load.at(rr::chute.x, rr::chute.y + 2.2F), load.yaw() + 180);
        kit.put(ranch_dir + "feed_bunk.glb", f.at(-48, 40), yaw + 90);
        kit.put(ranch_dir + "water_trough.glb", f.at(-40, 46), yaw);
        kit.put(ranch_dir + "water_trough.glb", f.at(-44, 112), yaw + 90);
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
    namespace hw = world::highways;
    for (const auto& line : hw::lines()) {
        const auto n = line.points.size();
        float along = 0;
        for (std::size_t i = 0; i + (line.loop ? 0 : 1) < n; ++i) {
            Vector3 a = line.points[i], b = line.points[(i + 1) % n];
            const float l = std::hypot(b.x - a.x, b.z - a.z), middle = along + l * .5F;
            along += l;
            // Through Verda City it is the city's main avenue: a boulevard, narrower.
            const bool city = middle > line.city_from && middle < line.city_to;
            // A highway crossing the ring runs into the interchange's roundabout from either side:
            // cut back to the roundabout's lane.
            bool gone = false;
            if (!line.loop)
                for (const auto& x : hw::interchanges()) {
                    const auto inside = [&](Vector3 p) { return std::hypot(p.x - x.at.x, p.z - x.at.z) < hw::roundabout_radius; };
                    if (inside(a) && inside(b)) {gone = true; break;}
                    if (!inside(a) && !inside(b)) continue;
                    // The point on the lane between them.
                    Vector3& in = inside(a) ? a : b;
                    const Vector3 out = inside(a) ? b : a;
                    float lo = 0, hi = 1;
                    for (int k = 0; k < 30; ++k) {
                        const float mid = (lo + hi) * .5F;
                        (inside(Vector3Lerp(out, in, mid)) ? hi : lo) = mid;
                    }
                    in = Vector3Lerp(out, in, lo);
                }
            if (gone) continue;
            capital->roads.push_back({a, b, city ? hw::boulevard_width : hw::width, world::RoadType::Highway});
        }
    }
    // Where each road meets a roundabout's lane: a corner of the lane there, so the roads meet end to end.
    const auto legs = [&](const hw::Interchange& x) {
        std::vector<float> angles;
        const auto meets = [&](Vector3 p) {
            if (std::abs(std::hypot(p.x - x.at.x, p.z - x.at.z) - hw::roundabout_radius) < .05F) angles.push_back(std::atan2(p.z - x.at.z, p.x - x.at.x));
        };
        for (const auto& road : capital->roads) if (road.type == world::RoadType::Highway) {meets(road.start); meets(road.end);}
        for (const auto& slip : x.slips) {meets(slip.points.front()); meets(slip.points.back());}
        return angles;
    };
    // The interchanges: each roundabout's lane (anticlockwise on the map: traffic keeps right and
    // turns right into it and out of it), and the slip roads off the ring and back on.
    for (const auto& x : hw::interchanges()) {
        // The corners: every 7.5 degrees, and every leg (dropping regular corners within a metre and a half of one).
        constexpr int pieces = 48;
        const auto joins = legs(x);
        std::vector<float> corners(joins.begin(), joins.end());
        for (int k = 0; k < pieces; ++k) {
            const float a = std::remainder(2 * PI * static_cast<float>(k) / pieces, 2 * PI);
            if (std::none_of(joins.begin(), joins.end(), [&](float j) { return std::abs(std::remainder(a - j, 2 * PI)) * hw::roundabout_radius < 1.5F; }))
                corners.push_back(a);
        }
        // Anticlockwise on the map: the angle falling.
        std::sort(corners.begin(), corners.end(), std::greater<>());
        for (std::size_t k = 0; k < corners.size(); ++k) {
            const auto at = [&](float a) { return Vector3{x.at.x + std::cos(a) * hw::roundabout_radius, 0, x.at.z + std::sin(a) * hw::roundabout_radius}; };
            capital->roads.push_back({at(corners[k]), at(corners[(k + 1) % corners.size()]), hw::roundabout_width, world::RoadType::Roundabout});
        }
        for (const auto& slip : x.slips)
            for (std::size_t k = 0; k + 1 < slip.points.size(); ++k)
                capital->roads.push_back({slip.points[k], slip.points[k + 1], hw::slip_width, world::RoadType::Ramp});
    }
    // The ring's bar and grills: the slip roads off and on, and the park's aisle between them.
    for (const auto& rest : hw::rest_areas()) {
        for (const auto* slip : {&rest.off, &rest.on})
            for (std::size_t k = 0; k + 1 < slip->size(); ++k)
                capital->roads.push_back({(*slip)[k], (*slip)[k + 1], hw::slip_width, world::RoadType::Ramp});
        capital->roads.push_back({rest.aisle_from, rest.aisle_to, 10, world::RoadType::Asphalt});
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
    // (She comes and goes on her timetable - world::harbour - and the game draws her where she is.)
    if (!kit.put(city + "civic/cargo_ship.glb", {L::cargo_quay_x1 + 9.2F, L::sea_level - 5, z}, 0, false, false)) return;
    // The cranes: their legs on the rails either side of the quay's edge strip, clear of the lane;
    // solid (WorldCollision meets their real shape: the portal between the legs is open).
    for (const float cz : {z - 22, z})
        kit.put(city + "civic/harbour_crane.glb", {L::cargo_quay_x1 - 8.5F, quay, cz}, 0, false, true);
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
    // The arrivals (world::harbour): the ship's brow down to the quay (drawn while she is alongside),
    // the arrival hall, a zebra over the quay lane, and the waiting area where people meet the ship -
    // a pick-up bay, a shelter, benches, a lamp.
    {
        namespace hb = world::harbour;
        kit.put(city + "civic/ship_brow.glb", {hb::brow_foot.x + 4.2F, quay, hb::brow_foot.z}, 0, false, false);
        if (kit.put(city + "civic/arrival_hall.glb", {hb::hall.x, quay, hb::hall.z}, 0, false))
            kit.purpose({hb::hall.x, quay + .2F, hb::hall.z + 3}, 0, world::BuildingPurpose::Office);
        kit.put(city + "ground/crosswalk_14.glb", {hb::crossing.x, quay + .005F, hb::crossing.z}, 0, false, false);
        kit.put(city + "ground/pickup_bay.glb", {hb::pickup_bay.x, quay + .005F, hb::pickup_bay.z}, 0, false, false);
        kit.put(city + "civic/waiting_shelter.glb", {hb::waiting.x - 8, quay, hb::waiting.z - 3.4F}, 180, false);   // (its bench toward the bay)
        kit.put(urban + "Traffic lights/street_light_clean.glb", {hb::waiting.x + 4, quay, hb::waiting.z - 4}, 270, false);
        kit.tree({hb::waiting.x - 14, quay, hb::waiting.z - 4});
    }
    // The port's gate on the terminal road, short of the quay road's crossing: the name over the
    // road, the gatehouse beside it, the barrier up.
    kit.put(city + "civic/port_gantry.glb", {world::harbour::gate_x, world::terrain::TerrainHeight::sample(world::harbour::gate_x, z), z}, 90, false, true);
    // Lamps and bollards along the quay edge, end to end (clear of the jetty's foot and the hall).
    for (float lz = z - 50; lz < L::quay_z1 - 2; lz += 25)
        if (std::abs(lz - L::fish_z) > 6 && std::abs(lz - world::harbour::hall.z) > 12) kit.put(urban + "Traffic lights/street_light_clean.glb", {L::cargo_quay_x1 - 2, quay, lz}, 90, false);
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
        {"disko", site("port_luma"), 15, 320, false, false},
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
        // (Level ground, within half a metre: the forecourt is seated on its high side.)
        if (!civic_lot(kit, region, near, min_d, max_d, 0, 0, 12.4F, 11.4F, 11.2F, 3.5F, spot, accept, false, .5F)) return;
        spot.origin = seated(spot, 12, 11);
        if (!kit.put(M::gas_model, spot.origin, spot.yaw, false)) return;
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
        spot.origin = seated(spot, 9, 6.2F);
        if (!kit.put(M::mechanic_model, spot.origin, spot.yaw, false)) return;
        kit.purpose(to_world(spot.origin, spot.yaw, 6.6F, .1F, -4.8F), spot.yaw, world::BuildingPurpose::Garage);   // the office
        kit.report.mechanics.push_back(spot.origin);
    };
    // Aŭtovendejo Verda: model bounds x -18..18, z -13.6..13; the lot's front on the street.
    {
        CivicSpot spot;
        if (civic_lot(kit, region, {0, 0, 0}, 200, 620, 0, 0, 18.4F, 13.9F, 13.8F, 3.5F, spot, clear_of_downtown) &&
            (spot.origin = seated(spot, 18, 13.6F), kit.put(M::car_lot_model, spot.origin, spot.yaw, false))) {
            kit.purpose(to_world(spot.origin, spot.yaw, 12.8F, .12F, 7.2F), spot.yaw, world::BuildingPurpose::Office);   // the sales office
            kit.report.car_lot = true;
            kit.report.car_lot_at = spot.origin;
            // The cars on show, noses to the street: one of each the lot sells.
            auto* capital = settlement(region, "capital_verda");
            // (game::autos::dealer's order: the first five on the front row, the rest behind.)
            static constexpr std::array<const char*, 10> stock{"hatchback", "motorcycle", "sxs", "wagon", "sedan", "van", "truck", "flatbed", "muscle", "green_hand_gt"};
            for (std::size_t i = 0; i < stock.size() && capital; ++i) {
                const std::size_t row = i / M::car_lot_spots.size(), slot = i % M::car_lot_spots.size();
                vehicle_marker(*capital, std::string("vehicle_spawn_") + stock[i] + "_forsale_" + std::to_string(i + 1),
                               to_world(spot.origin, spot.yaw, M::car_lot_spots[slot], 0, M::car_lot_rows[row]), spot.yaw + 180);
            }
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
        // The yard: the stem in from the highway, the loop, a paved apron over the whole yard, the
        // drop point beyond the loop, a diesel lane inside it and containers.
        const auto build_yard = [&](Vector3 junction, Vector3 stem_end, Vector3 centre, Vector3 out) {
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
                    for (const float side : {-1.0F, 1.0F})
                    for (const float reach : {14.0F, 9.0F, 19.0F, 4.0F}) {
                        // Only on the yard's level ground (not overhanging the bank at its edge): the
                        // first spot along the yard that is.
                        const float s = side * reach;
                        const Vector3 at{shed.x + across.x * s, 0, shed.z + across.z * s};
                        float low = 1e9F, high = -1e9F;
                        // (The row is 37.5 m long and 10.5 m deep, its length along the model's z.)
                        const float a = (yaw + 90) * DEG2RAD;
                        const Vector3 length{std::sin(a), 0, std::cos(a)}, depth{std::cos(a), 0, -std::sin(a)};
                        for (const float u : {-18.5F, -9.0F, 0.0F, 9.0F, 18.5F})
                            for (const float v : {-5.0F, 0.0F, 5.0F}) {
                                const float h = world::terrain::TerrainHeight::sample(at.x + length.x * u + depth.x * v, at.z + length.z * u + depth.z * v);
                                low = std::min(low, h); high = std::max(high, h);
                            }
                        if (high - low > .6F) continue;
                        kit.put(city + "civic/container_stack.glb", {at.x, world::terrain::TerrainHeight::sample(at.x, at.z), at.z}, yaw + 90, false);
                        break;
                    }
                    kit.report.freight_yards.push_back(centre);
            const float apron = world::terrain::TerrainHeight::sample(centre.x, centre.z);
            kit.put(city + "ground/parking_lot_68.glb", {centre.x, apron, centre.z}, std::atan2(out.x, out.z) * RAD2DEG, false);
        };
        // Walk back along the highway, past where it narrows, to a stretch with room beside it -
        // or, for the four towns' yards, straight to its spot in the layout (on its level pad).
        const world::layout::FreightYard* pinned = nullptr;
        for (const auto& yard : world::layout::freight_yards) if (std::string_view(yard.town) == id) pinned = &yard;
        bool built = false;
        for (const float back : {210.0F, 250.0F, 290.0F, 330.0F, 380.0F})
            for (const float side : {1.0F, -1.0F}) {
                if (built) break;
                if (pinned) {
                    if (back != 210.0F || side != 1.0F) continue;
                    const Vector3 out = pinned->out, centre = pinned->centre;
                    const Vector3 stem_end{centre.x - out.x * ring, 0, centre.z - out.z * ring};
                    const Vector3 junction{stem_end.x - out.x * (world::highways::paved_half + stem), 0, stem_end.z - out.z * (world::highways::paved_half + stem)};
                    kit.claim(centre, ring + lane * .5F + 2, ring + lane * .5F + 2, std::atan2(out.x, out.z) * RAD2DEG);
                    build_yard(junction, stem_end, centre, out);
                    built = true;
                    continue;
                }
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
                build_yard(junction, stem_end, centre, out);
                built = true;
            }
    }
}

// Lit streets: every town street, lane and suburb road gets a street light at least every
// street_light_spacing metres, alternating sides, on the verge with its arm out over the road -
// wherever there isn't one already, and where nothing stands (a house, a tree, a wall, another road).
void street_lighting(Kit& kit, world::VerdaRegion& region) {
    const std::string light = "assets/verda/urban/Traffic lights/street_light.glb";
    std::vector<Vector3> lit;
    std::vector<world::Road> roads;
    for (const auto& town : region.settlements()) {
        for (const auto& a : town.assets)
            if (a.model_path.find("Traffic lights/street_light") != std::string::npos || a.model_path.find("lamppost") != std::string::npos) lit.push_back(a.position);
        for (const auto& r : town.roads) roads.push_back(r);
    }
    const auto clear_of_roads = [&](Vector3 p) {
        for (const auto& r : roads) {
            const float dx = r.end.x - r.start.x, dz = r.end.z - r.start.z, l2 = dx * dx + dz * dz;
            const float t = l2 > 0 ? std::clamp(((p.x - r.start.x) * dx + (p.z - r.start.z) * dz) / l2, 0.0F, 1.0F) : 0;
            if (std::hypot(p.x - r.start.x - dx * t, p.z - r.start.z - dz * t) < r.width * .5F + .9F) return false;
        }
        return true;
    };
    // Off the civic buildings' forecourts (stations, shops, the radio stations): no post in front of a door.
    std::vector<const world::WorldAsset*> civic;
    for (const auto& town : region.settlements())
        for (const auto& a : town.assets) if (a.model_path.find("/city/civic/") != std::string::npos) civic.push_back(&a);
    const auto clear_of_doors = [&](Vector3 p) {
        for (const auto* a : civic) {
            const float yaw = a->rotation_y * DEG2RAD, dx = p.x - a->position.x, dz = p.z - a->position.z;
            const float lx = dx * std::cos(yaw) - dz * std::sin(yaw), lz = dx * std::sin(yaw) + dz * std::cos(yaw);
            if (std::abs(lx) < a->size.x * .5F + 3 && std::abs(lz) < a->size.z * .5F + 3) return false;
        }
        return true;
    };
    std::vector<world::Road> streets;
    for (const auto& town : region.settlements()) {
        if (town.id.starts_with("ranch_")) continue;
        for (const auto& r : town.roads)
            if (r.type != world::RoadType::Highway && r.type != world::RoadType::Dirt && r.width >= 5) streets.push_back(r);
    }
    for (const auto& road : streets) {
        const float length = std::hypot(road.end.x - road.start.x, road.end.z - road.start.z);
        if (length < 10) continue;
        const Vector3 dir{(road.end.x - road.start.x) / length, 0, (road.end.z - road.start.z) / length}, normal{dir.z, 0, -dir.x};
        int k = 0;
        for (float along = std::min(8.0F, length * .5F); along < length - 3; along += street_light_spacing * .5F, ++k) {
            const float side = (k + static_cast<int>(std::lround(road.start.x + road.start.z))) % 2 ? 1.0F : -1.0F;
            const Vector3 p{road.start.x + dir.x * along + normal.x * side * (road.width * .5F + 1.3F), 0,
                            road.start.z + dir.z * along + normal.z * side * (road.width * .5F + 1.3F)};
            if (world::terrain::TerrainHeight::sample(p.x, p.z) <= world::layout::sea_level + .5F) continue;
            // Only on level ground: not on the bank beside a slip road or a cutting, half buried or
            // standing out over the drop.
            {
                float low = 1e9F, high = -1e9F;
                for (const float u : {-1.6F, 0.0F, 1.6F})
                    for (const float v : {-1.6F, 0.0F, 1.6F}) {
                        const float h = world::terrain::TerrainHeight::sample(p.x + u, p.z + v);
                        low = std::min(low, h); high = std::max(high, h);
                    }
                if (high - low > .6F) continue;
            }
            if (std::any_of(lit.begin(), lit.end(), [&](Vector3 q) { return std::hypot(q.x - p.x, q.z - p.z) < street_light_spacing * .5F; })) continue;
            if (!clear_of_roads(p) || !clear_of_doors(p) || world::physics::WorldCollision::blocked({p.x, world::terrain::TerrainHeight::sample(p.x, p.z), p.z}, region, 1.0F)) continue;
            // The arm (the model's +x) reaches out over the road.
            const float yaw = std::atan2(normal.z * side, -normal.x * side) * RAD2DEG;
            if (kit.put(light, p, yaw)) {lit.push_back(p); ++kit.report.street_lights;}
        }
    }
}

// The ring's bar and grills, "La Ringo": the bar and grill (a pub), the car and truck park, a diesel
// lane for the rigs, and trees round the pad.
Lot roadhouse_plot(const world::highways::RestArea& rest) {
    return {rest.centre.x - 75, rest.centre.z - 75, rest.centre.x + 75, rest.centre.z + 75};
}
void ring_roadhouses(Kit& kit, world::VerdaRegion& region) {
    for (const auto& rest : world::highways::rest_areas()) {
        kit.release(roadhouse_plot(rest));
        // The car park and, behind it, the truck park (18 drive-in bays for rigs): paving on the
        // level pad, sunk so it stands 2 cm proud of it (the land is drawn lower under it, so no
        // grass flickers through: TerrainChunk), and nothing to collide with - wheels and feet run
        // on the ground (a solid slab shoved parked rigs about and jack-knifed them).
        const auto paving = [](Vector3 p) { return Vector3{p.x, world::terrain::TerrainHeight::sample(p.x, p.z) - .13F, p.z}; };
        kit.put(city + "ground/parking_lot_68.glb", paving(rest.lot), rest.lot_yaw, false, false);
        kit.put(city + "ground/truck_park.glb", paving(rest.truck_park), rest.truck_park_yaw, false, false);
        modular_building(kit, rest.building, rest.building_yaw, 8, 4, rest.inside ? "brick" : "stuco", 1,
                         rest.inside ? 9101U : 9102U, world::BuildingPurpose::Pub);
        kit.put(city + "civic/diesel_island.glb", rest.diesel, rest.diesel_yaw);
        const Vector3 out{rest.centre.x / std::hypot(rest.centre.x, rest.centre.z) * (rest.inside ? -1.0F : 1.0F), 0,
                          rest.centre.z / std::hypot(rest.centre.x, rest.centre.z) * (rest.inside ? -1.0F : 1.0F)};
        const Vector3 along{-out.z, 0, out.x};
        const auto at = [&](float a, float lateral) {   // `lateral` out from the ring's centre line
            return Vector3{rest.centre.x + along.x * a + out.x * (lateral - 84), 0, rest.centre.z + along.z * a + out.z * (lateral - 84)};
        };
        for (const float a : {-40.0F, -20.0F, 0.0F, 20.0F, 40.0F}) kit.tree(at(a, 142));
        for (const float a : {-58.0F, 58.0F})
            for (const float lateral : {98.0F, 120.0F}) kit.tree(at(a, lateral));
        // Rigs parked in the bays, nose out to the aisle: the brotherhood's home is full of them.
        world::Settlement* near = nullptr;
        float best = 1e9F;
        for (auto& town : region.runtime_settlements()) {
            const float d = std::hypot(town.center.x - rest.centre.x, town.center.z - rest.centre.z);
            if (d < best) { best = d; near = &town; }
        }
        if (!near) continue;
        const int rigs = rest.brotherhood ? 7 : 3;
        // (Bay 17, the last, is kept for a new brother's own rig.)
        static constexpr std::array<int, 7> bays{1, 3, 4, 7, 10, 12, 15};
        for (int k = 0; k < rigs; ++k)
            vehicle_marker(*near, "vehicle_spawn_semi_laringo_" + std::string(rest.name) + "_" + std::to_string(k + 1),
                           rest.bay(bays[static_cast<std::size_t>(k)]), rest.bay_yaw());
    }
}


// ------------------------------------------------------------------ Sovaĝa Monto's road
// Sovaĝejo's dirt road carries on through the trailer park's north gate, over the shoulder of Sovaĝa
// Monto and down to the ring road (world/MountainRoad.hpp lays it out and shapes the land to it).
// Each pullout has a gravel lay-by, a row of parking bays behind square bollards, a trailer home
// and a street light; the fork leads to Supra Sovaĝejo, six trailers on a bench above the sea -
// still Sovaĝejo's people.
namespace mtn = world::mountain;
// The road's samples thinned to pieces that stay within a few centimetres of the curve.
std::vector<Vector3> thin_line(const std::vector<Vector3>& points, float tolerance, float longest) {
    std::vector<bool> keep(points.size(), false);
    keep.front() = keep.back() = true;
    std::function<void(std::size_t, std::size_t)> split = [&](std::size_t a, std::size_t b) {
        if (b <= a + 1) return;
        float worst = 0;
        std::size_t at = a;
        const Vector3 p = points[a], q = points[b];
        const float dx = q.x - p.x, dz = q.z - p.z, l = std::max(1e-4F, std::hypot(dx, dz));
        for (std::size_t i = a + 1; i < b; ++i) {
            const float d = std::abs((points[i].x - p.x) * dz - (points[i].z - p.z) * dx) / l;
            if (d > worst) {worst = d; at = i;}
        }
        if (worst > tolerance || l > longest) {
            if (worst <= tolerance) at = (a + b) / 2;
            keep[at] = true;
            split(a, at);
            split(at, b);
        }
    };
    split(0, points.size() - 1);
    std::vector<Vector3> out;
    for (std::size_t i = 0; i < points.size(); ++i) if (keep[i]) out.push_back({points[i].x, 0, points[i].z});
    return out;
}
float yaw_toward(Vector3 d) { return std::atan2(d.x, d.z) * RAD2DEG; }   // a model's +z (its front) along d
const std::array<std::string, 6> trailer_models{"trailer_01", "trailer_02", "trailer_03", "trailer_04", "trailer_05", "trailer_06"};
// A trailer home, `toward` the way its front (porch and steps) faces: a HOME just inside the door,
// a car on the pad beside it, yard junk behind it, an Apelacio flag on a pole (Signage).
bool mountain_trailer(Kit& kit, world::Settlement& town, Vector3 centre, Vector3 toward, std::uint32_t seed, const std::string& car_id, bool car) {
    const auto r = hash(seed);
    const float yaw = yaw_toward(toward) + static_cast<float>(static_cast<int>(r % 7) - 3);
    if (!kit.claim(centre, 3.4F, 7.8F, yaw)) return false;
    if (!kit.put(trailers + trailer_models[r % trailer_models.size()] + ".glb", centre, yaw)) return false;
    auto inside = to_world(centre, yaw, .25F, 0, .56F);
    inside.y = world::terrain::TerrainHeight::sample(centre.x, centre.z) + .85F;
    kit.purpose(inside, 90 + yaw, world::BuildingPurpose::Home);
    if (car) vehicle_marker(town, car_id, to_world(centre, yaw, (r & 8) ? 6.6F : -6.6F, 0, 4.0F), yaw + ((r & 16) ? 180.0F : 0.0F));
    prop(kit, shacks + ((r % 3) == 0 ? "Pallet.glb" : "Barrel.glb"), to_world(centre, yaw, 2.2F, 0, -9.0F), static_cast<float>(r % 360), .8F);
    return true;
}
void sovagejo_mountain(Kit& kit, world::VerdaRegion& region) {
    world::Settlement* town = sovagejo(region);
    if (!town) return;
    // The trailer park's north fence opens onto the road (the four pieces across the lane go).
    for (auto& s : region.runtime_settlements())
        kit.report.cleared += static_cast<int>(std::erase_if(s.assets, [](const world::WorldAsset& a) {
            return a.model_path.ends_with("Chainlink Fence.glb") && std::abs(a.position.z - 38) < .6F && std::abs(a.position.x - sovagejo_road_x()) < 4.9F;
        }));
    // The road, and on to the ring road's centre line (where traffic joins it).
    const auto& line = mtn::road();
    const auto pieces = thin_line(line.points, .2F, 24);
    const std::size_t before = town->roads.size();
    for (std::size_t i = 0; i + 1 < pieces.size(); ++i)
        town->roads.push_back({pieces[i], pieces[i + 1], mtn::road_width, world::RoadType::Dirt});
    const Vector3 end = pieces.back();
    town->roads.push_back({end, highway_point(region, end), mtn::road_width, world::RoadType::Dirt});
    // The pullouts' lay-bys: off the road, along the bench, back on.
    for (const auto& p : mtn::pullouts()) {
        const auto on = [&](float a) { const Vector3 q = line.at(p.s + a); return Vector3{q.x, 0, q.z}; };
        const Vector3 a0 = on(-30), a1 = p.at(-18, 9), a2 = p.at(18, 9), a3 = on(30);
        town->roads.push_back({a0, {a1.x, 0, a1.z}, mtn::layby_width, world::RoadType::Gravel});
        town->roads.push_back({{a1.x, 0, a1.z}, {a2.x, 0, a2.z}, mtn::layby_width, world::RoadType::Gravel});
        town->roads.push_back({{a2.x, 0, a2.z}, a3, mtn::layby_width, world::RoadType::Gravel});
    }
    // Supra Sovaĝejo's lane: from the fork, through the pad, to the fire pit.
    const auto& up = mtn::hamlet();
    const Vector3 lane_end = up.at(26, 0);
    town->roads.push_back({{up.fork.x, 0, up.fork.z}, {up.centre.x, 0, up.centre.z}, mtn::lane_width, world::RoadType::Gravel});
    town->roads.push_back({{up.centre.x, 0, up.centre.z}, {lane_end.x, 0, lane_end.z}, mtn::lane_width, world::RoadType::Gravel});
    kit.add_roads(std::vector<world::Road>(town->roads.begin() + static_cast<long>(before), town->roads.end()));
    kit.report.mountain_roads += static_cast<int>(town->roads.size() - before);
    const std::string light = urban + "Traffic lights/street_light.glb";
    const std::string bollard = urban + "Cones & Barriers/Bollards/square_bollard.glb";
    int homes = 0;
    // Each pullout: parking bays behind bollards, the trailer, a light over the lay-by.
    for (std::size_t k = 0; k < mtn::pullouts().size(); ++k) {
        const auto& p = mtn::pullouts()[k];
        const auto r = hash(1300U + static_cast<std::uint32_t>(k));
        const float out_yaw = yaw_toward(p.out);
        for (int bay = 0; bay < 4; ++bay) {
            const float a = -16 + bay * 3.0F;
            if ((r >> bay) % 3 != 0)
                vehicle_marker(*town, "vehicle_spawn_hatchback_monto_" + std::to_string(k + 1) + "_" + std::to_string(bay + 1), p.at(a, 14.6F), out_yaw);
        }
        for (int post = 0; post < 5; ++post) kit.put(bollard, p.at(-17.5F + post * 3.0F, 17.4F), out_yaw);
        if (mountain_trailer(kit, *town, p.at(9, 17.5F), Vector3Scale(p.along, (r & 1) ? 1.0F : -1.0F), 1400U + static_cast<std::uint32_t>(k),
                             "vehicle_spawn_truck_monto_" + std::to_string(k + 1), false)) {++homes; ++kit.report.pullout_trailers;}
        kit.put(light, p.at(-1, 12.8F), yaw_toward(Vector3Scale(p.out, -1)));
        if (p.lookout) kit.put(light, p.at(19, 12.8F), yaw_toward(Vector3Scale(p.out, -1)));
    }
    // Supra Sovaĝejo: three trailers each side of the lane, fronts to it, a car beside most; the
    // fire pit and benches at the lane's end; lights along it; shade trees behind the lots.
    const Vector3 left{-up.lane.z, 0, up.lane.x};
    int upper = 0;
    for (int side = 0; side < 2; ++side) for (int i = 0; i < 3; ++i) {
        const float lateral = side == 0 ? 13.5F : -13.5F, a = -16 + i * 13.0F;
        const Vector3 toward = Vector3Scale(left, side == 0 ? -1.0F : 1.0F);
        if (mountain_trailer(kit, *town, up.at(a, lateral), toward, 1500U + static_cast<std::uint32_t>(side * 3 + i),
                             "vehicle_spawn_hatchback_supra_sovagejo_" + std::to_string(side * 3 + i + 1), (i + side) % 3 != 2)) {
            ++upper; ++kit.report.upper_trailers;
            if ((i + side) % 2 == 0) kit.tree(up.at(a - 2, lateral + (side == 0 ? 11.0F : -11.0F)));
        }
    }
    homes += upper;
    prop(kit, "assets/verda/survival/runtime/bonfire.glb", up.at(30, 0), 0, 1.2F);
    for (const auto& [a, l, yaw] : std::initializer_list<std::tuple<float, float, float>>{{30, 4.2F, 0}, {30, -4.2F, 180}, {34.2F, 0, 90}})
        prop(kit, urban + "Bus stops/busstop_bench.glb", up.at(a, l), yaw_toward(up.lane) + yaw - 90, 1.6F);
    for (const float a : {-30.0F, -4.0F, 22.0F}) kit.put(light, up.at(a, 4.2F), yaw_toward(Vector3Scale(left, -1)));
    // Each trailer brings its people (about three in the hamlet, two at a pullout).
    town->survivors += upper * 3 + (homes - upper) * 2;
}

// ------------------------------------------------------------------ Sovaĝejo's country
// The county round Sovaĝejo (world/SovagejoCounty.hpp lays it out and levels the ground): short
// dirt tracks off the mountain road and the homestead road to scattered homes - trailers, cabins,
// farmhouses, a family compound, a workshop family - each with its yard: old cars and parts donors,
// sheds, generators, woodpiles, a deer stand, boats, a mailbox; and Fajrejo, a tiny settlement on
// its gravel lot: a church, a general store, a mechanic's shed and four houses. Everyone here is
// Sovaĝejo's (and life counts them so: world::county::near).
namespace cty = world::county;
const std::string country = city + "country/";
void sovagejo_county(Kit& kit, world::VerdaRegion& region) {
    world::Settlement* town = sovagejo(region);
    if (!town) return;
    // The tracks (thinned to pieces close to their curves), and Fajrejo's gravel lot.
    const std::size_t before = town->roads.size();
    for (const auto& track : cty::tracks()) {
        const auto pieces = thin_line(track.points, .15F, 20);
        for (std::size_t i = 0; i + 1 < pieces.size(); ++i)
            town->roads.push_back({pieces[i], pieces[i + 1], cty::track_width, world::RoadType::Dirt});
    }
    const auto& v = cty::village();
    town->roads.push_back({{v.west.x, 0, v.west.z}, {v.east.x, 0, v.east.z}, cty::lot_width, world::RoadType::Gravel});
    kit.add_roads(std::vector<world::Road>(town->roads.begin() + static_cast<long>(before), town->roads.end()));
    kit.report.county_roads += static_cast<int>(town->roads.size() - before);
    // A model facing `front` (its own front is -Z), standing on the pad at `at`.
    const auto facing = [](Vector3 front) { return yaw_toward(front) + 180; };
    const auto building = [&](const std::string& model, Vector3 at, Vector3 front, float half_x, float half_z) {
        const float yaw = facing(front);
        if (!kit.claim(at, half_x, half_z, yaw)) return false;
        return kit.put(country + model + ".glb", at, yaw);
    };
    // A home's purpose marker just inside its door (the model's front wall at local z = -wall).
    const auto home = [&](Vector3 at, Vector3 front, float wall, float floor, world::BuildingPurpose purpose = world::BuildingPurpose::Home) {
        const float yaw = facing(front);
        auto inside = to_world(at, yaw, 0, 0, -wall + 1.3F);
        inside.y = at.y + floor + .1F;
        kit.purpose(inside, yaw, purpose);
    };
    int cars = 0, donors = 0, dwellings = 0;
    const auto car = [&](const char* kind, Vector3 at, float yaw, bool donor) {
        vehicle_marker(*town, std::string("vehicle_spawn_") + kind + (donor ? "_donor_" : "_") + "kamparo_" + std::to_string(++cars), at, yaw);
        donors += donor;
    };
    // (Yard things go where they're put: each spot is chosen clear of its home's own footprint.)
    const auto small = [&](const char* model, Vector3 at, float yaw, float = 0) { kit.put(country + model + ".glb", at, yaw); };
    const auto shack = [&](const char* model, Vector3 at, float yaw, float = 0) { kit.put(shacks + model, at, yaw); };
    const std::vector<std::string> rails{urban + "Fences/low_wooden_fence/wooden_fence_closed.glb",
        urban + "Fences/low_wooden_fence/wooden_fence_closed.glb", urban + "Fences/low_wooden_fence/wooden_fence_broken.glb",
        urban + "Fences/low_wooden_fence/wooden_fence_open.glb"};
    for (std::size_t k = 0; k < cty::sites().size(); ++k) {
        const auto& site = cty::sites()[k];
        const auto at = [&](float right, float forward) { return site.at(right, forward); };
        const float yaw = yaw_toward(site.front);          // +Z toward the track (a trailer's porch)
        const auto seed = 2200U + static_cast<std::uint32_t>(k) * 17U;
        // The mailbox by the track, the family's name on it (Signage).
        small("mailbox", at(6.5F, site.flat - 1.0F), yaw + 180, .5F);
        switch (site.kind) {
            case cty::Home::RoadsideTrailer:
                if (mountain_trailer(kit, *town, at(0, 0), site.front, seed, "", false)) ++dwellings;
                car("truck", at(-7.5F, 1), yaw + 180, false);
                car("hatchback", at(7.5F, -5), yaw + 75, true);
                small("generator", at(5.5F, -5.5F), yaw + 90, .8F);
                small("woodpile", at(-6.5F, -7.5F), yaw, 2.2F);
                shack("Barrel.glb", at(4.6F, 3.5F), 20, .6F);
                break;
            case cty::Home::HuntingCabin:
                if (building("log_cabin_dark", at(0, -1), site.front, 4.6F, 4.9F)) {home(at(0, -1), site.front, 3, .3F); ++dwellings;}
                car("truck", at(-6.5F, 3), yaw + 180, false);
                small("woodpile", at(6.2F, -2), yaw + 90, 2.2F);
                small("outhouse", at(-6, -7), yaw, 1.0F);
                prop(kit, "assets/verda/survival/runtime/Shelter_tent.glb", at(6, -8), yaw, 1.8F);
                kit.put(country + "deer_stand.glb", at(-20, -30), yaw + 30);   // (out in the trees, on the land)
                kit.tree(at(-14, -27)); kit.tree(at(-24, -36)); kit.tree(at(-26, -24));
                break;
            case cty::Home::Compound: {
                // Two trailers facing each other across the yard, the workshop at its head, the fire
                // pit between; a rail fence round it, open to the track.
                const Vector3 right{-site.front.z, 0, site.front.x};
                if (mountain_trailer(kit, *town, at(-13, -2), right, seed, "", false)) ++dwellings;
                if (mountain_trailer(kit, *town, at(13, -3), Vector3Scale(right, -1), seed + 1, "", false)) ++dwellings;
                if (building("workshop_shed", at(0, -17), site.front, 6.6F, 5.6F)) home(at(0, -17), site.front, 5, .1F, world::BuildingPurpose::Garage);
                prop(kit, "assets/verda/survival/runtime/bonfire.glb", at(0, 1), 0, 1.2F);
                car("truck", at(-4, 9), yaw + 180, false);
                car("hatchback", at(5, 9), yaw + 170, false);
                car("hatchback", at(-17, -16), yaw + 15, true);
                car("truck", at(16, -16), yaw - 20, true);
                car("hatchback", at(17, 4), yaw + 95, true);
                small("generator", at(-5, -8), yaw, .8F);
                small("woodpile", at(0, -9), yaw, 2.2F);
                small("outhouse", at(-19, -8), yaw + 90, 1.0F);
                shack("Leanto A.glb", at(19.5F, -12), yaw - 90, 2.2F);
                for (const float r : {-1.0F, 1.0F}) shack("Barrel.glb", at(-16.5F + r * .8F, -9.5F), static_cast<float>(seed % 90), .6F);
                fence(kit, at(-21, -21), at(21, -21), rails, 2.2F, seed);
                fence(kit, at(-21, -21), at(-21, 12), rails, 2.2F, seed + 1);
                fence(kit, at(21, -21), at(21, 12), rails, 2.2F, seed + 2);
                break;
            }
            case cty::Home::CreekFarm:
                if (building("farmhouse_cream", at(0, -2), site.front, 5.3F, 6.0F)) {home(at(0, -2), site.front, 4, .4F); ++dwellings;}
                car("truck", at(-8, 3), yaw + 180, false);
                car("hatchback", at(10, -6), yaw + 60, true);
                shack("Leanto B.glb", at(9, -9), yaw + 180, 2.2F);
                for (int i = 0; i < 3; ++i) kit.put("assets/verda/ranch/hay_bale.glb", at(-9.0F + i * 1.7F, -9), yaw + 90);
                kit.put("assets/verda/ranch/water_trough.glb", at(-11, -3), yaw + 90);
                small("outhouse", at(5, -11), yaw, 1.0F);
                break;
            case cty::Home::CreekTrailer:
                if (mountain_trailer(kit, *town, at(0, 0), site.front, seed, "", false)) ++dwellings;
                shack("Shack A.glb", at(8, -5), yaw + 180, 2.4F);
                small("boat_trailer", at(-7, 2), yaw + 180, 3.0F);
                car("truck", at(-7, -6), yaw + 180, false);
                car("hatchback", at(9, 5), yaw + 40, true);
                small("generator", at(4.5F, -6), yaw, .8F);
                break;
            case cty::Home::WestFarm:
                if (building("farmhouse_blue", at(0, -2), site.front, 5.3F, 6.0F)) {home(at(0, -2), site.front, 4, .4F); ++dwellings;}
                kit.put("assets/verda/ranch/windmill.glb", at(-11, -9), yaw);
                car("sxs", at(8, 2), yaw + 180, false);
                car("truck", at(-8, 3), yaw + 170, true);
                small("woodpile", at(8, -8), yaw, 2.2F);
                for (int i = 0; i < 2; ++i) kit.put("assets/verda/ranch/hay_bale.glb", at(11.0F, -2.0F - i * 1.8F), yaw);
                break;
            case cty::Home::FisherTrailer:
                if (mountain_trailer(kit, *town, at(0, 0), site.front, seed, "", false)) ++dwellings;
                small("boat_trailer", at(7, 1), yaw + 180, 3.0F);
                small("boat_trailer", at(-7, -6), yaw + 160, 3.0F);
                car("truck", at(-7, 3), yaw + 180, false);
                prop(kit, "assets/verda/survival/runtime/bonfire.glb", at(4, -8), 0, 1.2F);
                small("generator", at(-4.5F, -7), yaw, .8F);
                shack("Barrel.glb", at(5.5F, -4), 0, .6F);
                break;
            case cty::Home::Workshop:
                if (building("farmhouse_grey", at(-8, -3), site.front, 5.3F, 6.0F)) {home(at(-8, -3), site.front, 4, .4F); ++dwellings;}
                if (mountain_trailer(kit, *town, at(10, -6), site.front, seed, "", false)) ++dwellings;   // the grandmother's
                if (building("workshop_shed", at(-2, -17.5F), site.front, 6.6F, 5.6F)) home(at(-2, -17.5F), site.front, 5, .1F, world::BuildingPurpose::Garage);
                car("truck", at(-1, 6), yaw + 180, false);
                for (int i = 0; i < 3; ++i) car(i == 1 ? "truck" : "hatchback", at(14, 10 - 6.5F * static_cast<float>(i)), yaw + 90 + static_cast<float>(i) * 9, true);
                small("woodpile", at(-16, -10), yaw + 90, 2.2F);
                small("generator", at(6.5F, -13), yaw, .8F);
                break;
        }
        kit.tree(at(site.flat + 6, -site.flat * .6F));
        kit.tree(at(-site.flat - 5, -site.flat * .3F));
    }
    // Fajrejo: the church across the lot from the store, the mechanic's shed beside it, four
    // houses round about; the lot between them is the meeting place (cars, a bench, the flag).
    const Vector3 south = v.across, north = Vector3Scale(v.across, -1), west = Vector3Scale(v.along, -1);
    {
        const Vector3 at = v.at(-6, -22);
        const float yaw = facing(south);
        if (kit.claim(at, 5.9F, 10.6F, yaw) && kit.put(baptist_model, at, yaw)) {
            kit.purpose(to_world({at.x, v.centre.y, at.z}, yaw, 0, .3F, -6.5F), yaw, world::BuildingPurpose::Church);
            ++kit.report.village_buildings;
            ++kit.report.baptist_churches;   // (the west's faith)
        }
    }
    if (building("general_store", v.at(-4, 18.5F), north, 5.6F, 8.4F)) {home(v.at(-4, 18.5F), north, 6.5F, .3F, world::BuildingPurpose::Shop); ++kit.report.village_buildings;}
    if (building("mechanic_shed", v.at(15, -18), south, 6.6F, 5.6F)) {home(v.at(15, -18), south, 5, .1F, world::BuildingPurpose::Garage); ++kit.report.village_buildings;}
    struct House { const char* model; float a, b; Vector3 front; float wall, floor; bool trailer; };
    const House houses[]{{"log_cabin", -24, 26, north, 3, .3F, false}, {"farmhouse_grey", 17, 27, north, 4, .4F, false},
                         {"", 32, 14, west, 0, 0, true}, {"log_cabin", 10, -42, south, 3, .3F, false}};
    for (std::size_t i = 0; i < std::size(houses); ++i) {
        const auto& h = houses[i];
        const Vector3 at = v.at(h.a, h.b);
        bool built = false;
        if (h.trailer) built = mountain_trailer(kit, *town, at, h.front, 2400U + static_cast<std::uint32_t>(i), "", false);
        else if ((built = building(h.model, at, h.front, h.model[0] == 'f' ? 5.3F : 4.6F, h.model[0] == 'f' ? 6.0F : 4.9F))) home(at, h.front, h.wall, h.floor);
        if (built) {++kit.report.village_buildings; ++kit.report.village_homes; ++dwellings;}
        const Vector3 drive = Vector3Add(at, Vector3Scale(h.trailer ? v.across : Vector3{-h.front.z, 0, h.front.x}, 7.5F));
        car(i == 2 ? "hatchback" : "truck", drive, yaw_toward(h.front), false);
    }
    // The lot: pickups nosed in at the store, the side-by-side by the shed, a donor behind it.
    car("truck", v.at(-10, 5), yaw_toward(south), false);
    car("truck", v.at(-4, 5), yaw_toward(south), false);
    car("sxs", v.at(10, -5), yaw_toward(north), false);
    car("hatchback", v.at(24, -24), yaw_toward(v.along) + 20, true);
    small("generator", v.at(-11, 28), 0, .8F);
    small("woodpile", v.at(-18, -30), yaw_toward(v.along), 2.2F);
    small("outhouse", v.at(4, -33), yaw_toward(south) + 180, 1.0F);
    prop(kit, urban + "Bus stops/busstop_bench.glb", v.at(-12, 9.4F), yaw_toward(south) + 180, 1.6F);
    prop(kit, urban + "Bus stops/busstop_bench.glb", v.at(4, 9.4F), yaw_toward(south) + 180, 1.6F);
    kit.put(urban + "Traffic lights/street_light.glb", v.at(0, -9.6F), yaw_toward(south));
    kit.put(urban + "Traffic lights/street_light.glb", v.at(-20, 9.6F), yaw_toward(north));
    for (const float a : {-36.0F, -16.0F, 28.0F, 40.0F}) kit.tree(v.at(a, a < 0 ? -34.0F : 36.0F));
    // Each home brings its people; the store, the shed and the church their work.
    town->survivors += dwellings * 3;
    kit.report.county_homes += dwellings;
    kit.report.county_donors += donors;
}

WorldKitReport build_verda_towns(world::VerdaRegion& region, const creator::CreatorAssetRegistry& catalog) {
    wipe_downtown(region);
    lay_highways(region);
    Kit kit(region, catalog);
    // The ring's bar and grills (built last, below): their ground kept free from the start.
    for (const auto& rest : world::highways::rest_areas())
        kit.reserve(roadhouse_plot(rest));
    // The ballparks' plots (Verda City's is the stadium ground, kept in government()).
    for (const auto& park : world::ballpark::parks)
        if (std::hypot(park.centre.x - stadium_reserve.x, park.centre.z - stadium_reserve.z) > 1) kit.reserve(ballpark_plot(park.centre));
    // The interchanges' roundabouts: nothing built on their islands.
    for (const auto& x : world::highways::interchanges()) {
        const float r = world::highways::roundabout_pad;
        kit.reserve({x.at.x - r, x.at.z - r, x.at.x + r, x.at.z + r});
    }
    downtown(kit, region);
    downtown_south(kit, region);
    north_quarter(kit, region);
    sky_tower(kit, region);
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
    churches(kit, region);
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
    // Last, round everything built: street lights wherever a town's street is dark.
    street_lighting(kit, region);
    // After the lights (new pieces only): the ring's bar and grills.
    ring_roadhouses(kit, region);
    // After those (new pieces only): Sovaĝa Monto's road, its pullouts and Supra Sovaĝejo.
    sovagejo_mountain(kit, region);
    sovagejo_county(kit, region);
    return kit.report;
}
}
