#pragma once
#include "outland/characters/AnimationController.hpp"
#include <vector>
namespace outland::characters {
// Authored procedural fallback, not a claim that source GLBs contain walk clips.
// Deforms each body's own bind skeleton; unknown rigs keep their bind pose.
class SkeletalGait {
public:
    bool sample(const Model& model,AnimationAction action,double seconds,float facing_degrees);
    const std::vector<Transform>& pose() const {return pose_;}
    bool apply(Model& model,AnimationAction action,double seconds,float facing_degrees);
private:
    std::vector<Transform> pose_;
};
}
