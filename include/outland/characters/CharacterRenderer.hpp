#pragma once
#include "outland/assets/ModelCache.hpp"
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/NpcSystem.hpp"
#include <unordered_set>
#include "outland/characters/SkeletalGait.hpp"
namespace outland::characters {
// Own this inside Renderer::run so resources die while the graphics context is live.
class CharacterRenderer {
public:
    CharacterRenderer(const CharacterRegistry& registry,std::string asset_root);
    void draw(const std::string& id,Vector3 feet,float yaw_degrees,float scale=1.0F,const AnimationController* animation=nullptr);
    void update_player(AnimationAction action,float dt) { player_animation_.advance(action,dt); }
    void draw_player(Vector3 feet,float yaw_radians);
    void draw_npcs(const NpcSystem& npcs,Vector3 camera_position);
    static bool prepare(const CharacterDefinition& definition,Model& model);
private:
    AnimationController player_animation_;
    SkeletalGait gait_;
    const CharacterRegistry& registry_;
    std::string asset_root_;
    assets::ModelCache models_{64};
    std::unordered_set<std::string> failed_;
};
}
