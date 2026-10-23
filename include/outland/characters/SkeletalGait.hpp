#pragma once
#include "outland/characters/AnimationController.hpp"
#include <vector>
namespace outland::characters {
// How a body carries itself on top of its gait: what it holds, where it aims, how low it is.
enum class BodyHold : unsigned char { None, Pistol, Shouldered };
enum class BodyStyle : unsigned char { Person, Zombie };
struct BodyPose {
    BodyHold hold{BodyHold::None};
    BodyStyle style{BodyStyle::Person};
    float aim_pitch{0};   // radians, up positive
    float crouch{0};      // 0 standing .. 1 crouched
    float stock{.30F};    // metres from the shoulder pocket to the gun's grip (shouldered)
    float support{.20F};  // metres from the grip forward to the support hand
    bool seated{false};   // sitting (a driver): thighs forward, knees bent, hands out to a wheel;
                          // the hips keep their standing height, so draw it that far below the seat
    // Skydiving (the Battle Royale drop). `skydive` 0..1: spread-eagle freefall, arms out, knees
    // bent back. `hang` 0..1: under a canopy, hands up on the toggles, legs dangling. The whole
    // body tilts forward `body_pitch` radians about the hips and rolls `body_roll` (banking).
    float skydive{0}, hang{0}, body_pitch{0}, body_roll{0};
    // Arrests: `hands_up` 0..1 raises both hands beside the head (a suspect giving up); `cuffed`
    // 0..1 puts the hands together behind the back.
    float hands_up{0}, cuffed{0};
    // Knocked down (squad play) 0..1: on the knees, slumped forward, hands down on the ground;
    // crawling, the knees shuffle.
    float downed{0};
    // Clothes: a texture drawn in place of the body's own (0 keeps its own) - same UV layout.
    unsigned int outfit{0};
};
// Authored procedural fallback, not a claim that source GLBs contain walk clips.
// Deforms each body's own bind skeleton; unknown rigs keep their bind pose.
class SkeletalGait {
public:
    // `cycle`: gait cycles elapsed (one cycle = a step with each foot). `ground_speed` in m/s sets
    // stride, knee lift, arm swing and lean (<0: the action's nominal speed). `time`: seconds in
    // the current action (breathing, attacks, falling). `metres`: metres per skeleton unit.
    bool sample(const Model& model,AnimationAction action,double cycle,float ground_speed,float facing_degrees,
                const BodyPose& body={},float time=0,float metres=0);
    const std::vector<Transform>& pose() const {return pose_;}
    // `skin`: world transform of the glTF mesh node, which raylib bakes into vertices but not bones.
    bool apply(Model& model,AnimationAction action,double cycle,float ground_speed,float facing_degrees,const Matrix* skin=nullptr,
               const BodyPose& body={},float time=0,float metres=0);
    void to_skin_space(const Model& model,const Matrix& skin);
    // After a sample with a held weapon: the gun's grip (its model origin, on the bore), its
    // pointing direction and its up, in skeleton space.
    bool holding() const { return holding_; }
    Vector3 grip() const { return grip_; }
    Vector3 aim() const { return aim_; }
    Vector3 gun_up() const { return gun_up_; }
    // After a sample: the crown of the head (where a hat sits), its up and where the face points,
    // in skeleton space.
    bool crown_valid() const { return crown_valid_; }
    Vector3 crown() const { return crown_; }
    Vector3 crown_up() const { return crown_up_; }
    Vector3 crown_forward() const { return crown_forward_; }
    // Skeleton space -> the body's own frame in metres: x to its right, y up from the ground,
    // z where it faces, origin on the ground under the hips.
    Vector3 body_point(Vector3 p) const;
    Vector3 body_direction(Vector3 d) const;
private:
    Vector3 right_{1,0,0}, up_{0,1,0}, forward_{0,0,1}, origin_{};
    float metres_{1};
    std::vector<Transform> pose_;
    bool holding_{false};
    Vector3 grip_{}, aim_{}, gun_up_{};
    bool crown_valid_{false};
    Vector3 crown_{}, crown_up_{0,1,0}, crown_forward_{0,0,1};
};
}
