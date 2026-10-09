#include "outland/dev/DevLab.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <cassert>
#include <iostream>
#include <vector>
namespace {
int key = 0;
struct Touch {
    int id;
    Vector2 p;
};
std::vector<Touch> touches;
bool focused = true;
} // namespace
extern "C" {
bool __wrap_IsKeyPressed(int k) { return key == k; }
int __wrap_GetTouchPointCount() { return static_cast<int>(touches.size()); }
int __wrap_GetTouchPointId(int i) { return touches[i].id; }
Vector2 __wrap_GetTouchPosition(int i) { return touches[i].p; }
bool __wrap_IsWindowFocused() { return focused; }
bool __wrap_IsMouseButtonPressed(int) { return false; }
Vector2 __wrap_GetMousePosition() { return {}; }
}
int main() {
    using namespace outland;
    dev::DevLab lab;
    world::VerdaRegion region;
    const auto press = [&](int k) {
        key = k;
        lab.update();
        key = 0;
    };
    lab.begin_builder();assert(lab.building());
    lab.toggle_build();assert(!lab.building());
    lab.begin_builder();assert(lab.building() && !lab.tools_open());
    press(KEY_ONE);
    auto p = lab.spawn_position(region);
    assert(lab.teleport_requested() && p.x == 0 && p.z == 8);
    lab.clear_teleport();
    key=KEY_TWO;lab.update(1280,720,false);key=0;assert(!lab.teleport_requested());
    press(KEY_TWO);
    p = lab.spawn_position(region);
    assert(p.x == 0 && p.z == -1790);
    press(KEY_EIGHT);
    p = lab.spawn_position(region);
    assert(p.x == 1750 && p.z == -390);
    press(KEY_NINE);
    p = lab.spawn_position(region);
    assert(p.x == 300 && p.z == 1810);
    press(KEY_ZERO);
    p = lab.spawn_position(region);
    assert(p.x == -1750 && p.z == 460);
    press(KEY_F2);
    assert(lab.take_build_toggle() && !lab.take_build_toggle());
    press(KEY_F3);
    assert(lab.take_vehicle_spawn() && !lab.take_vehicle_spawn());
    press(KEY_F4);
    assert(lab.take_return() && !lab.take_return());
    touches = {{72, {640, 22}}};
    lab.update(1280, 720);
    assert(lab.tools_open() && lab.owns_point({10, 700}, 1280, 720));
    // A held contact cannot invoke the newly opened panel, even after moving over an action.
    touches[0].p = {450, 100};
    lab.update(1280, 720);
    assert(!lab.take_build_toggle());
    touches.clear();
    lab.update(1280, 720);
    touches = {{81, {450, 100}}};
    lab.update(1280, 720);
    assert(lab.take_build_toggle() && !lab.tools_open());
    lab.update(1280, 720);
    assert(!lab.take_build_toggle());
    touches.clear();
    lab.update(1280, 720);
    focused = false;
    touches = {{99, {640, 22}}};
    lab.update(1280, 720);
    focused = true;
    lab.update(1280, 720);
    assert(!lab.tools_open());
    // All travel buttons, including CLOSE, fit a short letterboxed X11 viewport.
    touches.clear();lab.update(640,240);
    touches={{201,{320,22}}};lab.update(640,240);assert(lab.tools_open());
    touches.clear();lab.update(640,240);
    const float panel=240.0F/350.0F;
    touches={{202,{320-100*panel,(82+4*54+22)*panel}}};lab.update(640,240);
    assert(!lab.tools_open());
    std::cout << "[PASS] Capital/coastal DEV destinations, build/spawn/return actions, modal "
                 "reservation, stable touch ownership and focus quarantine\n";
}
