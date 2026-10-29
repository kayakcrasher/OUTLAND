#pragma once
#include "outland/world/Road.hpp"
#include "outland/world/roads/HighwayNetwork.hpp"

// Draws a highway road (world/Highways.hpp) from the asset pack: each carriageway's two lanes in
// assets/verda/urban/textures/road4.png (white edge lines, dashed lane line), the median strip and
// hard shoulders in plain asphalt (road3.png), a gravel verge, and a run of Jersey barriers
// (Cones & Barriers/Concrete Barriers/Jersey_barrier.glb) down the middle. Immediate-mode rlgl, so
// StaticWorld records it into static meshes like any other road.
namespace outland::engine::highway {

// Segments [begin, end) of the road (2 m each, as world::geometry::road_segments counts them).
void draw(const world::Road& road, const world::highways::RoadInfo& info, int begin, int end);
// A slip road (RoadType::Ramp: asphalt with white edge lines) or a piece of an interchange
// roundabout's lane (RoadType::Roundabout: drawn as part of a smooth ring, edge lines both sides).
void draw_link(const world::Road& road, int begin, int end);

}
