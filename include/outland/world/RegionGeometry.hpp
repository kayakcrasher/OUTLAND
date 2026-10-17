#pragma once
#include "outland/world/Building.hpp"
#include "outland/world/Road.hpp"
#include <raylib.h>

// The procedural world's drawing, one piece at a time and at an explicit level of detail, so it can
// be recorded once into static meshes (engine::GeometryRecorder) instead of rebuilt every frame.
namespace outland::world::geometry {
void building(const Building& building, bool detailed);   // detailed: within 90 m; without the front door
void building_door(const Building& building);              // the front door, as open as it is now
void tree(Vector3 position, int lod);                     // 0 within 65 m, 1 within 140 m, 2 beyond
int road_segments(const Road& road);                      // 2 m ribbon segments
void road(const Road& road, int begin, int end);          // segments [begin, end)
}
