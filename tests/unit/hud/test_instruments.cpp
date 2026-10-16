// The compass bearing (north is -z, east +x) and the speedometer's dial range.
#include "outland/game/hud/Instruments.hpp"
#include <cassert>
#include <cmath>
#include <string>

using namespace outland::game::hud;

int main() {
    const auto near = [](float a, float b) { return std::abs(a - b) < .01F; };
    assert(near(bearing({0, 0, -1}), 0));      // toward Espera: north
    assert(near(bearing({1, 0, 0}), 90));      // toward Porto Luma: east
    assert(near(bearing({0, 0, 1}), 180));     // toward Suda Haveno: south
    assert(near(bearing({-1, 0, 0}), 270));    // toward Roka: west
    assert(near(bearing({1, 3, -1}), 45));     // height does not matter
    assert(near(bearing({0, 1, 0}), 0));       // straight up: north by convention
    assert(std::string(point_name(0)) == "N" && std::string(point_name(359)) == "N" && std::string(point_name(44)) == "NE");
    assert(std::string(point_name(180)) == "S" && std::string(point_name(250)) == "W" && std::string(point_name(300)) == "NW");
    // Compass marks: how far round, and where on a 420-px strip (120 degrees across).
    assert(near(relative_bearing(350, 10), 20) && near(relative_bearing(10, 350), -20) && near(relative_bearing(0, 180), -180));
    const auto ahead = strip_spot(90, 100, 420, false);
    assert(ahead.visible && !ahead.pinned && near(ahead.x, 35));
    assert(!strip_spot(0, 120, 420, false).visible);                 // a place behind you is off the strip...
    const auto behind = strip_spot(0, 120, 420, true);               // ...the way to go is pinned to its end
    assert(behind.visible && behind.pinned && near(behind.x, 200));
    assert(strip_spot(0, 240, 420, true).x < 0);                     // round to the left
    assert(dial_limit(30) == 120 && dial_limit(1) == 60 && dial_limit(50) % 20 == 0 && dial_limit(50) >= 50 * 3.6F);
}
