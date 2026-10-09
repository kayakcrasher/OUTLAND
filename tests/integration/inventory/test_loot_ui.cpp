#include "outland/game/inventory/LootUI.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <vector>
namespace {
struct Touch {
    int id;
    Vector2 point;
};
std::vector<Touch> touches;
Vector2 mouse{};
bool mouse_pressed = false;
int key = 0, held = 0;
Vector2 center(Rectangle r) {
    return {r.x + r.width * .5F, r.y + r.height * .5F};
}
} // namespace
extern "C" {
int __wrap_GetTouchPointCount() {
    return static_cast<int>(touches.size());
}
int __wrap_GetTouchPointId(int index) {
    return touches.at(index).id;
}
Vector2 __wrap_GetTouchPosition(int index) {
    return touches.at(index).point;
}
Vector2 __wrap_GetMousePosition() {
    return mouse;
}
bool __wrap_IsMouseButtonPressed(int button) {
    return button == MOUSE_BUTTON_LEFT && mouse_pressed;
}
bool __wrap_IsKeyPressed(int code) {
    return code == key && code != 0;
}
bool __wrap_IsKeyDown(int code) {
    return code == held && code != 0;
}
}
int main() {
    using namespace outland::game::inventory;
    ItemRegistry registry;
    std::string error;
    const std::string root = OUTLAND_SOURCE_DIR;
    assert(registry.load(root + "/assets/verda/survival/gameplay/items.tsv",
                         root + "/assets/verda/survival/gameplay/loot_tables.tsv", error));
    Inventory bag;
    assert(bag.add(registry, "water", 12) == 12);
    assert(bag.add(registry, "canned_food", 6) == 6);
    LootUI ui(root);
    auto frame = [&](bool focus = true) { return ui.update(bag, 1280, 720, focus); };
    touches = {{91, center(LootUI::action_rect(1, 1280, 720))}};
    assert(!frame().use);
    assert(!frame().use); // Opening/holding cannot consume.
    touches.clear();
    frame();
    touches = {{91, center(LootUI::row_rect(2, 1280, 720))}};
    assert(frame().selected == 2);
    // Reordered Android contacts and dragging a held contact never retrigger use/drop.
    touches.push_back({38, center(LootUI::action_rect(1, 1280, 720))});
    assert(frame().use);
    std::swap(touches[0], touches[1]);
    assert(!frame().use);
    touches[0].point = center(LootUI::action_rect(3, 1280, 720));
    assert(!frame().drop_stack);
    touches.clear();
    frame();
    touches = {{4, center(LootUI::action_rect(2, 1280, 720))}};
    assert(!frame(false).drop_one && !frame(true).drop_one);
    touches.clear();
    mouse_pressed = false;
    frame();
    touches = {{72, center(LootUI::row_rect(0, 1280, 720))}};
    mouse = center(LootUI::action_rect(3, 1280, 720));
    mouse_pressed = true;
    assert(!frame().drop_stack);
    touches.clear();
    assert(!frame().drop_stack);
    mouse_pressed = false;
    frame();
    mouse_pressed = true;
    assert(frame().drop_stack);
    mouse_pressed = false;
    key = KEY_DOWN;
    assert(frame().selected == 1);
    key = KEY_ENTER;
    assert(frame().use);
    key = KEY_DELETE;
    held = KEY_LEFT_SHIFT;
    assert(frame().drop_stack);
    held = key = 0;
    for (const auto size : std::vector<Vector2>{{960, 540}, {1920, 1080}}) {
        ui.opened();
        touches.clear();
        ui.update(bag, static_cast<int>(size.x), static_cast<int>(size.y), true);
        touches = {{12, center(LootUI::action_rect(0, static_cast<int>(size.x),
                                                   static_cast<int>(size.y)))}};
        assert(ui.update(bag, static_cast<int>(size.x), static_cast<int>(size.y), true).close);
    }
    touches.clear();
    bag.clear();
    assert(bag.add(registry, "backpack", 1) == 1 && bag.equip_container(registry, "backpack"));
    assert(bag.add(registry, "water", 100) == 100);
    ui.opened();
    frame();
    touches = {{11, center(LootUI::action_rect(5, 1280, 720))}};
    assert(frame().selected == 6);
    touches.clear();
    frame();
    touches = {{12, center(LootUI::row_rect(4, 1280, 720))}};
    assert(frame().selected == 10);
    bag.clear();
    touches.clear();
    const auto empty = frame();
    assert(empty.selected == 0 && !empty.use);
    std::cout << "[PASS] Inventory pages, stable multitouch, no synthesized mouse repeat, focus "
                 "cancellation and scaled controls\n";
}
