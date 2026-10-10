#include "outland/game/economy/Economy.hpp"
#include "outland/game/inventory/LootSession.hpp"
#include "outland/game/life/LifePopulation.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include <cassert>
#ifdef OUTLAND_DEV_TOOLS
#include "outland/creator/CreatorController.hpp"
#endif
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace outland;
using namespace outland::game;
int main() {
    namespace fs=std::filesystem;const fs::path root=OUTLAND_SOURCE_DIR;
    const auto dir=fs::temp_directory_path()/("outland-economy-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir/"maps");
    inventory::ItemRegistry items;economy::Economy economy;std::string error;
    assert(items.load((root/"assets/verda/survival/gameplay/items.tsv").string(),(root/"assets/verda/survival/gameplay/loot_tables.tsv").string(),error));
    assert(economy.load(items,(root/"assets/verda/economy/offers.tsv").string(),error));
    world::VerdaRegion region;assert(creator::CreatorMapIO::load(region,(root/"maps/verda_world.map").string()));
    const auto island=life::build_island(region,nullptr);assert(!island.residents.empty());
    economy.build(island.places);assert(!economy.shops().empty());
    const economy::Shop* general=nullptr;
    for(const auto& shop:economy.shops()) if(shop.kind=="general") {general=&shop;break;}
    assert(general);assert(economy.nearest(general->door));
    assert(!economy.nearest(general->door,[](Vector3){return false;}));
    assert(!economy.nearest({10000,0,10000}));
    auto offer=[&](const std::string& id)->const economy::Offer& {
        for(const auto& o:economy.offers()) if(o.id==id) return o;
        assert(false);return economy.offers().front();
    };
    for(const auto& item:items.items()) {
        bool sold=false;
        for(const auto& o:economy.offers()) if(o.item==item.id && o.kind=="general") sold=true;
        assert(sold);
    }
    const auto& water=offer("general_water");
    assert(inventory::pickup_unit_scale({{0,0,0},{2,0,3}})==1.0F/3); // flat carpet
    assert(inventory::pickup_unit_scale({{0,0,0},{1,2,1}})==.5F); // standing furniture
    assert(inventory::pickup_unit_scale({{0,0,0},{0,0,0}})==0);
    assert(inventory::pickup_unit_scale({{0,0,0},{1,std::numeric_limits<float>::quiet_NaN(),1}})==0);
    inventory::Inventory bag;economy::EconomyState state;
    assert(economy.trade(state,bag,items,*general,water,6,false).starts_with("Bought"));
    assert(state.money==202 && bag.count("water")==6 && economy.stock(state,*general,water)==24);
    assert(economy.trade(state,bag,items,*general,water,2,true).starts_with("Sold"));
    assert(state.money==208 && bag.count("water")==4 && economy.stock(state,*general,water)==26);
    const auto before=state;const auto stacks=bag.state().stacks;
    assert(!economy.trade(state,bag,items,*general,water,999,false).starts_with("Bought"));
    assert(!economy.trade(state,bag,items,*general,water,5,true).starts_with("Sold"));
    assert(!economy.trade(state,bag,items,*general,water,-1,false).starts_with("Bought"));
    auto forged=water;forged.buy=1;assert(economy.trade(state,bag,items,*general,forged,1,false)=="Invalid trade");
    assert(state.money==before.money && bag.state().stacks==stacks && economy.stock(state,*general,water)==26);
    inventory::Inventory full;assert(full.add(items,"battery",999)>0);
    assert(economy.trade(state,full,items,*general,water,1,false)=="Bag full or weapon already owned");
    assert(state.money==before.money);
    state.money=10000;assert(economy.trade(state,bag,items,*general,water,26,false).starts_with("Bought"));
    assert(economy.stock(state,*general,water)==0);
    const auto empty_cash=state.money;
    assert(!economy.trade(state,bag,items,*general,water,1,false).starts_with("Bought"));assert(state.money==empty_cash);
    state.restock_remaining=.1F;assert(!economy.update(state,.2F,true));assert(economy.stock(state,*general,water)==0);
    assert(!economy.update(state,std::numeric_limits<float>::quiet_NaN(),false));
    assert(economy.update(state,.2F,false) && economy.stock(state,*general,water)==water.stock);
    bag.clear();state={};
    assert(economy.trade(state,bag,items,*general,offer("general_wood_chair"),1,false).starts_with("Bought"));
    assert(bag.count("wood_chair")==1 && fs::exists(root/items.find("wood_chair")->model));
    assert(economy.trade(state,bag,items,*general,offer("general_backpack"),1,false).starts_with("Bought"));
    assert(bag.equip_container(items,"backpack"));bag.add(items,"battery",999);
    assert(!economy.trade(state,bag,items,*general,offer("general_backpack"),1,true).starts_with("Sold"));
    // Wallet + stock + bag + dropped furniture round-trip in one atomic profile.
    combat::WeaponSystem weapons;inventory::LootSession session(items);
    const auto map=(dir/"maps/creator.map").string();session.start(GameMode::Explore,map,weapons);
    assert(session.economy().money==250);
    assert(session.trade(economy,*general,water,1,false,weapons).starts_with("Bought"));
    assert(session.trade(economy,*general,offer("general_wood_table"),1,false,weapons).starts_with("Bought"));
    std::size_t table=0;while(session.inventory().state().stacks[table].item!="wood_table") ++table;
    assert(session.drop(table,true,{0,0,8},weapons).starts_with("Dropped"));
    assert(session.save(weapons));const auto saved=session.snapshot(weapons);
    inventory::LootSession restored(items);combat::WeaponSystem restored_weapons;
    restored.start(GameMode::Explore,map,restored_weapons);
    assert(restored.economy().money==saved.economy.money && restored.inventory().state().stacks==saved.inventory.stacks);
    assert(restored.world().state().pickups.size()==1 && restored.world().state().pickups.front().item=="wood_table");
    assert(economy.stock(restored.economy(),*general,water)==water.stock-1);
    // Loaded magazines cannot be sold away or converted into free money.
    const auto cash=restored.economy().money;
    assert(!restored.trade(economy,*general,offer("general_hunting_rifle"),1,true,restored_weapons).starts_with("Sold"));
    assert(restored.economy().money==cash && restored.inventory().count("hunting_rifle")==1);
    restored_weapons.set_loaded(combat::WeaponId::Rifle,0);
    assert(restored.trade(economy,*general,offer("general_hunting_rifle"),1,true,restored_weapons).starts_with("Sold"));
    assert(restored_weapons.selected()==combat::WeaponId::Pistol && restored_weapons.available());
    restored.economy().money=1000;
    assert(restored.trade(economy,*general,offer("general_hunting_rifle"),1,false,restored_weapons).starts_with("Bought"));
    assert(restored_weapons.selected()==combat::WeaponId::Rifle && restored_weapons.ammo().loaded==0);
    // Legacy V1 profile is accepted and upgraded exactly once, including starter grant.
    const auto legacy=(dir/"legacy.loot").string();
    std::ofstream(legacy)<<"OUTLAND_LOOT 1 3\nBAG \"\" 0 0 0\nWEAPONS 1 0 0\nNEXT 1\nEND\n";
    inventory::LootSnapshot snapshot;snapshot.mode=GameMode::Explore;
    assert(inventory::LootSave::load(items,snapshot,legacy,error) && snapshot.economy.money==250);
    snapshot.economy.money=0;assert(inventory::LootSave::save(items,snapshot,legacy,error));
    assert(inventory::LootSave::load(items,snapshot,legacy,error) && snapshot.economy.money==0);
    auto invalid=snapshot;invalid.economy.money=-1;assert(!inventory::LootSave::save(items,invalid,legacy,error));
    invalid=snapshot;invalid.economy.stock={{"s","o",1},{"s","o",1}};assert(!inventory::LootSave::save(items,invalid,legacy,error));
    assert(inventory::LootSave::load(items,snapshot,legacy,error) && snapshot.economy.money==0);
    std::ofstream(dir/"bad.tsv")<<"bad\n";const auto count=economy.offers().size();
    assert(!economy.load(items,(dir/"bad.tsv").string(),error) && economy.offers().size()==count);
#ifdef OUTLAND_DEV_TOOLS
    creator::CreatorController creator;
    assert(creator.registry().find("economy_wood_chair"));
    world::WorldAsset furniture;
    furniture.id="economy_furniture_test";furniture.model_path=items.find("wood_chair")->model;
    furniture.position={12,0,14};furniture.size={.45F,.95F,.45F};
    assert(region.place_world_asset(furniture));
    assert(creator::CreatorMapIO::save(region,(dir/"furniture.map").string()));
    world::VerdaRegion placed;assert(creator::CreatorMapIO::load(placed,(dir/"furniture.map").string()));
    bool found=false;
    for(const auto& town:placed.settlements()) for(const auto& a:town.assets) if(a.id==furniture.id) {found=true;assert(a.model_path==furniture.model_path);}
    assert(found);
#endif
    fs::remove_all(dir);
    std::cout<<"Economy: authored stores, supplies/furniture, exact prices, buy/sell, stock, capacity, restock, atomic profile and V1 migration passed\n";
}
