#include "outland/creator/CreatorMapIO.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    const auto file=std::filesystem::temp_directory_path()/"outland_runtime_assets_test.map";
    {
        std::ofstream output(file);
        output << "OUTLAND_CREATOR_MAP 3\nASSET \"creator_downtown_test_1\" 0 "
            "\"assets/verda/creator/downtown/buildings/Building_Small_1.gltf\" "
            "10 0 20 12.46 17.03 14.54 90 255 255 255 255 80 80 80 255 1\n"
            "ASSET \"creator_urban_test_1\" 4 "
            "\"assets/verda/urban/Walls & Fences/plackard_wall/plackard_slab_full.glb\" "
            "12 0 20 4 3 1 0 255 255 255 255 80 80 80 255 1\n";
    }
    outland::world::VerdaRegion region;
    const auto initial=region.settlements().front().assets.size();
    assert(outland::creator::CreatorMapIO::load(region,file.string()));
    const auto& assets=region.settlements().front().assets;
    assert(assets.size()==initial+2);
    assert(assets[initial].id=="creator_downtown_test_1");
    assert(assets[initial].model_path.ends_with("Building_Small_1.gltf"));
    assert(assets[initial].rotation_y==90);
    assert(assets.back().model_path.ends_with("Walls & Fences/plackard_wall/plackard_slab_full.glb"));
    assert(outland::creator::CreatorMapIO::load(region,file.string()));
    assert(region.settlements().front().assets.size()==initial+2);
    {
        std::ofstream output(file);
        output << "OUTLAND_CREATOR_MAP 4\nSETTLEMENT \"authored\" \"My Verda\" 10 0 20 1 2 3 4\n"
            "BUILDING \"base_house\" 0 100 2 200 8 4 8 90 210 195 165 255 90 70 55 255 1\n"
            "ROAD 0 0 0 30 0 10 5 2\n"
            "MARKER \"creator_marker_npc_spawn_hostile_test\" 2 20 0 30 1 1.8 1 0 1\n"
            "SETTLEMENT \"empty_after_delete\" \"Cleared area\" 0 0 0 0 0 0 0\n";
    }
    assert(outland::creator::CreatorMapIO::load(region,file.string()));
    assert(region.settlements().size()==2 && region.settlements()[0].name=="My Verda");
    assert(region.settlements()[0].buildings.size()==1 && region.settlements()[0].buildings[0].position.y==2);
    assert(region.settlements()[0].roads.size()==1 && region.settlements()[0].gameplay_markers.size()==1);
    assert(region.settlements()[1].assets.empty() && region.settlements()[1].buildings.empty());
    assert(outland::creator::CreatorMapIO::load(region,file.string()));assert(region.settlements()[0].roads.size()==1);
    {std::ofstream output(file);output<<"OUTLAND_CREATOR_MAP 4\nROAD 0 0 0 1 0 1 1 0\n";}
    assert(!outland::creator::CreatorMapIO::load(region,file.string()) && region.settlements()[0].name=="My Verda");
    std::filesystem::remove(file);
    std::cout << "[PASS] Saved imported model world loads idempotently in gameplay builds\n";
}
