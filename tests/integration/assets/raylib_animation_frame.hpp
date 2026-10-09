#pragma once
#include <raylib.h>
#include <type_traits>
namespace outland::test {
using AnimationFrame = float;
static_assert(std::is_same_v<decltype(&UpdateModelAnimation), void (*)(Model, ModelAnimation, float)>);
static_assert(RAYLIB_VERSION_MAJOR == 6, "OUTLAND requires the raylib 6 API");
}
