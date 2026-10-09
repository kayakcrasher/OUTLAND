#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

using namespace outland;
using namespace characters;
int main() {
    const auto scratch=std::filesystem::temp_directory_path()/(
        "outland_characters_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(scratch);
    CharacterRegistry registry;
    std::string error;
    const std::string manifest=std::string(OUTLAND_SOURCE_DIR)+"/assets/verda/characters/character_manifest.tsv";
    assert(registry.load(manifest,error) && error.empty());
    assert(registry.assets().size()==82);
    assert(registry.pool(CharacterPool::Civilian).size()==45);
    assert(registry.pool(CharacterPool::Emergency).size()==20);
    assert(registry.pool(CharacterPool::Hostile).size()==9);
    assert(registry.pool(CharacterPool::Creature).size()==6);
    assert(registry.pool(CharacterPool::Arms).empty());
    assert(registry.player()->id=="character_01");
    assert(registry.player()->z_up && registry.player()->facing_degrees==-90);
    for (const auto& asset:registry.assets()) {
        assert(std::filesystem::exists(std::filesystem::path(OUTLAND_SOURCE_DIR)/asset.model_path));
        assert(registry.find(asset.id)==&asset);
        assert(registry.find_model(asset.model_path)==&asset);
        assert(std::filesystem::path(asset.model_path).extension()==".glb");
    }
    const auto* arms=registry.find("arms_arms_rig");
    assert(arms && arms->source_model.ends_with(".glb") && arms->role=="first_person");
    assert(!registry.find("no such character"));
    assert(registry.choose(CharacterPool::Hostile,123)->id==registry.choose(CharacterPool::Hostile,123)->id);
    assert(npc_pool_for_marker("creator_marker_npc_spawn_1")==CharacterPool::Civilian);
    assert(npc_pool_for_marker("creator_marker_npc_spawn_emergency_7")==CharacterPool::Emergency);
    assert(npc_pool_for_marker("npc_spawn_hostile_test")==CharacterPool::Hostile);
    assert(npc_pool_for_marker("npc_spawn_creature_1")==CharacterPool::Creature);
    assert(npc_pool_for_marker("npc_spawn_hostileish")==CharacterPool::Civilian);
    const auto broken=scratch/"broken.tsv";
    { std::ofstream out(broken); out<<"id\tname\tcategory\trole\tmodel\ttexture\tsource\r\n"
        "bad\tBad\tcivilian\tnpc\tassets/verda/characters/../../bad.glb\t\ttest\r\n"; }
    assert(!registry.load(broken.string(),error) && !error.empty());
    assert(registry.assets().size()==82 && registry.player()->id=="character_01"); // Transactional failure.
    { std::ofstream out(broken); out<<"id\tname\tcategory\trole\tmodel\ttexture\tsource\n"
        "same\tOne\tcivilian\tnpc\tassets/verda/characters/one.glb\t\ttest\n"
        "same\tTwo\tcivilian\tnpc\tassets/verda/characters/two.glb\t\ttest\n"; }
    assert(!registry.load(broken.string(),error));
    assert(!registry.load((scratch/"missing.tsv").string(),error));
    // Loading, repeated loading, edited positions/rotations, disable/delete and
    // non-NPC markers are tested through the unchanged V3 save reader in both builds.
    const auto map=scratch/"world.map";
    auto write=[&](bool changed) {
        std::ofstream out(map); out<<"OUTLAND_CREATOR_MAP 3\n";
        if (!changed) out<<"MARKER \"creator_marker_npc_spawn_1\" 2 10 0 20 .8 1.8 .8 90 1\n";
        out<<"MARKER \"creator_marker_npc_spawn_emergency_1\" 2 "<<(changed ? "40 2 50" : "12 0 22")<<" .8 1.8 .8 "<<(changed ? 270 : 0)<<" 1\n"
            "MARKER \"creator_marker_npc_spawn_hostile_1\" 2 14 0 24 .8 1.8 .8 180 "<<(changed ? 0 : 1)<<"\n"
            "MARKER \"creator_marker_npc_spawn_creature_1\" 2 16 0 26 .8 1.8 .8 0 1\n"
            "MARKER \"creator_marker_npc_spawn_disabled\" 2 18 0 28 .8 1.8 .8 0 0\n"
            "MARKER \"creator_marker_loot_spawn_1\" 0 18 0 28 .8 1.8 .8 0 1\n"
            "MARKER \"creator_marker_zombie_spawn_1\" 1 18 0 28 .8 1.8 .8 0 1\n";
    };
    world::VerdaRegion region;
    NpcSystem npcs;
    write(false);
    assert(creator::CreatorMapIO::load(region,map.string()));
    npcs.reconcile(region,registry);
    assert(npcs.actors().size()==4);
    const auto first=npcs.actors();
    assert(first[0].position.x==10 && first[0].yaw_degrees==90);
    assert(registry.find(first[0].character_id)->pool==CharacterPool::Civilian);
    assert(registry.find(first[1].character_id)->pool==CharacterPool::Emergency);
    assert(registry.find(first[2].character_id)->pool==CharacterPool::Hostile);
    assert(registry.find(first[3].character_id)->pool==CharacterPool::Creature);
    assert(creator::CreatorMapIO::load(region,map.string()));
    npcs.reconcile(region,registry);
    assert(npcs.actors().size()==4);
    for (std::size_t i=0;i<4;++i) assert(npcs.actors()[i].character_id==first[i].character_id);
    write(true);
    assert(creator::CreatorMapIO::load(region,map.string()));
    npcs.reconcile(region,registry);
    assert(npcs.actors().size()==2);
    assert(npcs.actors()[0].character_id==first[1].character_id);
    assert(npcs.actors()[0].position.x==40 && npcs.actors()[0].position.y==2 && npcs.actors()[0].yaw_degrees==270);
#ifdef OUTLAND_DEV_TOOLS
    // Equal marker IDs in different settlements are distinct; duplicate IDs in
    // one settlement never spawn twice. Invalid editor coordinates are ignored.
    auto& settlements=region.editable_settlements();
    auto second=settlements.front();
    second.id += "/other";
    settlements.push_back(std::move(second));
    settlements.front().gameplay_markers.push_back(settlements.front().gameplay_markers.front());
    auto invalid=settlements.front().gameplay_markers.front();
    invalid.id="creator_marker_npc_spawn_invalid";
    invalid.position.x=std::numeric_limits<float>::quiet_NaN();
    settlements.front().gameplay_markers.push_back(invalid);
    npcs.reconcile(region,registry);
    assert(npcs.actors().size()==4);
    assert(npcs.actors()[0].spawn_key!=npcs.actors()[2].spawn_key);
#endif
    CharacterRegistry empty;
    npcs.reconcile(region,empty);
    assert(npcs.actors().empty());
    std::filesystem::remove_all(scratch);
    std::cout<<"[PASS] Production manifest, body pools, transactional parsing, stable NPC spawns and V3 reconciliation\n";
}
