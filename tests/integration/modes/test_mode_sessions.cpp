#include "outland/game/ModeSession.hpp"
#include "outland/game/inventory/LootSession.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/VerdaLayout.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace outland;
using namespace outland::game;
int main() {
    namespace fs=std::filesystem;
    const fs::path root=OUTLAND_SOURCE_DIR;
    const auto directory=fs::temp_directory_path()/("outland-modes-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(directory/"maps");
    inventory::ItemRegistry items;std::string error;
    assert(items.load((root/"assets/verda/survival/gameplay/items.tsv").string(),(root/"assets/verda/survival/gameplay/loot_tables.tsv").string(),error));
    world::VerdaRegion region;
    assert(creator::CreatorMapIO::load(region,(root/"maps/verda_world.map").string()));
    for(const auto level:{ai::BotLevel::Easy,ai::BotLevel::Medium,ai::BotLevel::Hard}) {
        ai::BattleRoyaleBots bots;Health health;ModeSession session;
        auto feet=session_spawn(region,GameMode::BattleRoyale,1337);
        assert(std::isfinite(feet.y) && feet.y>world::layout::sea_level+1);
        assert(!world::physics::WorldCollision::body_blocked(feet,feet.y,region,.4F));
        bots.start(region,level,23,123,feet);
        assert(bots.bots().size()==23);
        for(const auto& bot:bots.bots()) {
            assert(bot.alive() && bot.level()==level);
            assert(std::hypot(bot.position().x-feet.x,bot.position().z-feet.z)>145);
            assert(!world::physics::WorldCollision::body_blocked(bot.position(),bot.position().y,region,.35F));
        }
        session.start(GameMode::BattleRoyale,23,level);
        assert(std::string(session.objective())=="Be the last survivor.");
        session.update(.25F,feet,health,bots,true);assert(session.elapsed()==0);
        // Kill credit, final victory, immutable terminal result.
        for(const auto& bot:bots.bots()) assert(bots.damage_bot(bot.id(),1000,feet,0));
        session.update(.1F,feet,health,bots,false);
        assert(session.phase()==SessionPhase::Won && session.result().placement==1 && session.result().kills==23 && session.result().difficulty==level);
        health.damage(1000);session.update(.1F,feet,health,bots,false);
        assert(session.phase()==SessionPhase::Won && session.result().placement==1);
        // Full restart clears old eliminations and difficulty is selected afresh.
        bots.start(region,level,23,456,feet);health.reset();session.start(GameMode::BattleRoyale,23,level);
        assert(session.phase()==SessionPhase::Playing && session.result().kills==0 && bots.player_kills()==0);
        assert(bots.damage_bot(1,1000,feet,0));health.damage(1000);
        session.update(.1F,feet,health,bots,false);
        assert(session.phase()==SessionPhase::Dead && session.result().placement==23 && session.result().kills==1);
        bots.damage_bot(2,1000,feet,0);session.update(.1F,feet,health,bots,false);
        assert(session.result().placement==23 && session.result().kills==1);
        // Death is checked before victory on a mutual last elimination.
        bots.clear();health.reset();session.start(GameMode::BattleRoyale,1,level);health.damage(100);
        session.update(0,{},health,bots,false);assert(session.phase()==SessionPhase::Dead && session.result().placement==2);
        // Circle converges, damages outside combatants, without attributing zone kills to YOU.
        health.reset();bots.clear();bots.add_bot({3000,0,0},combat::WeaponId::Rifle,level,1);
        session.start(GameMode::BattleRoyale,1,level);session.circle.damage_per_second=500;
        session.update(.25F,{},health,bots,false);
        assert(!bots.bots().front().alive() && session.phase()==SessionPhase::Won && session.result().kills==0);
        health.reset();bots.clear();bots.add_bot({},combat::WeaponId::Rifle,level,1);
        session.start(GameMode::BattleRoyale,1,level);session.update(.25F,{3000,0,0},health,bots,false);
        assert(session.phase()==SessionPhase::Dead && session.result().placement==2);
    }
    // LAN sides: out with a teammate still up, the side fights on; the last rival down, the side wins.
    {
        ai::BattleRoyaleBots bots;Health health;ModeSession side;
        bots.add_bot({},combat::WeaponId::Rifle,ai::BotLevel::Easy,1);
        side.start(GameMode::BattleRoyale,1,ai::BotLevel::Easy);
        side.set_lan(1,false,1);   // one rival person, the bots mirrored, one teammate up
        health.damage(health.maximum()+1);
        side.update(.1F,{},health,bots,false);
        assert(side.phase()==SessionPhase::Dead && side.squad_fighting() && !side.squad_won());
        side.set_lan(0,false,1);side.update(.1F,{},health,bots,false);
        assert(side.phase()==SessionPhase::Dead && side.squad_fighting());   // a bot still stands
        assert(bots.damage_bot(bots.bots().front().id(),1000,{},0));
        side.update(.1F,{},health,bots,false);
        assert(side.phase()==SessionPhase::Won && side.squad_won() && side.result().placement==1);
        // Every teammate down too: the side is out, nothing more to wait for.
        ai::BattleRoyaleBots more;Health h2;ModeSession lost;
        more.add_bot({},combat::WeaponId::Rifle,ai::BotLevel::Easy,1);
        lost.start(GameMode::BattleRoyale,1,ai::BotLevel::Easy);lost.set_lan(1,false,0);
        h2.damage(h2.maximum()+1);lost.update(.1F,{},h2,more,false);
        assert(lost.phase()==SessionPhase::Dead && !lost.squad_fighting());
    }
    // Explore remains open-ended even without enemies and has a sensible public spawn.
    ModeSession explore;ai::BattleRoyaleBots none;Health health;
    explore.start(GameMode::Explore,0,ai::BotLevel::Medium);
    auto town_spawn=session_spawn(region,GameMode::Explore,1);
    assert(std::hypot(town_spawn.x,town_spawn.z)<200);
    explore.update(.1F,town_spawn,health,none,false);assert(!explore.terminal());
    assert(std::string(explore.objective()).find("Free roam")!=std::string::npos);
    health.damage(100);explore.update(.1F,town_spawn,health,none,false);assert(explore.phase()==SessionPhase::Dead);
    // Fresh match equipment, no restore of a stale/depleted BR profile, no overwrite.
    combat::WeaponSystem weapons;inventory::LootSession loot(items);
    const auto map=(directory/"maps/creator.map").string();
    const auto stale=inventory::LootSave::profile_path(map,GameMode::BattleRoyale);
    fs::create_directories(fs::path(stale).parent_path());std::ofstream(stale)<<"old profile is deliberately incompatible";
    for(int match=0;match<2;++match) {
        weapons.reset(false);loot.start(GameMode::BattleRoyale,map,weapons);
        // Battle Royale drops with only a pistol (loaded, two spare magazines).
        assert(weapons.available() && weapons.ammo(combat::WeaponId::Pistol).loaded==combat::definition(combat::WeaponId::Pistol).magazine);
        assert(loot.inventory().count("pistol")==1 && loot.inventory().count("pistol_ammo")==2*combat::definition(combat::WeaponId::Pistol).magazine);
        assert(loot.inventory().count("hunting_rifle")==0 && loot.inventory().count("rifle_ammo")==0 && loot.inventory().count("water")==0);
        assert(loot.inventory().count("frag_grenade")==0 && loot.inventory().state().stacks.size()==2 && loot.economy().money==0);
        loot.edit_inventory().clear();loot.changed();assert(loot.save(weapons));
    }
    std::ifstream old(stale);std::string text;std::getline(old,text);assert(text=="old profile is deliberately incompatible");
    fs::remove_all(directory);
    std::cout<<"Mode lifecycle: all difficulties, drops, fresh gear, kills, death, victory, restart, Explore, circle and result locking passed\n";
}
