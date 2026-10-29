#include "outland/engine/render/StaticWorld.hpp"
#include "outland/engine/render/ViewFrustum.hpp"
#include "outland/engine/render/HighwayRender.hpp"
#include "outland/world/roads/HighwayNetwork.hpp"
#include "outland/world/RegionGeometry.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>

namespace outland::engine {
namespace {
constexpr float tree_cell = 48;
constexpr int road_chunk = 64; // 2 m segments: 128 m of road
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
}

void StaticWorld::invalidate() {
    buildings_.clear(); trees_.clear(); roads_.clear();
    indexed_ = false;
}

void StaticWorld::index(const world::VerdaRegion& region) {
    invalidate();
    const auto& towns = region.settlements();
    std::map<std::pair<int, int>, std::size_t> cells;
    for (std::size_t s = 0; s < towns.size(); ++s) {
        const auto& town = towns[s];
        for (std::size_t b = 0; b < town.buildings.size(); ++b) {
            const auto& value = town.buildings[b];
            const float ground = world::terrain::TerrainHeight::sample(value.position.x, value.position.z);
            buildings_.push_back({s, b, {value.position.x, ground + value.size.y * .5F, value.position.z}, Vector3Length(value.size), {}, false});
        }
        for (const auto& asset : town.assets) {
            if (asset.type != world::AssetType::Tree || !asset.model_path.empty() || !asset.vehicle.definition.empty()) continue;
            const auto key = std::make_pair(static_cast<int>(std::floor(asset.position.x / tree_cell)), static_cast<int>(std::floor(asset.position.z / tree_cell)));
            auto found = cells.find(key);
            if (found == cells.end()) {found = cells.emplace(key, trees_.size()).first; trees_.emplace_back();}
            trees_[found->second].positions.push_back(asset.position);
        }
        for (std::size_t r = 0; r < town.roads.size(); ++r) {
            const auto& road = town.roads[r];
            const int segments = world::geometry::road_segments(road);
            for (int begin = 0; begin < segments; begin += road_chunk) {
                const int end = std::min(segments, begin + road_chunk);
                const Vector3 a = Vector3Lerp(road.start, road.end, static_cast<float>(begin) / segments);
                const Vector3 b = Vector3Lerp(road.start, road.end, static_cast<float>(end) / segments);
                Vector3 centre = Vector3Lerp(a, b, .5F);
                centre.y = world::terrain::TerrainHeight::sample(centre.x, centre.z);
                roads_.push_back({s, r, begin, end, centre, flat(a, b) * .5F + road.width + 4, nullptr, false});
            }
        }
    }
    for (auto& cell : trees_) {
        Vector3 sum{};
        for (const auto& p : cell.positions) sum = Vector3Add(sum, p);
        cell.centre = Vector3Scale(sum, 1.0F / static_cast<float>(cell.positions.size()));
        cell.centre.y = world::terrain::TerrainHeight::sample(cell.centre.x, cell.centre.z) + 5;
    }
    indexed_ = true;
}

void StaticWorld::draw(const world::VerdaRegion& region, Vector3 camera, const ViewFrustum* view, float reach, float road_lift) {
    reach = std::clamp(reach, 1.0F, 3.0F);
    if (!indexed_) index(region);
    draws_ = 0;
    // A few new recordings a frame, so arriving somewhere new never stalls; the rest are drawn
    // directly until their turn.
    int budget = 6;
    const auto& towns = region.settlements();
    const auto visible = [&](Vector3 centre, float radius) { return !view || view->sphere(centre, radius); };
    // Roads: the same 340 m reach as before.
    const bool lifted = road_lift > 0;
    if (lifted) { rlDrawRenderBatchActive(); rlPushMatrix(); rlTranslatef(0, road_lift, 0); }
    const auto& highways = world::highways::network(region);
    for (auto& chunk : roads_) {
        if (chunk.settlement >= towns.size() || chunk.index >= towns[chunk.settlement].roads.size()) continue;
        if (flat(chunk.centre, camera) > 340 * reach + chunk.radius || !visible(chunk.centre, chunk.radius)) continue;
        const auto& road = towns[chunk.settlement].roads[chunk.index];
        const auto* highway = highways.info(chunk.settlement, chunk.index);
        const auto draw_road = [&] {
            if (highway) highway::draw(road, *highway, chunk.begin, chunk.end);
            else if (road.type == world::RoadType::Ramp || road.type == world::RoadType::Roundabout) highway::draw_link(road, chunk.begin, chunk.end);
            else world::geometry::road(road, chunk.begin, chunk.end);
        };
        // Never record while lifted: rlgl bakes the current transform into the vertices, and the
        // lift would stay in the road mesh for good (roads floating metres up after the drop).
        if (chunk.failed || (!chunk.mesh && (budget <= 0 || lifted))) {draw_road(); continue;}
        if (!chunk.mesh) {
            --budget;
            chunk.mesh = std::make_unique<RecordedGeometry>();
            chunk.failed = !recorder_.record(draw_road, *chunk.mesh);
            continue; // recording drew it this frame
        }
        chunk.mesh->draw(); ++draws_;
    }
    if (lifted) { rlDrawRenderBatchActive(); rlPopMatrix(); }
    // Houses within 320 m; full detail within 90 m.
    for (auto& house : buildings_) {
        if (house.settlement >= towns.size() || house.index >= towns[house.settlement].buildings.size()) continue;
        const float distance = flat(house.centre, camera);
        if (distance > 320 * reach || !visible(house.centre, house.radius)) continue;
        const auto& value = towns[house.settlement].buildings[house.index];
        const bool detailed = distance < 90;
        if (detailed) world::geometry::building_door(value);   // live: doors swing
        auto& lod = house.lods[detailed ? 0 : 1];
        if (house.failed || (!lod && budget <= 0)) {world::geometry::building(value, detailed); continue;}
        if (!lod) {
            --budget;
            lod = std::make_unique<RecordedGeometry>();
            house.failed = !recorder_.record([&] {world::geometry::building(value, detailed);}, *lod);
            continue;
        }
        lod->draw(); ++draws_;
    }
    // Street trees by 48 m patch: within 260 m, three levels of detail by the patch's distance.
    for (auto& cell : trees_) {
        const float distance = flat(cell.centre, camera);
        if (distance > 260 * reach + tree_cell || !visible(cell.centre, tree_cell)) continue;
        const int lod = distance < 65 ? 0 : distance < 140 ? 1 : 2;
        auto& mesh = cell.lods[static_cast<std::size_t>(lod)];
        if (cell.failed || (!mesh && budget <= 0)) {for (const auto& p : cell.positions) world::geometry::tree(p, lod); continue;}
        if (!mesh) {
            --budget;
            mesh = std::make_unique<RecordedGeometry>();
            cell.failed = !recorder_.record([&] {for (const auto& p : cell.positions) world::geometry::tree(p, lod);}, *mesh);
            continue;
        }
        mesh->draw(); ++draws_;
    }
}
}
