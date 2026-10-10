#pragma once
#include <raylib.h>
#include <span>
#include <string_view>
namespace outland::characters {
enum class AnimationAction { None, Idle, Walk, Run, Attack, Death };
struct AnimationSample { int clip{-1}, frame{0}; bool fallback{true}; };
class AnimationController {
public:
    // `ground_speed` (m/s, <0 = unknown) drives the procedural gait's cadence and stride so feet
    // keep pace with the body; the gait cycle is continuous across walk/run changes.
    void advance(AnimationAction action,float dt,float ground_speed=-1);
    void restart() { elapsed_=0; }
    AnimationSample sample(const Model& model,std::span<const ModelAnimation> clips) const;
    AnimationAction action() const { return action_; }
    double elapsed() const { return elapsed_; }
    double gait_cycle() const { return cycle_; }
    float ground_speed() const { return speed_; }
    // Steps per second grow with speed; cycles/second for a given ground speed.
    static float gait_cadence(float ground_speed);
    static float nominal_speed(AnimationAction action);
    // Returns whether the shared model now has a sampled pose; false restores bind pose.
    static bool apply_sample(Model& model,std::span<const ModelAnimation> clips,AnimationSample sample,bool was_posed);
    static AnimationAction classify(std::string_view name);
    static bool compatible(const Model& model,const ModelAnimation& clip);
private:
    AnimationAction action_{AnimationAction::Idle};
    double elapsed_{0}, cycle_{0};
    float speed_{-1};
};
}
