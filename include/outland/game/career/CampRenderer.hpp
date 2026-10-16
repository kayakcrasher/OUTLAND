#pragma once
#include "outland/game/career/TrainingCamp.hpp"
#include <raylib.h>
#include <functional>

namespace outland::game::career {
// The training camp in the world (inside BeginMode3D): gravel apron, firing line and stand,
// distance posts, the earth berm, the fence and gate, the range hut and flag, and the pop-up
// target. `ground` gives the terrain height at (x, z).
void draw_camp(const TrainingCamp& camp, Vector3 camera, float time, const std::function<float(float, float)>& ground);
}
