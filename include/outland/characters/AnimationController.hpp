#pragma once
#include <raylib.h>
#include <span>
#include <string_view>
namespace outland::characters {
enum class AnimationAction { None, Idle, Walk, Run, Attack, Death };
struct AnimationSample { int clip{-1}, frame{0}; bool fallback{true}; };
class AnimationController {
public:
    void advance(AnimationAction action,float dt);
    void restart() { elapsed_=0; }
    AnimationSample sample(const Model& model,std::span<const ModelAnimation> clips) const;
    AnimationAction action() const { return action_; }
    double elapsed() const { return elapsed_; }
    // Returns whether the shared model now has a sampled pose; false restores bind pose.
    static bool apply_sample(Model& model,std::span<const ModelAnimation> clips,AnimationSample sample,bool was_posed);
    static AnimationAction classify(std::string_view name);
    static bool compatible(const Model& model,const ModelAnimation& clip);
private:
    AnimationAction action_{AnimationAction::Idle};
    double elapsed_{0};
};
}
