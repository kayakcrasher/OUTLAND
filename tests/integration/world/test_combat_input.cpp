#include "outland/input/InputSystem.hpp"
#include <raylib.h>
#include <cassert>
#include <iostream>
#include <vector>

namespace {
std::vector<Vector2> touches;
Vector2 mouse{};
bool mouse_pressed=false,mouse_down=false,mouse_released=false,right_down=false;
bool walking=false,sprinting=false,key_reload=false,key_weapon=false;
}
extern "C" {
int GetTouchPointCount() { return static_cast<int>(touches.size()); }
int GetTouchPointId(int index) { return index; }
Vector2 GetTouchPosition(int index) { return touches[static_cast<std::size_t>(index)]; }
Vector2 GetMousePosition() { return mouse; }
bool IsMouseButtonPressed(int button) { return button==MOUSE_BUTTON_LEFT && mouse_pressed; }
bool IsMouseButtonDown(int button) { return button==MOUSE_BUTTON_LEFT ? mouse_down : right_down; }
bool IsMouseButtonReleased(int button) { return button==MOUSE_BUTTON_LEFT && mouse_released; }
bool IsKeyDown(int key) { return (key==KEY_W && walking) || (key==KEY_LEFT_SHIFT && sprinting); }
bool IsKeyPressed(int key) { return (key==KEY_R && key_reload) || (key==KEY_TAB && key_weapon); }
}

int main() {
    outland::input::InputSystem input;
    const auto layout=input.layout();
    const auto point=[](const outland::input::TouchElementLayout& e){return Vector2{e.x*1280,e.y*720};};
    touches={point(layout.fire),point(layout.reload)};
    input.update(1280,720);
    assert(input.player().fire && input.player().reload && !input.player().next_weapon);
    input.update(1280,720);
    assert(input.player().fire && !input.player().reload); // One reload per touch press.
    touches.clear();input.update(1280,720);
    touches={point(layout.weapon)};input.update(1280,720);assert(input.player().next_weapon);
    input.update(1280,720);assert(!input.player().next_weapon);
    touches.clear();input.update(1280,720);
    mouse=point(layout.weapon);mouse_pressed=mouse_down=true;
    input.update(1280,720);assert(input.player().next_weapon && !input.player().fire);
    mouse_pressed=false;input.update(1280,720);assert(!input.player().next_weapon);
    mouse_down=false;mouse_released=true;input.update(1280,720);mouse_released=false;
    mouse={100,300};mouse_pressed=mouse_down=true;input.update(1280,720);assert(input.player().fire);
    mouse={1200,50};input.update(1280,720);assert(!input.player().fire); // UI is not a trigger.
    mouse_pressed=mouse_down=false;mouse_released=true;input.update(1280,720);mouse_released=false;
    walking=true;input.update(1280,720);assert(input.player().move_y==-1 && !input.player().sprint);
    sprinting=true;input.update(1280,720);assert(input.player().sprint);
    walking=sprinting=false;right_down=true;key_weapon=key_reload=true;
    input.update(1280,720);assert(input.player().aim && input.player().reload && input.player().next_weapon);
    std::cout<<"[PASS] Native touch/X11 fire, reload/switch edge triggers, UI exclusion, walking versus sprint and mouse/keyboard combat controls\n";
}
