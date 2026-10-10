#include "outland/creator/CreatorMapIO.hpp"
#include "outland/creator/CreatorSession.hpp"
#include "outland/creator/CreatorTouchUI.hpp"
#include "outland/dev/DevLab.hpp"
#include "outland/input/InputSystem.hpp"
#include "outland/input/PointerEvents.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <raymath.h>
#include <vector>

struct GLFWwindow {};
namespace {
GLFWwindow window;
using Callback = void (*)(GLFWwindow *, int, int, int);
int forwarded = 0, pressed_key = 0, queued_key = 0;
bool focused = true, available = true;
Vector2 cursor{};
struct Touch {
    int id;
    Vector2 point;
};
std::vector<Touch> touches;
void original(GLFWwindow *, int, int, int) { ++forwarded; }
Callback callback = original;
GLFWwindow *context() { return available ? &window : nullptr; }
Callback set_callback(GLFWwindow *w, Callback value) {
    assert(w == &window);
    auto old = callback;
    callback = value;
    return old;
}
void get_cursor(GLFWwindow *w, double *x, double *y) {
    assert(w == &window);
    *x = cursor.x;
    *y = cursor.y;
}
} // namespace
extern "C" {
void *__wrap_dlsym(void *, const char *name) {
    if (std::strcmp(name, "glfwGetCurrentContext") == 0)
        return reinterpret_cast<void *>(context);
    if (std::strcmp(name, "glfwSetMouseButtonCallback") == 0)
        return reinterpret_cast<void *>(set_callback);
    if (std::strcmp(name, "glfwGetCursorPos") == 0)
        return reinterpret_cast<void *>(get_cursor);
    return nullptr;
}
// A complete X11 press/release between polls leaves both current and previous
// raylib button states up. These polling APIs therefore see no edge.
bool __wrap_IsMouseButtonPressed(int) { return false; }
bool __wrap_IsMouseButtonDown(int) { return false; }
Vector2 __wrap_GetMousePosition() { return cursor; }
int __wrap_GetTouchPointCount() { return static_cast<int>(touches.size()); }
int __wrap_GetTouchPointId(int i) { return touches.at(i).id; }
Vector2 __wrap_GetTouchPosition(int i) { return touches.at(i).point; }
bool __wrap_IsWindowFocused() { return focused; }
bool __wrap_IsKeyPressed(int key) { return key == pressed_key && key != 0; }
bool __wrap_IsKeyDown(int) { return false; }
int __wrap_GetCharPressed() { return 0; }
int __wrap_GetKeyPressed() {
    const int key = queued_key;
    queued_key = 0;
    return key;
}
}
int main() {
    using namespace outland;
    const auto root = std::filesystem::temp_directory_path() / "outland_creative_builder_test";
    std::filesystem::remove_all(root);
    {
        input::PointerEvents events;
        assert(events.installed() && callback != original);
        creator::CreatorController controller;
        controller.set_enabled(true);
        controller.state().grid_step = 1;
        creator::CreatorTouchUI ui;
        input::InputSystem navigation;
        dev::DevLab lab;
        lab.begin_builder();
        const auto reserved = [&](Vector2 p) {
            return lab.owns_point(p, 1280, 720) || navigation.navigation_owns_point(p, 1280, 720);
        };
        const auto frame = [&](bool blocked = false) {
            events.begin_frame();
            const bool was_open = lab.tools_open();
            lab.update(1280, 720, !lab.building());
            ui.update(controller, 1280, 720, blocked || !focused || was_open || lab.tools_open(),
                      reserved);
        };
        const auto click = [&](Vector2 p) {
            cursor = p;
            callback(&window, 0, 1, 0);
            callback(&window, 0, 0, 0);
            frame();
        };
        const auto center = [](Rectangle r) {
            return Vector2{r.x + r.width * .5F, r.y + r.height * .5F};
        };
        const auto catalog = center(ui.control_button(creator::BuilderControl::Catalog, 1280, 720));
        click(catalog);
        assert(ui.inventory_open() && forwarded == 2);
        frame();
        assert(ui.inventory_open()); // Buffered click expires instead of toggling every frame.
        click(catalog);
        assert(!ui.inventory_open());
        queued_key = KEY_B;
        frame();
        pressed_key = 0;
        assert(ui.inventory_open());
        pressed_key = KEY_TAB;
        frame();
        pressed_key = 0;
        assert(!ui.inventory_open());
        world::VerdaRegion region;
        auto building = region.settlements().front().buildings.front();
        for (auto &settlement : region.editable_settlements()) {
            settlement.buildings.clear();
            settlement.roads.clear();
            settlement.assets.clear();
            settlement.gameplay_markers.clear();
        }
        building.position = {0, 0, 0};
        building.size = {4, 4, 4};
        building.rotation_y = 45;
        region.editable_settlements().front().buildings.push_back(building);
        const float ground = world::terrain::TerrainHeight::sample(0, 0);
        Camera3D camera{
            {20, ground + 10, 20}, {0, ground + 2, 0}, {0, 1, 0}, 65, CAMERA_PERSPECTIVE};
        const auto resolve = [&] {
            if (ui.actions().world_pointer)
                ui.resolve_world_press(
                    controller, region,
                    GetScreenToWorldRayEx(ui.actions().world_point, camera, 1280, 720));
        };
        click({640, 360});
        assert(ui.actions().world_pointer);
        resolve();
        assert(controller.selection().type == creator::CreatorSelectionType::Building &&
               controller.selection().building_id == building.id);
        frame();
        assert(controller.selection().valid());
        click(catalog);
        assert(ui.inventory_open());
        const auto &definitions = controller.registry().assets();
        const auto water =
            std::find_if(definitions.begin(), definitions.end(), [](const auto &asset) {
                return asset.model_path.starts_with("assets/verda/survival/runtime/") &&
                       asset.id.find("water") != std::string::npos;
            });
        assert(water != definitions.end());
        ui.show_all_assets();
        ui.set_search(water->id);
        click({60, 195});
        assert(!ui.inventory_open() && !ui.actions().world_pointer &&
               controller.selected_asset()->id == water->id);
        const float target_ground = world::terrain::TerrainHeight::sample(30, 30);
        camera = {{30, target_ground + 10, 50},
                  {30, target_ground, 30},
                  {0, 1, 0},
                  65,
                  CAMERA_PERSPECTIVE};
        click({640, 360});
        resolve();
        assert(ui.actions().place && controller.preview().valid && !controller.preview().blocked);
        assert(controller.preview().position.x == 30 && controller.preview().position.z == 30);
        creator::CreatorSession session((root / "maps/verda_creator.map").string());
        assert(session.edit(region, [&] { return controller.place_selected(region); }));
        assert(region.settlements().front().assets.size() == 1);
        click({640, 360});
        resolve();
        assert(controller.selection().type == creator::CreatorSelectionType::WorldAsset &&
               !ui.actions().place);
        click({60, 510});
        assert(ui.active_tool() == creator::CreatorTouchTool::Move && !ui.actions().move &&
               controller.selection().valid());
        const float moved_ground = world::terrain::TerrainHeight::sample(40, 40);
        camera = {
            {40, moved_ground + 10, 60}, {40, moved_ground, 40}, {0, 1, 0}, 65, CAMERA_PERSPECTIVE};
        click({640, 360});
        resolve();
        assert(ui.actions().move);
        assert(session.edit(region, [&] { return controller.move_selected(region); }));
        assert(region.settlements().front().assets.front().position.x == 40);
        assert(session.undo(region));
        assert(region.settlements().front().assets.front().position.x == 30);
        assert(session.redo(region));
        assert(region.settlements().front().assets.front().position.x == 40);
        ui.set_active_tool(creator::CreatorTouchTool::Place);
        // A tap on either navigation stick never selects/places, nor does a held native finger.
        click({205, 547});
        assert(!ui.actions().world_pointer && !ui.actions().place);
        touches = {{7, {640, 360}}};
        frame();
        assert(ui.actions().world_pointer);
        frame();
        assert(!ui.actions().world_pointer);
        cursor = {640, 360};
        callback(&window, 0, 1, 0);
        callback(&window, 0, 0, 0);
        touches.clear();
        frame();
        assert(!ui.actions().world_pointer);
        // Focus/modal taps are consumed and cannot replay after focus returns.
        focused = false;
        click(catalog);
        ui.update(controller, 1280, 720, true, reserved);
        focused = true;
        frame();
        assert(!ui.inventory_open());
        cursor = catalog;
        callback(&window, 0, 1, 0);
        callback(&window, 0, 0, 0);
        frame(true);
        frame();
        assert(!ui.inventory_open());
        click({640, 22});
        assert(lab.tools_open() && !ui.actions().world_pointer);
        frame();
        assert(lab.tools_open());
        click({540, 320});
        assert(!lab.tools_open() && !ui.actions().world_pointer);
        pressed_key = KEY_TWO;
        frame();
        pressed_key = 0;
        assert(controller.selected_hotbar_slot() == 1 && !lab.teleport_requested());
        // SAVE and EXPORT receive the same short desktop clicks, and reload all authored geometry.
        click({1230, 110});
        assert(ui.actions().save && session.save(region));
        click(center(ui.control_button(creator::BuilderControl::Export, 1280, 720)));
        assert(ui.actions().export_world && session.export_world(region));
        world::VerdaRegion restored;
        assert(creator::CreatorMapIO::load(restored, session.export_path()));
        assert(restored.settlements().front().buildings.size() == 1 &&
               restored.settlements().front().assets.size() == 1);
        assert(restored.settlements().front().assets.front().model_path == water->model_path);
        // Ground targeting rejects sky/zero rays and retains existing free-placement and snapping.
        assert(!controller.point_preview(region, {{0, ground + 10, 0}, {0, 1, 0}}));
        assert(!controller.point_preview(region, {{0, ground + 10, 0}, {}}));
        controller.state().snap_to_ground = false;
        controller.state().placement_height = 2;
        assert(controller.point_preview(region, {{100, 50, 100}, {0, 0, -1}}));
        assert(controller.preview().position.y == 52);
        // Shared DEV/Creator geometry keeps ASSETS out from underneath DEV TOOLS
        // in the narrow/letterboxed viewport seen on a phone.
        for (const auto size : {Vector2{640, 240}, Vector2{360, 640}}) {
            const int width = static_cast<int>(size.x), height = static_cast<int>(size.y);
            creator::CreatorTouchUI compact;
            dev::DevLab tools;
            tools.begin_builder();
            cursor =
                center(compact.control_button(creator::BuilderControl::Catalog, width, height));
            callback(&window, 0, 1, 0);
            callback(&window, 0, 0, 0);
            events.begin_frame();
            tools.update(width, height, false);
            assert(!tools.tools_open());
            compact.update(controller, width, height, false, [&](Vector2 p) {
                return tools.owns_point(p, width, height) ||
                       navigation.navigation_owns_point(p, width, height);
            });
            assert(compact.inventory_open());
            const float scale = input::editor_scale(width, height);
            cursor = {width * .5F, 22 * scale};
            callback(&window, 0, 1, 0);
            callback(&window, 0, 0, 0);
            events.begin_frame();
            tools.update(width, height, false);
            assert(tools.tools_open());
            compact.update(controller, width, height, true);
            assert(compact.inventory_open());
        }
        for (int i = 0; i < 30; ++i) {
            callback(&window, 0, 1, 0);
            callback(&window, 0, 0, 0);
        }
        events.begin_frame();
        assert(input::PointerEvents::presses().size() == 8);
        events.begin_frame();
        assert(input::PointerEvents::presses().empty());
    }
    assert(callback == original);
    available = false;
    {
        input::PointerEvents events;
        assert(!events.installed());
    }
    std::filesystem::remove_all(root);
    std::cout << "[PASS] Creative catalog, short X11 clicks, screen-ray building/model selection, "
                 "ground placement, native/modal ownership, save/export and callback lifecycle\n";
}
