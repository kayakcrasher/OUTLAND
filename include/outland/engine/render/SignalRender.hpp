#pragma once
#include "outland/world/roads/Signals.hpp"
#include <vector>

namespace outland::engine::render {

// The traffic signals' lamps, lit by the signal clock: on each pole, the model's own head (facing
// along the pole's x, for the traffic on the road along x) and a second head on the pole facing the
// cross road. Only the lit lamp glows; the others are dark glass.
void draw_signals(const std::vector<world::roads::Signal>& signals, double time, Vector3 camera);

} // namespace outland::engine::render
