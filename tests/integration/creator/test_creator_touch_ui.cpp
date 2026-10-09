#include "outland/creator/CreatorTouchUI.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <cassert>
#include <algorithm>
#include <iostream>
#include <vector>

namespace {
struct Touch { int id; Vector2 p; };
std::vector<Touch> touches;
Vector2 mouse{};
bool mouse_pressed=false;
}
extern "C" {
int __wrap_GetTouchPointCount() { return static_cast<int>(touches.size()); }
int __wrap_GetTouchPointId(int index) { return touches.at(index).id; }
Vector2 __wrap_GetTouchPosition(int index) { return touches.at(index).p; }
Vector2 __wrap_GetMousePosition() { return mouse; }
bool __wrap_IsMouseButtonPressed(int button) { return button==MOUSE_BUTTON_LEFT && mouse_pressed; }
}

int main() {
    using namespace outland::creator;
    CreatorController controller;
    controller.set_enabled(true);
    controller.state().flying = false;
    CreatorTouchUI ui;
    const auto frame=[&] { ui.update(controller,1280,720); };
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
    controller.set_enabled(false);
    touches={{72,{1230,110}}}; frame(); assert(!ui.actions().save);
    controller.set_enabled(true); frame(); assert(!ui.actions().save);
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
            touches={{103,{60,175}}}; frame();
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
    assert(imported == 610 && urban == 483 && characters == 82);
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
    touches={{203,{60,175}}}; frame();
    assert(controller.selected_asset()->model_path.starts_with("assets/verda/urban/Building Parts/"));
    release();
    ui.set_inventory_open(true);
    touches={{204,{640,470}}}; frame(); release(); // Characters filter switches to Props.
    touches={{205,{60,175}}}; frame(); release();
    assert(controller.selected_asset()->model_path.starts_with("assets/verda/characters/"));
    assert(controller.selected_asset()->category == CreatorAssetCategory::Prop);
    std::cout << "[PASS] Creator touch edges, modal drawer and gameplay pointer separation\n";
}
