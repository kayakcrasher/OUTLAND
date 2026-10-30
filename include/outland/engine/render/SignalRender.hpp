#pragma once
#include "outland/world/roads/Signals.hpp"
#include <vector>

namespace outland::engine::render {

// The traffic signals' lamps, lit by the signal clock: on each pole, the model's own head (facing
// along the pole's x, for the traffic on the road along x) and a second head on the pole facing the
// cross road. Only the lit lamp glows; the others are dark glass. `shine`: only the lit lamps, drawn
// again after the night has darkened the scene, so they stay bright.
void draw_signals(const std::vector<world::roads::Signal>& signals, double time, Vector3 camera, bool shine = false);

} // namespace outland::engine::render
