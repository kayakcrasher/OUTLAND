#include "outland/game/PauseMenu.hpp"
#include <raylib.h>
#include <iostream>
#include <string>

// The pause menu's layout on a PC, a big phone and a small phone: every button on screen, inside
// its panel and clear of the others, and the MENU button where AUDIO was.
using namespace outland;
namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) { ++failures; std::cerr << "[FAIL] " << what << '\n'; }
}
bool inside(Rectangle inner, Rectangle outer) {
    return inner.x >= outer.x - .5F && inner.y >= outer.y - .5F && inner.x + inner.width <= outer.x + outer.width + .5F &&
           inner.y + inner.height <= outer.y + outer.height + .5F;
}
} // namespace

int main() {
    using Menu = game::PauseMenu;
    for (const auto [w, h] : {std::pair{1280, 720}, std::pair{1920, 1080}, std::pair{2400, 1080}, std::pair{844, 390}}) {
        const std::string at = " at " + std::to_string(w) + "x" + std::to_string(h);
        const Rectangle screen{0, 0, static_cast<float>(w), static_cast<float>(h)};
        const auto main = Menu::panel(w, h, Menu::Page::Main);
        check(inside(main, screen), "main panel on screen" + at);
        for (int i = 0; i < 6; ++i) {
            check(inside(Menu::main_button(i, w, h), main), "main button in its panel" + at);
            for (int j = 0; j < i; ++j) check(!CheckCollisionRecs(Menu::main_button(i, w, h), Menu::main_button(j, w, h)), "main buttons apart" + at);
        }
        const auto settings = Menu::panel(w, h, Menu::Page::Settings);
        check(inside(settings, screen), "settings panel on screen" + at);
        std::vector<Rectangle> taken{Menu::footer_button(0, Menu::Page::Settings, w, h), Menu::footer_button(1, Menu::Page::Settings, w, h)};
        for (int r = 0; r < Menu::row_count; ++r)
            for (int side = 0; side < 2; ++side) {
                const auto b = Menu::row_button(static_cast<Menu::Row>(r), side, w, h);
                check(inside(b, settings), "setting in its panel" + at);
                bool apart = true;
                for (const auto& other : taken) apart &= !CheckCollisionRecs(b, other) || (other.x == b.x && other.y == b.y);
                check(apart, "settings apart" + at);
                taken.push_back(b);
            }
        const auto controls = Menu::panel(w, h, Menu::Page::Controls);
        check(inside(controls, screen), "controls panel on screen" + at);
        std::vector<Rectangle> slots{Menu::footer_button(0, Menu::Page::Controls, w, h), Menu::footer_button(1, Menu::Page::Controls, w, h)};
        for (int a = 0; a < static_cast<int>(input::action_count); ++a)
            for (int k = 0; k < 2; ++k) {
                const auto b = Menu::slot(a, k, w, h);
                check(inside(b, controls), "key slot in its panel" + at);
                for (const auto& other : slots) check(!CheckCollisionRecs(b, other), "key slots apart" + at);
                slots.push_back(b);
            }
        check(inside(Menu::menu_button(w, h), screen), "MENU button on screen" + at);
    }
    // Defaults read as in PC shooters.
    const auto keys = input::Bindings::defaults();
    check(keys[input::Action::Fire][0] == input::Bindings::mouse(MOUSE_BUTTON_LEFT), "left mouse fires");
    check(keys[input::Action::Aim][0] == input::Bindings::mouse(MOUSE_BUTTON_RIGHT), "right mouse aims");
    check(keys[input::Action::Forward][0] == KEY_W && keys[input::Action::Jump][0] == KEY_SPACE && keys[input::Action::Reload][0] == KEY_R &&
          keys[input::Action::Interact][0] == KEY_E && keys[input::Action::Inventory][0] == KEY_TAB, "WASD, SPACE, R, E, TAB");
    // No key does two things.
    for (std::size_t i = 0; i < input::action_count; ++i)
        for (int a = 0; a < 2; ++a)
            for (std::size_t j = 0; j < input::action_count; ++j)
                for (int b = 0; b < 2; ++b)
                    if ((i != j || a != b) && keys.codes[i][static_cast<std::size_t>(a)] != 0)
                        check(keys.codes[i][static_cast<std::size_t>(a)] != keys.codes[j][static_cast<std::size_t>(b)], "one action per key");
    if (failures) return 1;
    std::cout << "[PASS] Pause menu layout on PC and phone screens; PC-style default keys\n";
}
