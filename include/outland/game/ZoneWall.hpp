#pragma once
#include <raylib.h>

// The Battle Royale circle as a physical wall in the world: a curtain of blue light standing on
// the ground all the way round, tall enough to see from a helicopter, fading upward, with bright
// bands at its foot and crest and streaks that drift up it (and race when the wall is moving), so
// a player sees where the safe zone ends from far away (PUBG's blue wall). Only the stretch near
// the camera is drawn. The next circle is a white ring standing on the ground.
namespace outland::game {
// Draws inside BeginMode3D. `center` is (x, z). `menace` 0..1 deepens the colour toward violet
// as the phases go by.
void draw_zone_wall(Vector2 center, float radius, Vector3 camera, float time_seconds, float menace = 0, bool moving = false);
inline void draw_zone_wall(float radius, Vector3 camera, float time_seconds) { draw_zone_wall({0, 0}, radius, camera, time_seconds); }
// The circle the zone will close to next: a thin white ring on the ground near the camera.
void draw_next_circle(Vector2 center, float radius, Vector3 camera, float time_seconds);
// How many wall segments are drawn for a radius (about 12 m each, 64..1440).
int zone_wall_segments(float radius);
}
