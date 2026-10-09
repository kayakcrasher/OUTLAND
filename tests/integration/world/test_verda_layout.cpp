#include "outland/creator/CreatorMapIO.hpp"
#include "outland/dev/DevLab.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/terrain/TerrainWorld.hpp"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
using namespace outland;
namespace {
int created = 0, unloaded = 0, drawn = 0;
int pressed_key = 0;
std::set<std::pair<int, int>> meshes;
} // namespace
// Exercise real terrain generation/cache ownership without a graphics context.
extern "C" bool __wrap_IsKeyPressed(int key) { return key == pressed_key; }
extern "C" void __wrap_UploadMesh(Mesh *, bool) {}
extern "C" Model __wrap_LoadModelFromMesh(Mesh mesh) {
    ++created;
    meshes.insert({static_cast<int>(std::floor(mesh.vertices[0] / 128)),
                   static_cast<int>(std::floor(mesh.vertices[2] / 128))});
    Model model{};
    model.meshCount = 1;
    model.meshes = static_cast<Mesh *>(MemAlloc(sizeof(Mesh)));
    *model.meshes = mesh;
    model.materialCount = 1;
    model.materials = static_cast<Material *>(std::calloc(1, sizeof(Material)));
    model.materials[0].maps =
        static_cast<MaterialMap *>(std::calloc(MATERIAL_MAP_BRDF + 1, sizeof(MaterialMap)));
    return model;
}
extern "C" void __wrap_UnloadModel(Model model) {
    ++unloaded;
    const auto &mesh = model.meshes[0];
    meshes.erase({static_cast<int>(std::floor(mesh.vertices[0] / 128)),
                  static_cast<int>(std::floor(mesh.vertices[2] / 128))});
    MemFree(mesh.vertices);
    MemFree(mesh.normals);
    MemFree(mesh.texcoords);
    MemFree(model.meshes);
    std::free(model.materials[0].maps);
    std::free(model.materials);
}
extern "C" void __wrap_DrawModel(Model, Vector3, float, Color) { ++drawn; }
extern "C" void __wrap_DrawPlane(Vector3, Vector2, Color) {}
int main() {
    namespace fs = std::filesystem;
    using world::terrain::TerrainHeight;
    world::VerdaRegion legacy(false);
    const auto original = legacy.settlements().front();
    assert(original.center.z == -70 && legacy.settlements().size() == 1);
    const float old_height = TerrainHeight::sample(110, 90);
    world::VerdaRegion region;
    assert(region.coastal_layout() && region.settlements().size() == 5);
    std::set<std::string> ids;
    for (const auto &town : region.settlements()) {
        assert(ids.insert(town.id).second && !town.buildings.empty() && !town.roads.empty());
        const auto *site = world::layout::find(town.id);
        assert(site && town.name == site->name);
        assert(town.center.x == site->center.x && town.center.z == site->center.z);
        if (town.id != "capital_verda") {
            const float radius = std::hypot(town.center.x, town.center.z);
            assert(radius > 1700 &&
                   radius < world::layout::shore_radius(town.center.x, town.center.z) - 160);
        }
        for (const auto &b : town.buildings)
            assert(std::hypot(b.position.x - town.center.x, b.position.z - town.center.z) < 150);
        for (const auto &other : region.settlements())
            if (other.id != town.id)
                assert(std::hypot(other.center.x - town.center.x, other.center.z - town.center.z) >
                       1700);
    }
    const auto &moved = region.settlements().front();
    assert(moved.buildings.size() == original.buildings.size() &&
           moved.assets.size() == original.assets.size());
    for (std::size_t i = 0; i < moved.buildings.size(); ++i) {
        assert(moved.buildings[i].id == original.buildings[i].id);
        assert(moved.buildings[i].position.x == original.buildings[i].position.x);
        assert(moved.buildings[i].position.z == original.buildings[i].position.z - 1730);
        assert(moved.buildings[i].size.x == original.buildings[i].size.x);
    }
    for (std::size_t i = 0; i < moved.assets.size(); ++i)
        assert(moved.assets[i].id == original.assets[i].id &&
               moved.assets[i].position.z == original.assets[i].position.z - 1730);
    for (std::size_t i = 0; i < moved.roads.size(); ++i)
        assert(moved.roads[i].start.z == original.roads[i].start.z - 1730 &&
               moved.roads[i].width == original.roads[i].width);
    for (float x : {-120.0F, 0.0F, 120.0F})
        for (float z : {-120.0F, 0.0F, 120.0F})
            assert(TerrainHeight::sample(x, z) == 0);
    // Every coastal hub has a road chain to the fixed capital, with no ocean segments.
    std::set<std::pair<float, float>> reachable{{0, 0}};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto &s : region.settlements())
            for (const auto &road : s.roads) {
                const auto a = std::pair{road.start.x, road.start.z},
                           b = std::pair{road.end.x, road.end.z};
                if (reachable.contains(a))
                    changed = reachable.insert(b).second || changed;
                if (reachable.contains(b))
                    changed = reachable.insert(a).second || changed;
                for (int step = 0; step <= 10; ++step) {
                    const float t = step * .1F, x = road.start.x + (road.end.x - road.start.x) * t,
                                z = road.start.z + (road.end.z - road.start.z) * t;
                    if (std::hypot(x, z) > world::layout::shore_radius(x, z) - 160)
                        assert(TerrainHeight::sample(x, z) > world::layout::sea_level);
                }
            }
    }
    for (const auto &s : region.settlements())
        assert(reachable.contains({s.center.x, s.center.z}));
    // Coast shaping joins natural land continuously, including low valleys.
    for(float angle:{0.0F,1.0F,2.0F,3.0F}) {
        const float x=std::cos(angle),z=std::sin(angle);
        const float edge=world::layout::shore_radius(x,z)-world::layout::shoreline_band;
        assert(std::abs(TerrainHeight::sample(x*(edge-.01F),z*(edge-.01F))-
                        TerrainHeight::sample(x*(edge+.01F),z*(edge+.01F)))<.1F);
    }
    assert(TerrainHeight::sample(2700, 0) == world::layout::sea_level-8);
    assert(world::physics::WorldCollision::blocked({2700, 0, 0}, region, .45F));
    assert(!world::physics::WorldCollision::blocked({2700, 0, 0}, legacy, .45F));
    dev::DevLab lab;
    assert(lab.spawn_position(region).x == 0 && lab.spawn_position(region).z == 8);
    pressed_key = KEY_TWO;
    lab.update();
    pressed_key = 0;
    assert(lab.spawn_position(region).z == -1790);
    pressed_key = KEY_EIGHT;
    lab.update();
    pressed_key = 0;
    assert(lab.spawn_position(region).x == 1750 && lab.spawn_position(region).z == -390);
    pressed_key = KEY_TWO;
    lab.update();
    pressed_key = 0;
    assert(lab.spawn_position(legacy).z == -60);
    TerrainHeight::set_coastal_layout(true);
    {
        world::terrain::TerrainWorld terrain;
        assert(terrain.chunk_count() == 49 && created == 49);
        terrain.update({127, 0, 0});
        assert(created == 49);
        terrain.update({128, 0, 0});
        assert(created == 56 && unloaded == 7 && meshes.size() == 49);
        terrain.update({-1, 0, -1});
        assert(terrain.chunk_count() == 49 && meshes.contains({-4, -4}) && meshes.contains({2, 2}));
        for (const auto &site : world::layout::sites) {
            terrain.update(site.center);
            assert(terrain.chunk_count() == 49 && meshes.size() == 49);
            assert(meshes.contains({static_cast<int>(std::floor(site.center.x / 128)),
                                    static_cast<int>(std::floor(site.center.z / 128))}));
        }
        const int before_mode = created;
        TerrainHeight::set_coastal_layout(false);
        terrain.update(world::layout::sites.back().center);
        assert(created == before_mode + 49 && meshes.size() == 49);
        TerrainHeight::set_coastal_layout(true);
        terrain.update(world::layout::sites.back().center);
        assert(created == before_mode + 98 && meshes.size() == 49);
        const int count = created;
        terrain.update({std::numeric_limits<float>::infinity(), 0, 0});
        assert(created == count);
        terrain.draw();
        assert(drawn == 49);
    }
    assert(created == unloaded && meshes.empty());
    const auto path = fs::temp_directory_path() / "outland-coastal-layout-test.map";
#ifdef OUTLAND_DEV_TOOLS
    auto &authored = region.editable_settlements().front();
    authored.buildings.front().position.x += 7;
    authored.assets.front().rotation_y = 37;
    authored.gameplay_markers.push_back({"creator_marker_npc_spawn_civilian_test",
                                         world::GameplayMarkerType::NpcSpawn,
                                         {7, 2, -1810},
                                         {1, 1, 1},
                                         45,
                                         true});
    assert(creator::CreatorMapIO::save(region, path.string()));
    world::VerdaRegion restored(false);
    assert(creator::CreatorMapIO::load(restored, path.string()) && restored.coastal_layout() &&
           TerrainHeight::coastal_layout());
    assert(restored.settlements().front().buildings.front().position.x ==
           authored.buildings.front().position.x);
    assert(restored.settlements().front().assets.front().rotation_y == 37);
    assert(restored.settlements().front().gameplay_markers.back().position.y == 2);
    assert(restored.settlements()[1].roads.size() == region.settlements()[1].roads.size());
#endif
    {
        std::ofstream out(path);
        out << "OUTLAND_CREATOR_MAP 4\nSETTLEMENT \"capital_custom\" \"Saved capital\" 80 2 90 1 4 "
               "5 6\n";
    }
    assert(creator::CreatorMapIO::load(region, path.string()) && !region.coastal_layout() &&
           !TerrainHeight::coastal_layout());
    assert(region.settlements().front().center.x == 80 &&
           TerrainHeight::sample(110, 90) == old_height);
    {
        std::ofstream out(path);
        out << "OUTLAND_CREATOR_MAP 5\nGEOGRAPHY 9\n";
    }
    assert(!creator::CreatorMapIO::load(region, path.string()) &&
           !TerrainHeight::coastal_layout() && region.settlements().front().center.x == 80);
    {
        std::ofstream out(path);
        out << "OUTLAND_CREATOR_MAP 5\nGEOGRAPHY 1\nSETTLEMENT \"capital_verda\" \"Verda\" 0 0 0 0 "
               "0 0 0\n";
    }
    assert(creator::CreatorMapIO::load(region, path.string()) && region.coastal_layout() &&
           TerrainHeight::sample(110, 90) == 0);
    fs::remove(path);
    std::cout << "[PASS] Coastal layout, intact Espera, connected land roads, flat capital, "
                 "bounded terrain streaming and legacy/V5 persistence\n";
}
