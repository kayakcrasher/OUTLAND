#include "outland/creator/CreatorTouchUI.hpp"
#include "outland/dev/DevLab.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <cassert>
#include <algorithm>
#include <iostream>
#include <vector>
#include <filesystem>
#include <deque>
#include <string>
#include <raymath.h>
#include "outland/world/terrain/TerrainHeight.hpp"

namespace {
struct Touch { int id; Vector2 p; };
std::vector<Touch> touches;
Vector2 mouse{};
bool mouse_pressed=false,mouse_down=false;
int pressed_key=0,held_key=0;std::deque<int> typed_letters;
}
extern "C" {
bool __wrap_IsWindowFocused(){return true;}
int __wrap_GetTouchPointCount() { return static_cast<int>(touches.size()); }
int __wrap_GetTouchPointId(int index) { return touches.at(index).id; }
Vector2 __wrap_GetTouchPosition(int index) { return touches.at(index).p; }
Vector2 __wrap_GetMousePosition() { return mouse; }
bool __wrap_IsMouseButtonDown(int button) {return button==MOUSE_BUTTON_LEFT && mouse_down;}
bool __wrap_IsKeyPressed(int key) {return key!=0 && key==pressed_key;}
bool __wrap_IsKeyDown(int key) {return key!=0 && key==held_key;}
int __wrap_GetCharPressed() {if(typed_letters.empty())return 0;const int c=typed_letters.front();typed_letters.pop_front();return c;}
bool __wrap_IsMouseButtonPressed(int button) { return button==MOUSE_BUTTON_LEFT && mouse_pressed; }
}

int main() {
    using namespace outland::creator;
    CreatorController controller;
    outland::dev::DevLab lab;
    lab.begin_builder();
    assert(lab.building());
    controller.set_enabled(lab.building());
    controller.state().flying = false;
    CreatorTouchUI ui;
    const auto frame=[&] {
        controller.set_enabled(lab.building());
        ui.update(controller,1280,720);
    };
    const auto release=[&] { touches.clear(); mouse_pressed=false; frame(); };
    touches={{91,{1230,110}}}; frame(); assert(ui.actions().save);
    frame(); assert(!ui.actions().save); // Holding save does not write each frame.
    touches[0].p={1230,160}; frame(); assert(!ui.actions().toggle_fly);
    release();
    touches={{37,{1230,160}}}; frame(); assert(ui.actions().toggle_fly && controller.state().flying);
    frame(); assert(!ui.actions().toggle_fly && controller.state().flying);
    release();
    touches={{4,{1210,610}}}; frame(); assert(ui.inventory_open());
    frame(); assert(ui.inventory_open());
    assert(ui.owns_point({205,547},1280,720));
    // Toolbar cannot modify world while the drawer is modal.
    touches.push_back({8,{1230,110}}); frame(); assert(!ui.actions().save);
    release();
    touches={{4,{1210,610}}}; frame(); assert(!ui.inventory_open());
    assert(!ui.owns_point({205,547},1280,720));
    assert(ui.owns_point({1230,110},1280,720));
    release();
    // Native touch suppresses the mouse event synthesized by Android.
    mouse={1230,110}; mouse_pressed=true;
    touches={{72,{205,547}}}; frame(); assert(!ui.actions().save);
    release();
    mouse_pressed=true; frame(); assert(ui.actions().save);
    release();
    lab.toggle_build();
    touches={{72,{1230,110}}}; frame(); assert(!ui.actions().save);
    lab.toggle_build(); frame(); assert(!ui.actions().save);
    release();
    touches={{73,{1210,510}}}; frame(); assert(ui.actions().rotate);
    frame(); assert(!ui.actions().rotate);
    release();
    // Every page can assign its model to the hotbar, including the final partial page.
    for (const auto category : {CreatorAssetCategory::Building, CreatorAssetCategory::BuildingPart,
         CreatorAssetCategory::Road, CreatorAssetCategory::Prop}) {
        const auto assets = controller.registry().category(category);
        const int category_index = static_cast<int>(category);
        for (std::size_t page = 0; page * 8 < assets.size(); ++page) {
            ui.set_inventory_open(true);
            touches={{101,{22 + (1280-44-24)/7.0F * (category_index+.5F),105}}}; frame(); release();
            for (std::size_t n = 0; n < page; ++n) {
                touches={{102,{1200,526}}}; frame(); release();
            }
            touches={{103,{60,195}}}; frame();
            assert(!ui.inventory_open());
            assert(controller.selected_asset()->id == assets[page * 8]->id);
            release();
        }
    }
    // Each downloaded model is placeable through the same controller path.
    outland::world::VerdaRegion region;
    const auto building_count = region.settlements().front().buildings.size();
    int imported = 0, urban = 0, characters = 0;
    for (std::size_t index = 0; index < controller.registry().size(); ++index) {
        const auto& asset = controller.registry().assets()[index];
        if (!asset.model_path.starts_with("assets/verda/creator/downtown/") &&
            !asset.model_path.starts_with("assets/verda/urban/") &&
            !asset.model_path.starts_with("assets/verda/characters/")) continue;
        assert(controller.select_asset(index));
        controller.update(region, {0,1,8}, {0,0,-1});
        assert(controller.place_selected(region));
        assert(region.settlements().front().assets.back().model_path == asset.model_path);
        if (asset.category == CreatorAssetCategory::Road)
            assert(!region.settlements().front().assets.back().collision);
        ++imported;
        urban += asset.model_path.starts_with("assets/verda/urban/");
        characters += asset.model_path.starts_with("assets/verda/characters/");
    }
    assert(imported == 613 && urban == 484 && characters == 84);
    assert(region.settlements().front().buildings.size() == building_count);
    // Each category marker must still be a normal persisted NpcSpawn, selected by ID.
    for (const auto* id : {"npc_spawn", "npc_spawn_emergency", "npc_spawn_hostile", "npc_spawn_creature"}) {
        const auto& definitions=controller.registry().assets();
        const auto found=std::find_if(definitions.begin(),definitions.end(),[&](const auto& asset){return asset.id==id;});
        assert(found!=definitions.end());
        assert(controller.select_asset(static_cast<std::size_t>(found-definitions.begin())));
        controller.update(region,{0,1,8},{0,0,-1});
        assert(controller.place_selected(region));
        const auto& marker=region.settlements().front().gameplay_markers.back();
        assert(marker.type==outland::world::GameplayMarkerType::NpcSpawn);
        assert(marker.id.starts_with(std::string("creator_marker_")+id+"_"));
    }
    // Filter the larger urban pack without changing stable hotbar/asset IDs.
    ui.set_inventory_open(true);
    touches={{201,{270,105}}}; frame(); release(); // Building parts category.
    touches={{202,{640,470}}}; frame(); release(); // Urban-only filter.
    touches={{203,{60,195}}}; frame();
    assert(controller.selected_asset()->model_path.starts_with("assets/verda/urban/Building Parts/"));
    release();
    ui.set_inventory_open(true);
    touches={{204,{640,470}}}; frame(); release(); // Characters filter switches to Props.
    touches={{205,{60,195}}}; frame(); release();
    assert(controller.selected_asset()->model_path.starts_with("assets/verda/characters/"));
    assert(controller.selected_asset()->category == CreatorAssetCategory::Prop);
    release();ui.set_inventory_open(true);
    for(const auto* prefix:{"assets/verda/survival/","assets/verda/industrial/","assets/verda/vehicles/"}){
        touches={{800,{640,470}}};frame();release();const auto filtered=ui.drawer_assets(controller);assert(!filtered.empty());
        for(const auto* asset:filtered)assert(asset->model_path.starts_with(prefix));
        if(std::string(prefix).find("industrial")!=std::string::npos)assert(filtered.size()==13);
        if(std::string(prefix).find("vehicles")!=std::string::npos)assert(filtered.size()>=1);
    }
    touches={{801,{640,470}}};frame();release();assert(ui.drawer_assets(controller).size()>600);
    ui.set_inventory_open(false);ui.show_all_assets();ui.set_search("");
    assert(ui.drawer_assets(controller).size()==controller.registry().size());
    for(const auto& asset:controller.registry().assets())if(asset.model_path.empty())assert(asset.category==CreatorAssetCategory::Building || asset.category==CreatorAssetCategory::Gameplay);
    for(const auto& model:std::filesystem::directory_iterator(std::string(OUTLAND_SOURCE_DIR)+"/assets/verda/starter")) {
        if(model.path().extension()!=".obj")continue;
        const auto path="assets/verda/starter/"+model.path().filename().string();
        const auto& definitions=controller.registry().assets();
        const auto found=std::find_if(definitions.begin(),definitions.end(),[&](const auto& asset){return asset.model_path==path;});
        assert(found!=definitions.end());assert(controller.select_asset(static_cast<std::size_t>(found-definitions.begin())));
        controller.update(region,{0,1,8},{0,0,-1});assert(controller.place_selected(region));
    }
    int survival=0;
    for(const auto& model:std::filesystem::directory_iterator(std::string(OUTLAND_SOURCE_DIR)+"/assets/verda/survival/runtime")) {
        if(model.path().extension()!=".glb")continue;
        const auto path="assets/verda/survival/runtime/"+model.path().filename().string();
        const auto& definitions=controller.registry().assets();
        const auto found=std::find_if(definitions.begin(),definitions.end(),[&](const auto& asset){return asset.model_path==path;});
        assert(found!=definitions.end());assert(controller.select_asset(static_cast<std::size_t>(found-definitions.begin())));
        controller.update(region,{0,1,8},{0,0,-1});assert(controller.place_selected(region));assert(region.settlements().front().assets.back().model_path==path);++survival;
    }
    assert(survival==44);
    ui.set_search("fire");assert(!ui.drawer_assets(controller).empty());
    for(const auto* asset:ui.drawer_assets(controller))assert(asset->id.find("fire")!=std::string::npos || asset->model_path.find("fire")!=std::string::npos);
    ui.set_search("THIS ASSET DOES NOT EXIST");assert(ui.drawer_assets(controller).empty());ui.set_search("");
    auto tap=[&](BuilderControl control,int id=303){const auto rect=ui.control_button(control,1280,720);touches={{id,{rect.x+rect.width/2,rect.y+rect.height/2}}};frame();};
    release();tap(BuilderControl::Up);assert(ui.actions().fly_vertical==1);frame();assert(ui.actions().fly_vertical==1);
    touches[0].p={1200,500};frame();assert(ui.actions().fly_vertical==1 && !ui.actions().rotate);release();assert(ui.actions().fly_vertical==0);
    tap(BuilderControl::Undo);assert(ui.actions().undo);frame();assert(!ui.actions().undo);release();
    const auto distance=controller.state().placement_distance;tap(BuilderControl::Far);assert(controller.state().placement_distance==distance+1);release();
    tap(BuilderControl::Raise);assert(controller.state().placement_height==.25F);release();
    tap(BuilderControl::Grid);assert(controller.state().grid_step==1);release();
    ui.set_inventory_open(true);tap(BuilderControl::Search);release();typed_letters={'r','o','c','k'};frame();assert(!ui.drawer_assets(controller).empty());
    pressed_key=KEY_ENTER;frame();pressed_key=0;ui.set_inventory_open(false);ui.set_search("");
    // Grid/height controls alter the actual placement ghost, including pitched free placement.
    controller.state().snap_to_ground=false;controller.state().placement_height=2;controller.state().grid_step=1;
    controller.update(region,{10.1F,20,30.2F},{0,-.5F,-1});
    assert(controller.preview().valid && controller.preview().position.y<22 && controller.preview().position.x==10);
    controller.state().snap_to_ground=true;controller.update(region,{10.1F,20,30.2F},{0,0,-1});
    assert(std::abs(controller.preview().position.y-(outland::world::terrain::TerrainHeight::sample(controller.preview().position.x,controller.preview().position.z)+2))<.0001F);
    // Native roads can also be selected, moved, rotated and duplicated.
    outland::world::VerdaRegion road_world;
    for(auto& settlement:road_world.editable_settlements()){settlement.buildings.clear();settlement.assets.clear();settlement.gameplay_markers.clear();settlement.roads.clear();}
    outland::world::Road road;road.start={-5,0,0};road.end={5,0,0};road_world.editable_settlements().front().roads.push_back(road);
    const float ground=outland::world::terrain::TerrainHeight::sample(0,0);
    assert(controller.select_target(road_world,{0,ground+20,0},{0,-1,0}));assert(controller.selection().type==CreatorSelectionType::Road);
    controller.update(road_world,{50,ground+20,50},{0,0,-1});assert(controller.selection().valid());assert(controller.move_selected(road_world));
    assert(controller.rotate_selected(road_world,90));assert(std::abs(Vector3Distance(road_world.settlements().front().roads[0].start,road_world.settlements().front().roads[0].end)-10)<.001F);
    assert(controller.duplicate_selected(road_world) && road_world.settlements().front().roads.size()==2);
    for(const auto viewport:{Vector2{360,640},Vector2{640,240},Vector2{700,393},Vector2{640,360},Vector2{960,540},Vector2{1920,1080}}) {
        touches.clear();ui.update(controller,static_cast<int>(viewport.x),static_cast<int>(viewport.y));
        const auto button=ui.control_button(BuilderControl::Undo,static_cast<int>(viewport.x),static_cast<int>(viewport.y));
        const Vector2 point{button.x+button.width/2,button.y+button.height/2};assert(ui.owns_point(point,static_cast<int>(viewport.x),static_cast<int>(viewport.y)));
        touches={{991,point}};ui.update(controller,static_cast<int>(viewport.x),static_cast<int>(viewport.y));assert(ui.actions().undo);
        touches.clear();ui.update(controller,static_cast<int>(viewport.x),static_cast<int>(viewport.y));
    }
    // A selection survives later frames in build mode (the former renderer toggled it off).
    const auto road_center=Vector3Scale(Vector3Add(road_world.settlements().front().roads[0].start,road_world.settlements().front().roads[0].end),.5F);
    assert(controller.select_target(road_world,Vector3Add(road_center,{0,100,0}),{0,-1,0}));
    const auto selected=controller.selection().type;
    release();frame();assert(controller.selection().valid() && controller.selection().type==selected);
    // DEV tools remain reachable while the asset drawer is open; they own opening/closing taps.
    ui.set_inventory_open(true);touches={{400,{640,22}}};lab.update(1280,720);
    assert(lab.tools_open());ui.update(controller,1280,720,true);assert(!ui.actions().save);
    touches.clear();lab.update(1280,720);ui.update(controller,1280,720,true);
    touches={{401,{450,100}}};lab.update(1280,720);assert(lab.take_build_toggle());
    ui.update(controller,1280,720,true);lab.toggle_build();controller.set_enabled(lab.building());
    assert(!lab.building());frame();assert(!ui.actions().place && !ui.actions().save);
    lab.begin_builder();ui.set_inventory_open(false);release();
    // Complete drawer -> placement -> selection flow uses the same mode and frame gate.
    outland::world::VerdaRegion authored;
    for(auto& settlement:authored.editable_settlements()){settlement.buildings.clear();settlement.assets.clear();settlement.gameplay_markers.clear();settlement.roads.clear();}
    const auto& catalog=controller.registry().assets();
    const auto item=std::find_if(catalog.begin(),catalog.end(),[](const auto& asset){return asset.model_path.starts_with("assets/verda/survival/runtime/");});
    assert(item!=catalog.end());ui.show_all_assets();ui.set_search(item->id);ui.set_inventory_open(true);
    touches={{410,{60,195}}};frame();assert(!ui.inventory_open() && controller.selected_asset()->id==item->id);release();
    controller.state().snap_to_ground=true;controller.state().placement_height=0;controller.state().grid_step=0;
    controller.update(authored,{0,20,8},{0,0,-1});
    touches={{411,{1230,460}}};frame();assert(ui.actions().place);
    assert(controller.place_selected(authored));assert(authored.settlements().front().assets.back().model_path==item->model_path);release();
    const auto placed=authored.settlements().front().assets.back().position;
    assert(controller.select_target(authored,Vector3Add(placed,{0,100,0}),{0,-1,0}));
    frame();assert(controller.selection().valid());
    // Export has touch and X11 mouse edges, and modal contacts cannot leak on release.
    tap(BuilderControl::Export,402);assert(ui.actions().export_world);frame();assert(!ui.actions().export_world);release();
    const auto export_button=ui.control_button(BuilderControl::Export,1280,720);
    mouse={export_button.x+5,export_button.y+5};mouse_pressed=true;frame();assert(ui.actions().export_world);release();
    touches={{403,{1230,110}}};ui.update(controller,1280,720,true);frame();assert(!ui.actions().save);release();
    std::cout << "[PASS] Creator touch edges, modal drawer and gameplay pointer separation\n";
}
