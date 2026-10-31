#pragma once
#include "outland/world/sky/DayNight.hpp"
#include <raylib.h>

// The world's light: the sun and the sky light shade every model by which way its faces point
// (the sun's side bright and warm, the side away lit only by the sky, undersides by the ground),
// and the air hazes everything into the horizon's colour with distance, so far hills and towns
// fade into the sky instead of standing flat against it. Both follow the time of day.
//
// Two shaders: one lit, for models with normals (terrain, buildings, props, vehicles); one that
// only hazes, for geometry without normals (anything drawn shape by shape, and the recorded road
// and building batches). Characters keep raylib's own shader (it does their skinning).
// OUTLAND_SHADING=0 turns it off; if the shaders don't compile, everything draws as before.
namespace outland::engine::render {

// Compile the shaders (after the window is open).
void shading_initialize();
void shading_shutdown();
[[nodiscard]] bool shading_enabled();
// Give a freshly loaded model the lit shader (or the haze one, if a mesh has no normals) and
// mipmapped, trilinear-filtered textures, so distant surfaces don't shimmer.
void shade_model(Model& model);
// The haze-only shader for a material drawn without normals.
void shade_flat(Material& material);
// Once a frame, before the world is drawn: the sky now and where the eye is.
void shading_frame(const world::sky::SkyState& sky);
// Around the world pass: shapes drawn one by one get the haze too.
void shading_begin();
void shading_end();

} // namespace outland::engine::render
