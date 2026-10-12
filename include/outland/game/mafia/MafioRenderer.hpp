#pragma once
#include "outland/game/mafia/Mafio.hpp"
#include <raylib.h>

// La Mafio in the world: each crew's banner (the gang's colour) on a pole by the door of the
// building they took over, and the Green Hand's poker table with its chips and cards. Drawn
// inside BeginMode3D, only near the camera.
namespace outland::game::mafia {
void draw_turf(const Mafio& mafio, Vector3 camera, float time);
}
