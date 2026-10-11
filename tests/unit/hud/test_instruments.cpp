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
    assert(dial_limit(30) == 120 && dial_limit(1) == 60 && dial_limit(50) % 20 == 0 && dial_limit(50) >= 50 * 3.6F);
}
