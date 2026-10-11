#pragma once
#include <raylib.h>

// The Battle Royale circle as a physical wall in the world: a curtain of blue light standing on
// the ground all the way round, fading upward, its streaks drifting, so a player sees where the
// safe zone ends from far away (PUBG's blue wall). Only the stretch near the camera is drawn.
namespace outland::game {
// Draws inside BeginMode3D. The circle is centred on the world origin.
void draw_zone_wall(float radius, Vector3 camera, float time_seconds);
// How many wall segments are drawn for a radius (about 12 m each, 64..1440).
int zone_wall_segments(float radius);
}
