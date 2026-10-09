#include "outland/dev/VerdaWorldKit.hpp"
#include "outland/creator/CreatorAssetRegistry.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
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
        asset.size = {definition.footprint.width, definition.footprint.height, definition.footprint.depth};
        asset.rotation_y = yaw;
        asset.vehicle.home = asset.position; asset.vehicle.home_yaw = yaw;
        asset.collision = definition.category != creator::CreatorAssetCategory::Road &&
            (definition.footprint.height > .25F || definition.category == creator::CreatorAssetCategory::BuildingPart);
        if (!region_.place_world_asset(std::move(asset))) return false;
        ++report.assets;
        return true;
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

void capital(Kit& kit) {
    const auto c = site("capital_verda");
    // Central quarter: enterable two-storey blocks around the main crossing.
    modular_building(kit, {c.x + 40, 0, c.z + 40}, 0, 8, 4, "brick", 2, 11);
    modular_building(kit, {c.x - 40, 0, c.z + 40}, 0, 4, 4, "fancy_brick", 2, 12);
    modular_building(kit, {c.x + 40, 0, c.z - 40}, 180, 4, 4, "stuco", 1, 13);
    modular_building(kit, {c.x - 40, 0, c.z - 42}, 180, 8, 4, "fancy_brick", 2, 14);
    modular_building(kit, {c.x + 72, 0, c.z + 32}, 90, 4, 4, "brick", 1, 15);
    // Skyline: textured prebuilt apartment blocks (hollow facades) as solid cover and landmarks.
    // The Downtown pack's towers are left out until its missing textures are restored.
    for (const auto& [x, z, name, yaw] : std::initializer_list<std::tuple<float, float, const char*, float>>{
            {-78, 85, "flat_Lshape_brick", 0}, {78, 85, "flat_square_brick", 0}, {-78, -85, "flat_square_concrete", 180}, {78, -88, "flat_Lshape_large_alt", 180}})
        if (kit.claim({c.x + x, 0, c.z + z}, 11.5F, 11.5F, yaw)) kit.put(urban + "Buildings Prebuild/" + name + ".glb", {c.x + x, 0, c.z + z}, yaw);
    // Street furniture along the avenue and at the crossing.
    for (int i = -6; i <= 6; ++i) if (i != 0) {
        kit.put(urban + "Traffic lights/street_light.glb", {c.x + 6.5F, 0, c.z + i * 22.0F}, 90);
        kit.put(urban + "Traffic lights/street_light.glb", {c.x - 6.5F, 0, c.z + i * 22.0F + 11}, -90);
    }
    for (const auto& [x, z, yaw] : std::initializer_list<std::tuple<float, float, float>>{{6, 6, 0}, {-6, 6, 90}, {6, -6, -90}, {-6, -6, 180}})
        kit.put(urban + "Traffic lights/traffic_light.glb", {c.x + x, 0, c.z + z}, yaw);
    prop(kit, urban + "Bus stops/busstop_clean_bench.glb", {c.x - 9, 0, c.z + 40}, -90, 2.5F);
    prop(kit, urban + "Bus stops/busstop_graffiti.glb", {c.x - 9, 0, c.z - 30}, -90, 2.5F);
    // A police checkpoint on the north approach: barriers and cones are BR cover.
    for (int i = 0; i < 4; ++i) kit.put(urban + "Cones & Barriers/Concrete Barriers/Jersey_barrier.glb", {c.x - 3.6F + i * 2.5F * (i < 2 ? 1 : 1.2F), 0, c.z - 120 - (i % 2) * 3}, 0);
    for (int i = 0; i < 6; ++i) kit.put(urban + "Cones & Barriers/Cones/Traffic_cone_round.glb", {c.x - 3 + i * 1.2F, 0, c.z - 115}, 0);
    for (int i = 0; i < 8; ++i) kit.put(urban + "Cones & Barriers/Bollards/simple_bollard.glb", {c.x + 12 + i * 2.0F, 0, c.z + 12}, 0);
    for (const auto& [x, z] : std::initializer_list<std::pair<float, float>>{{18, 24}, {-18, 24}, {18, -24}, {-18, -24}})
        kit.put(urban + "Litter/TrashCan/trash_can.glb", {c.x + x, 0, c.z + z}, 0);
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
    Kit kit(region, catalog);
    capital(kit);
    roka(kit);
    porto_luma(kit);
    suda_haveno(kit);
    espera(kit);
    return kit.report;
}
}
