#include "outland/input/InputSystem.hpp"
#include "outland/input/Bindings.hpp"
#include <raylib.h>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
struct Touch { int id; Vector2 position; };
std::vector<Touch> touches;
Vector2 mouse{};
bool mouse_pressed=false, mouse_down=false, right_down=false, right_pressed=false;
float wheel=0;
bool key_f=false;
bool walking=false, sprinting=false, key_reload=false, key_weapon=false, key_grenade=false, key_sight=false;
}
extern "C" {
int GetTouchPointCount() { return static_cast<int>(touches.size()); }
int GetTouchPointId(int index) { return touches.at(index).id; }
Vector2 GetTouchPosition(int index) { return touches.at(index).position; }
Vector2 GetMousePosition() { return mouse; }
bool IsMouseButtonPressed(int button) { return button==MOUSE_BUTTON_LEFT ? mouse_pressed : button==MOUSE_BUTTON_RIGHT && right_pressed; }
bool IsMouseButtonDown(int button) { return button==MOUSE_BUTTON_LEFT ? mouse_down : right_down; }
bool IsKeyDown(int key) { return (key==KEY_W && walking) || (key==KEY_LEFT_SHIFT && sprinting) || (key==KEY_F && key_f); }
bool IsKeyPressed(int key) { return (key==KEY_R && key_reload) || (key==KEY_Q && key_weapon) || (key==KEY_G && key_grenade) || (key==KEY_X && key_sight); }
float GetMouseWheelMove() { return wheel; }
int GetKeyPressed() { return 0; }
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
    // NADE throws once per tap, alongside fire, and its button is clear of AIM and LOAD.
    touches={{41,point(layout.fire)},{42,point(layout.grenade)}};
    input.update(1280,720); assert(input.player().grenade && input.player().fire && !input.player().aim && !input.player().reload);
    input.update(1280,720); assert(!input.player().grenade && input.player().fire);
    release();
    for (const auto* other : {&layout.aim, &layout.reload, &layout.look, &layout.interact, &layout.fire_mode})
        assert(std::hypot((other->x-layout.grenade.x)*1280,(other->y-layout.grenade.y)*720)>80);
    key_grenade=true; input.update(1280,720); assert(input.player().grenade); key_grenade=false; release();
    // SIGHT changes the optic once per tap (X on a keyboard); its button sits clear of BAG and VIEW.
    touches={{43,point(layout.sight)}};
    input.update(1280,720); assert(input.player().sight && !input.player().inventory && !input.player().toggle_view);
    input.update(1280,720); assert(!input.player().sight);
    release();
    for (const auto* other : {&layout.inventory, &layout.view, &layout.interact, &layout.weapon, &layout.fire_mode})
        assert(std::hypot((other->x-layout.sight.x)*1280,(other->y-layout.sight.y)*720)>80);
    key_sight=true; input.update(1280,720); assert(input.player().sight); key_sight=false; release();
    // At the wheel: the left stick only steers, the right thumb works GAS and BRAKE, the space
    // above the pedals pans the camera, and the combat buttons are gone.
    input.set_driving(true);
    input.update(1280,720);
    {
        const auto pan=outland::input::InputSystem::pan_area(1280,720);
        const auto in_pan=[&](Vector2 p){return p.x>=pan.x && p.x<=pan.x+pan.width && p.y>=pan.y && p.y<=pan.y+pan.height;};
        auto steer=point(layout.movement); steer.x+=80; steer.y-=80; // up and right on the stick
        touches={{60,steer},{61,point(layout.gas)}};
        input.update(1280,720);
        assert(input.player().move_x>.5F && input.player().move_y==0 && input.player().gas==1 && input.player().brake_pedal==0);
        touches={{60,steer},{62,point(layout.brake_pedal)}};
        input.update(1280,720);
        assert(input.player().brake_pedal==1 && input.player().gas==0);
        release();
        touches={{63,{pan.x+pan.width*.5F,pan.y+pan.height*.5F}}};
        input.update(1280,720); assert(input.player().look_dx==0);
        touches={{63,{pan.x+pan.width*.5F+60,pan.y+pan.height*.5F}}};
        input.update(1280,720); assert(std::abs(input.player().look_dx-60*PI/720)<1e-4F && input.player().look_x==0 && !input.player().fire);
        release();
        touches={{64,point(layout.fire)}};
        input.update(1280,720); assert(!input.player().fire); // no guns at the wheel
        release();
        for (const auto* pedal : {&layout.gas, &layout.brake_pedal}) assert(!in_pan(point(*pedal)) && point(*pedal).y>pan.y+pan.height);
        touches={{65,point(layout.interact)}};
        input.update(1280,720); assert(input.player().interact); // EXIT
        release();
    }
    input.set_driving(false);
    input.update(1280,720);
    touches={{41,point(layout.weapon)}};
    input.update(1280,720); assert(input.player().next_weapon);
    input.update(1280,720); assert(!input.player().next_weapon);
    release();
    // The stick moves, a finger on the right of the screen swipes the view (by how far it slides:
    // half the screen's height is 90 degrees), and both keep their roles after array reordering
    // and while crossing a button.
    auto move=point(layout.movement); move.x+=100;
    const Vector2 look{760,520};
    touches={{50,move},{12,look}};
    input.update(1280,720);
    assert(input.player().move_x>.5F && input.player().look_dx==0 && input.player().look_x==0 && !input.player().sprint);
    touches={{12,{look.x+72,look.y-36}},{50,move}};
    input.update(1280,720);
    assert(std::abs(input.player().look_dx-72*PI/720)<1e-4F && std::abs(input.player().look_dy+36*PI/720)<1e-4F && input.player().move_x>.5F);
    input.update(1280,720); assert(input.player().look_dx==0); // a still finger does not turn
    touches={{12,point(layout.fire)},{50,move}};
    input.update(1280,720);
    assert(input.player().look_dx>0 && input.player().move_x>.5F && !input.player().fire);
    touches={{12,point(layout.fire)}};
    input.update(1280,720); assert(input.player().move_x==0 && !input.player().fire);
    release();
    // Thumb on FIRE: it shoots and, sliding, steers the sights.
    touches={{13,point(layout.fire)}}; input.update(1280,720); assert(input.player().fire && input.player().look_dx==0);
    touches[0].position.x-=40; input.update(1280,720); assert(input.player().fire && input.player().look_dx<0);
    release();
    // Sensitivity and invert apply to swipes.
    input.set_look({.5F,1,true,false});
    touches={{14,look}}; input.update(1280,720);
    touches[0].position.y+=72; input.update(1280,720);
    assert(std::abs(input.player().look_dy+.5F*72*PI/720)<1e-4F);
    input.set_look({});
    release();
    // A thumb put down anywhere in the lower left (off the buttons) makes a floating stick there.
    touches={{15,{420,380}}}; input.update(1280,720); assert(input.player().move_x==0);
    assert(std::abs(input.drawn_layout(1280,720).movement.x-420.0F/1280)<1e-4F);
    touches[0].position.x+=60; input.update(1280,720); assert(input.player().move_x>.3F && input.player().look_dx==0);
    release();
    assert(input.drawn_layout(1280,720).movement.x==layout.movement.x);
    input.set_button_size(1.2F); assert(std::abs(input.drawn_layout(1280,720).fire.scale-layout.fire.scale*1.2F)<1e-5F);
    input.set_button_size(1);
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
    // Keyboard and mouse as in PC shooters: the mouse looks only while captured, then the right
    // button aims, the left fires and the wheel swaps weapons.
    walking=sprinting=false; right_down=true; key_weapon=key_reload=true;
    input.update(1280,720); assert(!input.player().aim && input.player().reload && input.player().next_weapon);
    input.set_touch_controls(false);
    right_down=false; input.set_mouse_look(true,{}); input.update(1280,720);
    right_down=true; input.set_mouse_look(true,{100,-50});
    input.update(1280,720); assert(input.player().aim && input.player().reload && input.player().next_weapon);
    assert(std::abs(input.player().look_dx-.12F)<1e-5F && std::abs(input.player().look_dy+.06F)<1e-5F);
    input.update(1280,720,false); assert(!input.player().aim && !input.player().reload && !input.player().next_weapon);
    right_down=key_weapon=key_reload=false;
    mouse_down=true; input.update(1280,720); assert(input.player().fire);
    // The click on RESUME (or the browser's click that captures the mouse) does not shoot.
    input.cancel_controls(); input.update(1280,720); assert(!input.player().fire);
    mouse_down=false; input.update(1280,720); mouse_down=true; input.update(1280,720); assert(input.player().fire);
    input.set_mouse_look(false,{}); input.update(1280,720);
    input.set_mouse_look(true,{}); input.update(1280,720); assert(!input.player().fire);
    mouse_down=false; input.update(1280,720); mouse_down=true; input.update(1280,720); assert(input.player().fire);
    mouse_down=false; wheel=-1; input.update(1280,720); assert(input.player().next_weapon && !input.player().fire);
    wheel=0;
    // AIM as a toggle: a press turns the sights on, the next off.
    input.set_look({1,1,false,true});
    right_down=right_pressed=true; input.update(1280,720); assert(input.player().aim);
    right_pressed=false; input.update(1280,720); assert(input.player().aim);
    right_down=false; input.update(1280,720); assert(input.player().aim); // stays on after the release
    right_down=right_pressed=true; input.update(1280,720); assert(!input.player().aim);
    right_down=right_pressed=false;
    input.set_mouse_look(false,{}); input.update(1280,720); assert(!input.player().fire && input.player().look_dx==0);
    input.set_look({});
    // Rebinding: F fires; F no longer also works USE.
    auto bindings=outland::input::Bindings::defaults();
    assert(bindings[outland::input::Action::Interact][1]==KEY_F);
    bindings.bind(outland::input::Action::Fire,1,KEY_F);
    assert(bindings[outland::input::Action::Interact][1]==0 && bindings[outland::input::Action::Fire][1]==KEY_F);
    assert(outland::input::Bindings::name(bindings[outland::input::Action::Fire][0])=="LEFT MOUSE" && outland::input::Bindings::name(KEY_SPACE)=="SPACE");
    input.set_bindings(bindings);
    key_f=true; input.update(1280,720); assert(input.player().fire && !input.player().interact); key_f=false;
    std::cout<<"[PASS] Swipe look, floating stick, mouse look, bindings, stable touch ownership, actions, mouse deduplication and Creator/modal isolation\n";
}
