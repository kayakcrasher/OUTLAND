#include "outland/input/InputSystem.hpp"
#include <raylib.h>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
struct Touch { int id; Vector2 position; };
std::vector<Touch> touches;
Vector2 mouse{};
bool mouse_pressed=false, mouse_down=false, right_down=false;
bool walking=false, sprinting=false, key_reload=false, key_weapon=false;
}
extern "C" {
int GetTouchPointCount() { return static_cast<int>(touches.size()); }
int GetTouchPointId(int index) { return touches.at(index).id; }
Vector2 GetTouchPosition(int index) { return touches.at(index).position; }
Vector2 GetMousePosition() { return mouse; }
bool IsMouseButtonPressed(int button) { return button==MOUSE_BUTTON_LEFT && mouse_pressed; }
bool IsMouseButtonDown(int button) { return button==MOUSE_BUTTON_LEFT ? mouse_down : right_down; }
bool IsKeyDown(int key) { return (key==KEY_W && walking) || (key==KEY_LEFT_SHIFT && sprinting); }
bool IsKeyPressed(int key) { return (key==KEY_R && key_reload) || (key==KEY_TAB && key_weapon); }
}

int main() {
    outland::input::InputSystem input;
    const auto layout=input.layout();
    const auto point=[](const outland::input::TouchElementLayout& e){return Vector2{e.x*1280,e.y*720};};
    const auto release=[&] { touches.clear(); mouse_down=mouse_pressed=false; input.update(1280,720); };
    touches={{41,point(layout.fire)},{97,point(layout.reload)}};
    input.update(1280,720);
    assert(input.player().fire && input.player().reload && !input.player().next_weapon);
    input.update(1280,720);
    assert(input.player().fire && !input.player().reload);
    release();
    touches={{41,point(layout.weapon)}};
    input.update(1280,720); assert(input.player().next_weapon);
    input.update(1280,720); assert(!input.player().next_weapon);
    release();
    // Two sticks retain stable IDs after array reordering and while crossing a button.
    auto move=point(layout.movement); move.x+=100;
    auto look=point(layout.look); look.y-=70;
    touches={{50,move},{12,look}};
    input.update(1280,720);
    assert(input.player().move_x>.5F && input.player().look_y<-.5F && !input.player().sprint);
    touches={{12,point(layout.fire)},{50,move}};
    input.update(1280,720);
    assert(input.player().look_x>0 && input.player().move_x>.5F && !input.player().fire);
    touches={{12,point(layout.fire)}};
    input.update(1280,720); assert(input.player().move_x==0 && !input.player().fire);
    release();
    // A second contact inside an occupied stick cannot steal or recapture it.
    touches={{3,move},{4,point(layout.movement)}};
    input.update(1280,720); assert(input.player().move_x>.5F);
    touches.erase(touches.begin());
    input.update(1280,720); assert(input.player().move_x==0);
    release();
    touches={{10,point(layout.jump)},{11,point(layout.interact)},
        {12,point(layout.inventory)},{13,point(layout.sprint)}};
    input.update(1280,720);
    assert(input.player().jump && input.player().brake && input.player().interact && input.player().inventory && input.player().sprint);
    input.update(1280,720);
    assert(!input.player().jump && input.player().brake && !input.player().interact && !input.player().inventory && input.player().sprint);
    touches.push_back({14,point(layout.crouch)});
    input.update(1280,720); assert(input.player().crouch && !input.player().sprint);
    release();
    // A held action keeps its role as it crosses other action buttons.
    touches={{7,point(layout.reload)}}; input.update(1280,720); assert(input.player().reload);
    touches[0].position=point(layout.fire); input.update(1280,720);
    assert(!input.player().fire && !input.player().reload);
    release();
    // Synthesized mouse events never duplicate native Android touch input.
    touches={{88,point(layout.movement)}};
    mouse=point(layout.fire); mouse_down=mouse_pressed=true;
    input.update(1280,720); assert(!input.player().fire);
    release();
    mouse=point(layout.weapon); mouse_pressed=mouse_down=true;
    input.update(1280,720); assert(input.player().next_weapon && !input.player().fire);
    mouse_pressed=false; input.update(1280,720); assert(!input.player().next_weapon);
    release();
    mouse={100,300}; mouse_pressed=mouse_down=true;
    input.update(1280,720); assert(!input.player().fire); // Only FIRE fires.
    mouse=point(layout.fire); mouse_pressed=false;
    input.update(1280,720); assert(!input.player().fire); // No slide-in capture.
    release();
    // Creator routing rejects gameplay actions and allows only unreserved navigation.
    input.update(1280,720,false);
    touches={{2,point(layout.fire)},{3,move}};
    input.update(1280,720,false,false,[](Vector2 p){ return p.x>500; });
    assert(!input.player().fire && input.player().move_x>.5F);
    input.update(1280,720,true); // Held contacts cannot fire across a mode switch.
    assert(!input.player().fire && input.player().move_x==0);
    release();
    touches={{5,point(layout.fire)}}; input.update(1280,720); assert(input.player().fire);
    input.update(1280,720,true,true); assert(!input.player().fire);
    input.update(1280,720); assert(!input.player().fire);
    release();
    touches={{6,point(layout.fire)}}; input.update(1280,720); assert(input.player().fire);
    input.cancel_controls(); input.update(1280,720); assert(!input.player().fire);
    release();
    // Fresh UI contacts can be cancelled before gameplay capture; synthetic release cannot fire.
    touches={{700,point(layout.fire)}};input.cancel_controls();input.update(1280,720);assert(!input.player().fire);
    touches.clear();mouse=point(layout.fire);mouse_down=mouse_pressed=true;input.update(1280,720);assert(!input.player().fire);
    input.update(1280,720);assert(!input.player().fire);release();
    mouse=point(layout.fire);mouse_down=mouse_pressed=true;input.cancel_controls();input.update(1280,720);assert(!input.player().fire);release();
    // Resize quarantines contacts; a new contact still gets matching geometry at 480x270.
    touches={{6,point(layout.fire)}}; input.update(1280,720); assert(input.player().fire);
    input.update(480,270); assert(!input.player().fire);
    touches.clear(); input.update(480,270);
    const float small_radius=92 * outland::input::touch_scale(480,270) * layout.movement.scale;
    touches={{20,{layout.movement.x*480 + small_radius*.55F, layout.movement.y*270}}};
    input.update(480,270);
    assert(std::abs(input.player().move_x-.5F)<.001F && input.player().move_y==0);
    touches.clear(); input.update(480,270);
    touches={{21,{layout.inventory.x*480,layout.inventory.y*270}}};
    input.update(480,270); assert(input.player().inventory);
    input.update(0,0); assert(input.player().move_x==0);
    release(); input.update(1280,720);
    walking=true; input.update(1280,720); assert(input.player().move_y==-1 && !input.player().sprint);
    sprinting=true; input.update(1280,720); assert(input.player().sprint);
    walking=sprinting=false; right_down=true; key_weapon=key_reload=true;
    input.update(1280,720); assert(input.player().aim && input.player().reload && input.player().next_weapon);
    input.update(1280,720,false); assert(!input.player().aim && !input.player().reload && !input.player().next_weapon);
    std::cout<<"[PASS] Dual sticks, stable touch ownership, actions, mouse deduplication and Creator/modal isolation\n";
}
