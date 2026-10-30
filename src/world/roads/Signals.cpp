#include "outland/world/roads/Signals.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <algorithm>
#include <cmath>

namespace outland::world::roads {

std::vector<Signal> find_signals(const VerdaRegion& region) {
    // The kit stands two poles at each crossing, on opposite corners: one at (+x, -z) facing +x,
    // one at (-x, +z) facing -x.
    std::vector<std::pair<Vector3, float>> east, west;
    for (const auto& town : region.settlements())
        for (const auto& asset : town.assets) {
            if (asset.model_path.find("Traffic lights/traffic_light.glb") == std::string::npos) continue;
            const float yaw = std::remainder(asset.rotation_y, 360.0F);
            (std::abs(yaw) < 45 ? east : west).emplace_back(asset.position, asset.rotation_y);
        }
    std::vector<Signal> out;
    for (const auto& [a, yaw_a] : east) {
        const std::pair<Vector3, float>* partner = nullptr;
        float best = 40;
        for (const auto& b : west) {
            const float dx = a.x - b.first.x, dz = b.first.z - a.z;
            if (dx <= 0 || dz <= 0) continue;
            const float d = std::hypot(dx, dz);
            if (d < best) { best = d; partner = &b; }
        }
        if (!partner) continue;
        Signal s;
        s.centre = {(a.x + partner->first.x) * .5F, (a.y + partner->first.y) * .5F, (a.z + partner->first.z) * .5F};
        const float ox = (a.x - partner->first.x) * .5F, oz = (partner->first.z - a.z) * .5F;
        // The poles stand just off the corners: the crossing road's edge is a little inside them.
        s.stop_x = ox + .5F; s.stop_z = oz + .5F;
        s.half_x = oz - 1.0F; s.half_z = ox - 1.0F;
        // The boulevards: the north-south one on x = 0, the east-west one on z = 0.
        const bool on_ns = std::abs(s.centre.x) < 2, on_ew = std::abs(s.centre.z) < 2;
        if (on_ns && on_ew) { s.main_z = true; s.main_green = 20; s.cross_green = 20; s.offset = 0; }
        else if (on_ns) { s.main_z = true; s.main_green = 26; s.cross_green = 14; s.offset = -s.centre.z / 10; }
        else if (on_ew) { s.main_z = false; s.main_green = 26; s.cross_green = 14; s.offset = -s.centre.x / 10; }
        else { s.main_z = true; s.main_green = 20; s.cross_green = 20; s.offset = (s.centre.x + s.centre.z) / 20; }
        s.poles = {{a, yaw_a}, *partner};
        out.push_back(std::move(s));
    }
    return out;
}

namespace {
// The aspect, and how long it holds.
Aspect aspect_at(const Signal& s, bool along_z, double time, float& until) {
    const float cycle = signal_cycle(s);
    float t = static_cast<float>(std::fmod(time + s.offset, static_cast<double>(cycle)));
    if (t < 0) t += cycle;
    // The main road's phase first, then the cross road's.
    const bool main = along_z == s.main_z;
    const float start = main ? 0 : s.main_green + signal_amber + signal_all_red;
    const float green = main ? s.main_green : s.cross_green;
    float into = t - start;
    if (into < 0) into += cycle;
    if (into < green) { until = green - into; return Aspect::Green; }
    if (into < green + signal_amber) { until = green + signal_amber - into; return Aspect::Amber; }
    until = cycle - into;
    return Aspect::Red;
}
}

Aspect aspect(const Signal& signal, bool along_z, double time) {
    float until = 0;
    return aspect_at(signal, along_z, time, until);
}

bool signal_ahead(const std::vector<Signal>& signals, Vector3 p, Vector3 forward, double time, float range, SignalAhead& out) {
    bool found = false;
    for (const auto& s : signals) {
        const float rx = p.x - s.centre.x, rz = p.z - s.centre.z;
        // Travelling along one road or the other (not mid-turn across the junction).
        bool along_z;
        float behind, lateral, stop, half;
        if (std::abs(forward.z) > .8F) { along_z = true; behind = -rz * (forward.z > 0 ? 1.0F : -1.0F); lateral = rx; stop = s.stop_z; half = s.half_z; }
        else if (std::abs(forward.x) > .8F) { along_z = false; behind = -rx * (forward.x > 0 ? 1.0F : -1.0F); lateral = rz; stop = s.stop_x; half = s.half_x; }
        else continue;
        if (std::abs(lateral) > half) continue;
        const float distance = behind - stop;
        // Past the line already (in the junction): carry on through.
        if (distance < -1.5F || distance > range) continue;
        if (found && distance >= out.distance) continue;
        float until = 0;
        out = {&s, distance, aspect_at(s, along_z, time, until), until};
        found = true;
    }
    return found;
}

bool must_stop(const SignalAhead& ahead, float speed) {
    if (ahead.aspect == Aspect::Green) return false;
    if (ahead.aspect == Aspect::Red) return ahead.distance > -1.0F;
    // Amber: stop if there's room to at a comfortable rate; otherwise go on through.
    const float v = std::abs(speed);
    return ahead.distance > v * v / (2 * 3.5F) + .5F;
}

} // namespace outland::world::roads
