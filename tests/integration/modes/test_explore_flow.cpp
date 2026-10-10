#include "outland/game/ModeSession.hpp"
#include "outland/game/economy/Economy.hpp"
#include "outland/game/inventory/LootSession.hpp"
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/game/law/Justice.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/sky/DayNight.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
using namespace outland;
using namespace outland::game;
int main() {
    namespace fs=std::filesystem;const fs::path root=OUTLAND_SOURCE_DIR;
    const auto dir=fs::temp_directory_path()/("outland-explore-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    world::VerdaRegion region;assert(creator::CreatorMapIO::load(region,(root/"maps/verda_world.map").string()));
    characters::CharacterRegistry characters;inventory::ItemRegistry items;vehicles::VehicleRegistry definitions;std::string error;
    assert(characters.load((root/"assets/verda/characters/character_manifest.tsv").string(),error));
    assert(items.load((root/"assets/verda/survival/gameplay/items.tsv").string(),(root/"assets/verda/survival/gameplay/loot_tables.tsv").string(),error));
    assert(definitions.load((root/"assets/verda/vehicles/vehicle_manifest.tsv").string(),error));
    combat::WeaponSystem weapons;weapons.reset(false);inventory::LootSession loot(items);
    loot.start(GameMode::Explore,(dir/"maps/world.map").string(),weapons);
    characters::NpcSystem npcs;life::LifeSimulation population;population.build(region,&characters);
    population.reset(npcs,life::WorldClock::at(0,9,30));assert(!population.residents().empty());
    economy::Economy economy;assert(economy.load(items,(root/"assets/verda/economy/offers.tsv").string(),error));
    economy.build(population.places());assert(!economy.shops().empty());
    vehicles::VehicleSystem cars(definitions);cars.set_training_structure(false);cars.reconcile(region);
    Health health;ModeSession session;ai::BattleRoyaleBots bots;session.start(GameMode::Explore,0,ai::BotLevel::Medium);
    const auto feet=session_spawn(region,GameMode::Explore,1);
    characters::NpcContext context;context.player_position=feet;law::Justice justice;
    const double before=population.clock().minutes;
    for(int tick=0;tick<160;++tick) {
        npcs.reconcile(region,characters);npcs.update(.05F,context,region);population.update(.05F,feet,npcs);
        loot.update(.05F,region,feet,weapons);cars.update(.05F,{},region,feet);
        law::JusticeFrame frame;frame.dt=.05F;frame.now=tick*.05;frame.player=feet;
        frame.daylight=world::sky::sky_at(population.clock().hour()).daylight;
        justice.update(frame,population,npcs);session.update(.05F,feet,health,bots,false);
    }
    assert(population.clock().minutes>before && !session.terminal() && justice.wanted()==0);
    assert(population.stats().physical>0 && population.stats().physical<=population.config().max_physical);
    assert(weapons.available() && weapons.ammo().loaded>0);
    combat::CombatWorld combat(region);combat.set_training_range(false);
    weapons.update(.1F,{.fire=true},{{feet.x,feet.y+1.5F,feet.z},{0,.1F,1}},combat);
    assert(weapons.events().shots==1);loot.changed();
    // Trading does not replace the player bag or disable later firing/driving.
    const auto& shop=economy.shops().front();
    const economy::Offer* offer=nullptr;
    for(const auto& o:economy.offers()) if(o.kind==shop.kind && o.item=="water") {offer=&o;break;}
    if(!offer) for(const auto& o:economy.offers()) if(o.kind==shop.kind) {offer=&o;break;}
    assert(offer && economy.trade(loot.economy(),loot.edit_inventory(),items,shop,*offer,1,false).starts_with("Bought"));
    loot.changed();assert(loot.save(weapons));
    const double clock=population.clock().minutes;
    context.paused=true;npcs.update(.05F,context,region);population.update(.05F,feet,npcs,true);
    assert(population.clock().minutes==clock);context.paused=false;
    population.update(.05F,feet,npcs);assert(population.clock().minutes>clock);
    assert(!cars.vehicles().empty());
    bool entered=false;Vector3 alongside{};
    for(std::size_t index=0;index<cars.vehicles().size() && !entered;++index) {
        const auto* asset=cars.asset(region,cars.vehicles()[index].id);assert(asset);
        for(const float side:{-1.5F,1.5F}) {
            alongside={asset->position.x+side,asset->position.y,asset->position.z};
            if(cars.enter(region,static_cast<int>(index),alongside)) {entered=true;break;}
        }
    }
    assert(entered && cars.driver());
    cars.update(.1F,{.throttle=1},region,alongside);assert(cars.driver());
    Vector3 exit{};assert(cars.exit(region,exit,true));
    assert(!cars.driver() && weapons.available() && !session.terminal());
    population.set_clock(life::WorldClock::at(0,23));assert(world::sky::sky_at(population.clock().hour()).daylight<.1F);
    // Witnessed violence still raises wanted status with the same population and inventory.
    law::JusticeConfig law_config;law_config.call_min=law_config.call_max=0;justice.configure(law_config);
    law::JusticeFrame frame;frame.dt=.1F;frame.now=9;frame.player=feet;frame.daylight=1;
    justice.crime(law::CrimeKind::Assault,feet,population.residents().front().id,frame,population);
    justice.update(frame,population,npcs);assert(justice.wanted()>=2 && loot.economy().money>=0 && !session.terminal());
    fs::remove_all(dir);
    std::cout<<"Explore coexistence: populated town, civilian budgets, loot, money/store, weapons, driving, clock/night and witnesses/police passed\n";
}
