#pragma once
#include "outland/assets/ModelCache.hpp"
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/NpcSystem.hpp"
#include <functional>
#include <unordered_map>
namespace outland::engine { class ViewFrustum; }
#include <unordered_set>
#include "outland/characters/SkeletalGait.hpp"
namespace outland::characters {
// Where a body's hands put its gun this frame (world space): the grip is the gun's model origin.
struct HeldGun { bool held{false}; Vector3 grip{}, aim{0,0,1}, up{0,1,0}; };
// Own this inside Renderer::run so resources die while the graphics context is live.
class CharacterRenderer {
public:
    CharacterRenderer(const CharacterRegistry& registry,std::string asset_root);
    void begin_frame() { models_.begin_frame(1); }
    void draw(const std::string& id,Vector3 feet,float yaw_degrees,float scale=1.0F,const AnimationController* animation=nullptr,
              const BodyPose& body={},HeldGun* held=nullptr);
    void update_player(AnimationAction action,float dt,float ground_speed=-1) { player_animation_.advance(action,dt,ground_speed); }
    HeldGun draw_player(Vector3 feet,float yaw_radians,const BodyPose& body={});
    // `dress` says what each body holds (zombies are styled here); `armed` draws what they hold.
    using Dress=std::function<void(const NpcInstance&,BodyPose&)>;
    using Armed=std::function<void(const NpcInstance&,const HeldGun&)>;
    void draw_npcs(const NpcSystem& npcs,Vector3 camera_position,const engine::ViewFrustum* view = nullptr,
                   const Dress& dress={},const Armed& armed={});
    static bool prepare(const CharacterDefinition& definition,Model& model);
private:
    AnimationController player_animation_;
    SkeletalGait gait_;
    const CharacterRegistry& registry_;
    std::string asset_root_;
    assets::ModelCache models_{64};
    std::unordered_set<std::string> failed_;
    std::unordered_map<std::string,std::pair<bool,Matrix>> skin_spaces_; // glTF mesh-node transform per model
};
}
