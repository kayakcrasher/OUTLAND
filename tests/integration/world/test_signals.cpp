// Traffic signals that work: found at every signalled crossing (25 across the five towns), never green
// both ways, an all-red between the phases, the boulevards' longer green and green wave, and the
// rule drivers follow (stop on red; on amber only with room to).
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/roads/Signals.hpp"
#include <cmath>
#include <iostream>
#include <string>

using namespace outland;
namespace roads = world::roads;
namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
}

int main() {
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    const auto signals = roads::find_signals(region);
    // Downtown Verda City's nine crossings, and four in the middle of each of the other four towns.
    check(signals.size() == 25, "a signal at each signalled crossing: " + std::to_string(signals.size()));
    for (const auto& s : signals) {
        const std::string name = "the signal at " + std::to_string(static_cast<int>(s.centre.x)) + "," + std::to_string(static_cast<int>(s.centre.z));
        check(s.poles.size() == 2 && s.stop_x > 7 && s.stop_z > 7 && s.stop_x < 14 && s.stop_z < 14, name + ": two poles, stop lines at the crossing's edge");
        const float cycle = roads::signal_cycle(s);
        check(std::abs(cycle - 50) < .01F, name + ": a 50 s cycle");
        // Never green (or amber) both ways at once; each way gets its green; an all-red between.
        bool both = false, all_red = false;
        float green_x = 0, green_z = 0;
        for (float t = 0; t < cycle; t += .1F) {
            const auto x = roads::aspect(s, false, t), z = roads::aspect(s, true, t);
            both |= x != roads::Aspect::Red && z != roads::Aspect::Red;
            all_red |= x == roads::Aspect::Red && z == roads::Aspect::Red;
            green_x += x == roads::Aspect::Green ? .1F : 0;
            green_z += z == roads::Aspect::Green ? .1F : 0;
        }
        check(!both, name + ": never open both ways at once");
        check(all_red, name + ": an all-red to clear the junction");
        // The boulevards get the longer green.
        const bool ns = std::abs(s.centre.x) < 2, ew = std::abs(s.centre.z) < 2;
        if (ns && !ew) check(green_z > green_x + 8, name + ": the north-south boulevard's long green");
        if (ew && !ns) check(green_x > green_z + 8, name + ": the east-west boulevard's long green");
        if (ns && ew) check(std::abs(green_x - green_z) < .5F, name + ": the boulevards share it evenly");
    }
    // The green wave: driving down the north-south boulevard at 10 m/s, each light along it turns
    // green as much later as the drive takes.
    {
        const roads::Signal *a = nullptr, *b = nullptr;
        for (const auto& s : signals)
            if (std::abs(s.centre.x) < 2 && std::abs(s.centre.z + 84) < 2) a = &s;
            else if (std::abs(s.centre.x) < 2 && std::abs(s.centre.z - 84) < 2) b = &s;
        check(a && b, "two signals along the boulevard");
        if (a && b) {
            const auto green_starts = [](const roads::Signal& s) {
                for (float t = 0; t < 50; t += .05F)
                    if (roads::aspect(s, true, t) == roads::Aspect::Green && roads::aspect(s, true, t - .05F) != roads::Aspect::Green) return t;
                return -1.0F;
            };
            const float lag = std::fmod(green_starts(*b) - green_starts(*a) + 100, 50.0F);
            check(std::abs(lag - 16.8F) < .5F, "the green wave: 168 m on, green 16.8 s later (" + std::to_string(lag) + ")");
        }
    }
    // A driver coming up to a signal: it's found ahead, in the right lane and direction, not behind.
    {
        const auto& s = signals.front();
        roads::SignalAhead ahead;
        const Vector3 north{s.centre.x + 3, 0, s.centre.z - 40}, along{0, 0, 1};
        check(roads::signal_ahead(signals, north, along, 0, 60, ahead) && ahead.signal == &s, "the signal ahead is found");
        check(std::abs(ahead.distance - (40 - s.stop_z)) < .01F, "its stop line, " + std::to_string(ahead.distance) + " m on");
        check(!roads::signal_ahead(signals, {s.centre.x + 3, 0, s.centre.z + 20}, along, 0, 60, ahead) || ahead.signal != &s, "not once past it");
        check(!roads::signal_ahead(signals, {s.centre.x + 40, 0, s.centre.z - 40}, along, 0, 30, ahead), "nor from another street");
        // The rule: red, stop; amber, stop only with room to.
        check(roads::must_stop({&s, 30, roads::Aspect::Red, 5}, 12), "stop on red");
        check(roads::must_stop({&s, 40, roads::Aspect::Amber, 2}, 10), "stop on amber with room to");
        check(!roads::must_stop({&s, 5, roads::Aspect::Amber, 2}, 12), "go on through an amber too close to stop");
        check(!roads::must_stop({&s, 20, roads::Aspect::Green, 9}, 12), "go on green");
    }
    if (failures) return 1;
    std::cout << "[PASS] traffic signals\n";
    return 0;
}
