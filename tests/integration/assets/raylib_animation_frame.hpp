#pragma once
#include <raylib.h>

// raylib 5.5 uses int frames; newer releases use float. Match the installed API
// when defining test mocks instead of guessing from a version number.
namespace outland::test {
template <typename> struct AnimationFrameArgument;
template <typename Result, typename ModelArg, typename AnimationArg, typename FrameArg>
struct AnimationFrameArgument<Result (*)(ModelArg, AnimationArg, FrameArg)> {
    using type = FrameArg;
};
using AnimationFrame = AnimationFrameArgument<decltype(&UpdateModelAnimation)>::type;
}
