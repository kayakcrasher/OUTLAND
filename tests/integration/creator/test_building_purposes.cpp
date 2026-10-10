#include "outland/creator/CreatorController.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/life/LifePopulation.hpp"
#include "outland/world/BuildingPurpose.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
using namespace outland;
using world::BuildingPurpose;
using game::life::PlaceKind;
namespace {
void check(bool condition,const std::string& message) {
    if(!condition) {std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}
const game::life::Place* place_for(const game::life::Island& island,const std::string& building_id) {
    for(const auto& p:island.places) if(p.building_id==building_id) return &p;
    return nullptr;
}
}
int main() {
    // ---- Cycling ----
    check(world::next_building_purpose(BuildingPurpose::Auto)==BuildingPurpose::Home,"AUTO -> HOME");
    check(world::next_building_purpose(BuildingPurpose::Vacant)==BuildingPurpose::Auto,"VACANT -> AUTO");
    check(world::next_building_purpose(BuildingPurpose::Vacant,1,false)==BuildingPurpose::Home,"markers skip AUTO");
    check(world::next_building_purpose(BuildingPurpose::Home,-1,false)==BuildingPurpose::Vacant,"backwards, markers skip AUTO");
    check(world::next_building_purpose(BuildingPurpose::Home,-1)==BuildingPurpose::Auto,"backwards");
    {
        int seen=0;auto p=BuildingPurpose::Auto;
        do {p=world::next_building_purpose(p);++seen;} while(p!=BuildingPurpose::Auto && seen<100);
        check(seen==world::building_purpose_count,"cycle visits every purpose once");
    }
    check(std::string(world::building_purpose_name(BuildingPurpose::Pub))=="PUB","names");

    // ---- Which objects can carry a purpose ----
    {
        world::WorldAsset wall;wall.model_path="assets/x/brick_wall.glb";wall.size={3,3,.3F};
        world::WorldAsset shed;shed.model_path="assets/x/shed.glb";shed.size={6,3.5F,5};
        world::WorldAsset car=shed;car.vehicle.definition="hatchback";
        world::WorldAsset tower;tower.model_path="assets/verda/creator/city/towers/bank.glb";tower.size={2,2,2};
        check(!world::asset_can_have_purpose(wall) && world::asset_can_have_purpose(shed),"walls no, sheds yes");
        check(!world::asset_can_have_purpose(car) && world::asset_can_have_purpose(tower),"cars no, city towers yes");
    }

    // ---- Island life follows authored purposes ----
    world::VerdaRegion region(true);
    auto& towns=region.editable_settlements();
    check(towns.size()>=2 && towns[0].buildings.size()>=3,"default towns have buildings");
    const auto auto_island=game::life::build_island(region,nullptr);
    auto& first=towns[0].buildings[0];
    auto& second=towns[0].buildings[1];
    auto& third=towns[0].buildings[2];
    first.purpose=BuildingPurpose::Police;
    second.purpose=BuildingPurpose::Vacant;
    third.purpose=BuildingPurpose::Pub;
    // A building-sized model the downtown kit knows nothing about, given a purpose.
    world::WorldAsset barn;barn.id="creator_asset_test_barn";barn.model_path="assets/x/barn.glb";barn.size={10,5,8};
    barn.position=Vector3Add(towns[1].center,{30,0,30});barn.purpose=BuildingPurpose::Garage;
    towns[1].assets.push_back(barn);
    // A building assembled from parts: only a purpose marker says what it is.
    world::GameplayMarker clinic;clinic.id="creator_marker_building_purpose_1";clinic.type=world::GameplayMarkerType::BuildingPurpose;
    clinic.position=Vector3Add(towns[1].center,{-40,0,25});clinic.rotation_y=90;clinic.purpose=BuildingPurpose::Clinic;
    towns[0].gameplay_markers.push_back(clinic); // stored under the first town, as the Creator does
    // A marker inside an existing building retags it.
    world::GameplayMarker church;church.id="creator_marker_building_purpose_2";church.type=world::GameplayMarkerType::BuildingPurpose;
    const auto& fourth=towns[0].buildings[3];
    church.position=fourth.position;church.purpose=BuildingPurpose::Church;
    towns[0].gameplay_markers.push_back(church);

    const auto island=game::life::build_island(region,nullptr);
    const auto* police=place_for(island,first.id);
    check(police && police->kind==PlaceKind::Police,"authored POLICE building");
    check(place_for(island,second.id)==nullptr && place_for(auto_island,second.id)!=nullptr,"VACANT building leaves island life");
    check(place_for(island,third.id) && place_for(island,third.id)->kind==PlaceKind::Pub,"authored PUB building");
    const auto* garage=place_for(island,barn.id);
    check(garage && garage->kind==PlaceKind::Garage && garage->settlement==1,"a purposed model becomes a place in the town it stands in");
    const auto* made=place_for(island,clinic.id);
    check(made && made->kind==PlaceKind::Clinic && made->settlement==1,"a purpose marker in a part-built building makes a place");
    const Vector3 expected_door=game::life::door_position(clinic.position,{0,0,0},clinic.rotation_y,2.0F);
    check(Vector3Distance(made->door,expected_door)<.01F && Vector3Distance(made->door,clinic.position)>1.9F,"its door is out of the front");
    check(place_for(island,fourth.id) && place_for(island,fourth.id)->kind==PlaceKind::Church,"a marker inside a building retags it");
    check(place_for(island,church.id)==nullptr,"...without making a second place");
    // People work at authored places.
    int officers=0,garage_hands=0;
    const auto police_index=static_cast<int>(police-island.places.data()),garage_index=static_cast<int>(garage-island.places.data());
    for(const auto& r:island.residents) {officers+=r.work==police_index;garage_hands+=r.work==garage_index;}
    check(officers>0 && garage_hands>0,"authored workplaces are staffed ("+std::to_string(officers)+" officers, "+std::to_string(garage_hands)+" mechanics)");
    // Deterministic.
    const auto again=game::life::build_island(region,nullptr);
    check(again.places.size()==island.places.size() && again.residents.size()==island.residents.size(),"deterministic");

    // ---- Saved with the map ----
    const auto path=std::filesystem::temp_directory_path()/"outland_purpose_test.map";
    check(creator::CreatorMapIO::save(region,path.string()),"save");
    world::VerdaRegion loaded(false);
    check(creator::CreatorMapIO::load(loaded,path.string()),"load");
    const auto& lt=loaded.settlements();
    check(lt[0].buildings[0].purpose==BuildingPurpose::Police && lt[0].buildings[1].purpose==BuildingPurpose::Vacant,"building purposes round trip");
    check(std::any_of(lt[1].assets.begin(),lt[1].assets.end(),[&](const auto& a){return a.id==barn.id && a.purpose==BuildingPurpose::Garage;}),"asset purpose round trip");
    check(std::any_of(lt[0].gameplay_markers.begin(),lt[0].gameplay_markers.end(),[&](const auto& m){
        return m.id==clinic.id && m.type==world::GameplayMarkerType::BuildingPurpose && m.purpose==BuildingPurpose::Clinic;}),"marker purpose round trip");
    check(lt[0].buildings[5].purpose==BuildingPurpose::Auto,"untouched buildings stay AUTO");
    // A purpose for something that doesn't exist, or out of range, is a broken file.
    std::string text;{std::ifstream in(path);text.assign(std::istreambuf_iterator<char>(in),{});}
    for(const auto& bad:{std::string("PURPOSE \"nobody\" 3\n"),std::string("PURPOSE \"")+barn.id+"\" 99\n"}) {
        const auto broken=std::filesystem::temp_directory_path()/"outland_purpose_bad.map";
        std::ofstream(broken)<<text<<bad;
        world::VerdaRegion untouched(false);
        check(!creator::CreatorMapIO::load(untouched,broken.string()),"reject: "+bad);
    }
    // Version 6 maps (before purposes) still load.
    world::VerdaRegion legacy(true);
    check(creator::CreatorMapIO::load(legacy,std::string(OUTLAND_SOURCE_DIR)+"/maps/verda_loot_defaults.map"),"v6 map loads");

    // ---- In the builder: place a purpose marker, select it, cycle it; select a house and cycle it ----
    {
        creator::CreatorController controller;controller.set_enabled(true);
        world::VerdaRegion world(true);
        const auto& catalog=controller.registry().assets();
        const auto entry=std::find_if(catalog.begin(),catalog.end(),[](const auto& a){return a.id=="building_purpose";});
        check(entry!=catalog.end(),"Building Purpose is in the catalog");
        check(controller.select_asset(static_cast<std::size_t>(entry-catalog.begin())),"select it");
        controller.state().snap_to_ground=true;controller.state().placement_height=0;controller.state().grid_step=0;
        bool placed=false;
        for(float x=0;x<400 && !placed;x+=37) {
            controller.update(world,{x,20,8},{0,0,-1});
            placed=controller.preview().valid && !controller.preview().blocked && controller.place_selected(world);
        }
        check(placed,"place a purpose marker");
        const auto& marker=world.settlements().front().gameplay_markers.back();
        check(marker.type==world::GameplayMarkerType::BuildingPurpose && marker.purpose==BuildingPurpose::Home,"new markers say HOME");
        const auto at=marker.position;
        check(controller.select_target(world,Vector3Add(at,{0,1,2.5F}),{0,0,-1}) &&
              controller.selection().type==creator::CreatorSelectionType::GameplayMarker,"select the marker");
        bool applicable=false;
        check(controller.selected_purpose(world,applicable)==BuildingPurpose::Home && applicable,"USE shows HOME");
        check(controller.cycle_selected_purpose(world),"cycle");
        check(world.settlements().front().gameplay_markers.back().purpose==BuildingPurpose::Shop,"HOME -> SHOP");
        const auto& house=world.settlements()[0].buildings[0];
        controller.clear_selection();
        check(controller.select_target(world,Vector3Add(house.position,{0,50,0}),{0,-1,0}) &&
              controller.selection().type==creator::CreatorSelectionType::Building,"select a house");
        check(controller.cycle_selected_purpose(world) && world.settlements()[0].buildings[0].purpose==BuildingPurpose::Home,"house AUTO -> HOME");
        check(controller.cycle_selected_purpose(world,-1) && world.settlements()[0].buildings[0].purpose==BuildingPurpose::Auto,"and back");
    }
    std::cout<<"building purpose tests passed\n";
}
