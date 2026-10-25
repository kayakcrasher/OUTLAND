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
// The crown of the head too (`hat`): where a hat sits, its up and where the face points.
struct HeldGun {
    bool held{false}; Vector3 grip{}, aim{0,0,1}, up{0,1,0};
    bool hat{false}; Vector3 crown{}, crown_up{0,1,0}, crown_forward{0,0,1};
};
// Own this inside Renderer::run so resources die while the graphics context is live.
class CharacterRenderer {
public:
    CharacterRenderer(const CharacterRegistry& registry,std::string asset_root);
    void begin_frame() { models_.begin_frame(1); }
    void draw(const std::string& id,Vector3 feet,float yaw_degrees,float scale=1.0F,const AnimationController* animation=nullptr,
              const BodyPose& body={},HeldGun* held=nullptr);
    void update_player(AnimationAction action,float dt,float ground_speed=-1) { player_animation_.advance(action,dt,ground_speed); }
    HeldGun draw_player(Vector3 feet,float yaw_radians,const BodyPose& body={});
    // The body the player is drawn in (clothes from Vestaĵejo Verda); empty: the registry's player.
    void set_player(std::string id) { player_id_=std::move(id); }
    // `dress` says what each body holds (zombies are styled here); `armed` draws what they hold
    // (and what they wear: called when a gun is held or the crown of the head is known).
    using Dress=std::function<void(const NpcInstance&,BodyPose&)>;
    using Armed=std::function<void(const NpcInstance&,const HeldGun&)>;
    void draw_npcs(const NpcSystem& npcs,Vector3 camera_position,const engine::ViewFrustum* view = nullptr,
                   const Dress& dress={},const Armed& armed={});
    static bool prepare(const CharacterDefinition& definition,Model& model);
private:
    AnimationController player_animation_;
    std::string player_id_;
    SkeletalGait gait_;
    // How each model's bones were last posed by the gait (so a body drawn again alike is not re-posed).
    struct PoseKey { AnimationAction action; double cycle; float speed; double elapsed; float metres; BodyPose body; };
    static bool same_pose(const PoseKey& a,const PoseKey& b) {
        const auto& p=a.body;const auto& q=b.body;
        return a.action==b.action && a.cycle==b.cycle && a.speed==b.speed && a.elapsed==b.elapsed && a.metres==b.metres &&
            p.hold==q.hold && p.style==q.style && p.aim_pitch==q.aim_pitch && p.crouch==q.crouch && p.stock==q.stock &&
            p.support==q.support && p.seated==q.seated && p.skydive==q.skydive && p.hang==q.hang && p.body_pitch==q.body_pitch &&
            p.body_roll==q.body_roll && p.hands_up==q.hands_up && p.cuffed==q.cuffed && p.downed==q.downed &&
            p.play==q.play && p.play_phase==q.play_phase;
    }
    std::unordered_map<const Model*,PoseKey> last_pose_;
    const CharacterRegistry& registry_;
    std::string asset_root_;
    assets::ModelCache models_{64};
    std::unordered_set<std::string> failed_;
    std::unordered_map<std::string,std::pair<bool,Matrix>> skin_spaces_; // glTF mesh-node transform per model
};
}
