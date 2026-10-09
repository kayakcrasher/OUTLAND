#pragma once
#include <raylib.h>
#include <type_traits>
namespace outland::assets {
static_assert(RAYLIB_VERSION_MAJOR == 6, "OUTLAND supports the raylib 6 model/skeleton API");
static_assert(
    std::is_same_v<decltype(&UpdateModelAnimation), void (*)(Model, ModelAnimation, float)>);
static_assert(std::is_same_v<decltype(Model{}.skeleton), ModelSkeleton>);
} // namespace outland::assets
