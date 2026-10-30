#pragma once
#include <raylib.h>
#include <cstdint>
#include <vector>

namespace outland::world { class VerdaRegion; }

// Traffic signals that work. At each signalled crossing (25: downtown Verda City's nine and four in
// each other town, each with a pair of signal poles on opposite corners) the lights run a fixed
// cycle: the main road green, amber, all red, the cross road green, amber, all red. The boulevards get the longer green and the lights
// along each one are timed as a green wave (a car at 36 km/h meets one green after another); where
// the two boulevards cross they share the cycle evenly. The all-red lets the junction clear before
// the other road goes. Island traffic stops at the line on red, and on amber unless it is too close
// to stop.
namespace outland::world::roads {

enum class Aspect : std::uint8_t { Green, Amber, Red };

struct Signal {
    Vector3 centre{};
    float stop_x{9}, stop_z{9};      // the stop line's distance from the centre, for traffic along x / along z
    float half_x{8}, half_z{8};      // half the width of the road along x / along z (who the signal is for)
    bool main_z{true};               // the main road runs along z
    float main_green{20}, cross_green{20};
    float offset{0};                 // seconds: the green wave
    std::vector<std::pair<Vector3, float>> poles;   // the signal poles (position, yaw)
};

inline constexpr float signal_amber = 3, signal_all_red = 2;
// One full cycle: both greens, both ambers and both all-reds.
inline float signal_cycle(const Signal& s) { return s.main_green + s.cross_green + 2 * (signal_amber + signal_all_red); }

// The signalled crossings: found from the signal poles the world kit stands at them.
std::vector<Signal> find_signals(const VerdaRegion& region);
// What a signal shows traffic travelling along z (or along x), at `time` seconds on the signal clock.
Aspect aspect(const Signal& signal, bool along_z, double time);

struct SignalAhead {
    const Signal* signal{nullptr};
    float distance{0};               // to the stop line
    Aspect aspect{Aspect::Green};
    float until_change{0};           // seconds until this aspect changes
};
// The next signal a vehicle at `position` heading `forward` comes to within `range` metres.
bool signal_ahead(const std::vector<Signal>& signals, Vector3 position, Vector3 forward, double time, float range, SignalAhead& out);
// Should a driver stop for it? Red: yes. Amber: yes, unless too close to stop at a comfortable 3.5 m/s².
bool must_stop(const SignalAhead& ahead, float speed);

} // namespace outland::world::roads
