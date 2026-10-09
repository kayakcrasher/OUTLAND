#include "outland/creator/CreatorController.hpp"
#include "outland/creator/CreatorSession.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <raymath.h>
using namespace outland;
int main() {
    namespace fs = std::filesystem;
    creator::CreatorController controller;
    controller.set_enabled(true);
    const auto &catalog = controller.registry().assets();
    std::size_t industrial = 0, vehicle = 0;
    const auto path = (fs::temp_directory_path() / "outland_vehicle_creator.map").string();
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        const auto &entry = catalog[i];
        bool wanted =
            entry.id.starts_with("industrial_") ||
            (entry.category == creator::CreatorAssetCategory::Vehicle && !entry.model_path.empty());
        if (!wanted)
            continue;
        if (entry.id.starts_with("industrial_"))
            ++industrial;
        else
            ++vehicle;
        assert(fs::is_regular_file(fs::path(OUTLAND_SOURCE_DIR) / entry.model_path));
        assert(entry.placement.placeable);
        assert(controller.select_asset(i));
        world::VerdaRegion region;
        for (auto &site : region.editable_settlements()) {
            site.assets.clear();
            site.roads.clear();
            site.buildings.clear();
            site.gameplay_markers.clear();
        }
        controller.update(region, {25, 4, 25}, {0, 0, 1}, false);
        assert(controller.preview().valid);
        creator::CreatorSession session(path);
        assert(session.edit(region, [&] { return controller.place_selected(region); }));
        const auto &asset = region.settlements().front().assets.back();
        const auto placed = asset.position;
        const auto id = asset.id;
        assert(asset.model_path == entry.model_path);
        assert(controller.select_target(
            region, {placed.x, placed.y + asset.size.y * .5F, placed.z - 20}, {0, 0, 1}));
        assert(controller.selection().world_asset_id == id);
        assert(session.edit(region, [&] { return controller.rotate_selected(region, 45); }));
        controller.update(region, {50, 4, 25}, {0, 0, 1}, false);
        assert(session.edit(region, [&] { return controller.move_selected(region); }));
        assert(session.edit(region, [&] { return controller.duplicate_selected(region); }));
        assert(region.settlements().front().assets.size() == 2);
        assert(session.undo(region));
        assert(region.settlements().front().assets.size() == 1);
        assert(session.redo(region));
        assert(region.settlements().front().assets.size() == 2);
        assert(session.save(region));
        world::VerdaRegion restored;
        assert(session.load(restored));
        assert(restored.settlements().front().assets.size() == 2);
        assert(restored.settlements().front().assets.back().rotation_y == 45);
        if ((entry.category == creator::CreatorAssetCategory::Vehicle &&
             !entry.model_path.empty())) {
            game::vehicles::VehicleRegistry registry;
            std::string error;
            assert(registry.load(std::string(OUTLAND_SOURCE_DIR) +
                                     "/assets/verda/vehicles/vehicle_manifest.tsv",
                                 error));
            game::vehicles::VehicleSystem cars(registry);
            cars.reconcile(restored);
            assert(cars.vehicles().size() == 2);
            for (const auto &v : cars.vehicles())
                assert(registry.find(cars.asset(restored, v.id)->vehicle.definition));
        }
        const auto other = restored.settlements().front().assets.front().position;
        assert(
            controller.select_target(restored, {other.x, other.y + .1F, other.z - 20}, {0, 0, 1}));
        assert(session.edit(restored, [&] { return controller.delete_selected(restored); }));
        assert(restored.settlements().front().assets.size() == 1);
    }
    assert(industrial == 13 && vehicle >= 1);
    fs::remove(path);
    // A rotated solid model uses its whole footprint, not the old point-sized prop collider.
    world::VerdaRegion region;
    for (auto &site : region.editable_settlements()) {
        site.assets.clear();
        site.roads.clear();
        site.buildings.clear();
        site.gameplay_markers.clear();
    }
    world::WorldAsset shack;
    shack.id = "creator_shack";
    shack.position = {25, 0, 25};
    shack.size = {10, 3, 2};
    shack.rotation_y = 90;
    shack.model_path = "industrial";
    region.editable_settlements().front().assets.push_back(shack);
    assert(world::physics::WorldCollision::blocked({25, 0, 29}, region, .45F));
    assert(!world::physics::WorldCollision::blocked({29, 0, 25}, region, .45F));
    std::cout << "[PASS] All 13 industrial assets and hatchback "
                 "place/select/move/rotate/duplicate/delete/undo/redo/save/load, with rotated "
                 "object collision\n";
}
