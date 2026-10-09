#include "outland/characters/CharacterRenderer.hpp"
#include <raymath.h>
#include "outland/characters/AnimationAssetCatalog.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
namespace outland::characters {
CharacterRenderer::CharacterRenderer(const CharacterRegistry& registry,std::string root)
    :registry_(registry),asset_root_(std::move(root)) {}
bool CharacterRenderer::prepare(const CharacterDefinition& definition,Model& model) {
    const auto basis=definition.z_up ? MatrixRotateX(-PI*.5F) : MatrixIdentity();
    model.transform=basis;
    const auto bounds=assets::transformed_model_bounds(model);
    const float height=bounds.max.y-bounds.min.y;
    if (!std::isfinite(height) || height<=.00001F || !std::isfinite(definition.height) || definition.height<=0) return false;
    const float scale=definition.height/height;
    model.transform=MatrixMultiply(basis,MatrixScale(scale,scale,scale));
    // ModelCache applies the final ground/centre translation after preparation.
    return true;
}
void CharacterRenderer::draw(const std::string& id,Vector3 feet,float yaw,float scale,const AnimationController* animation) {
    const auto* asset=registry_.find(id);
    if (!asset || asset->pool==CharacterPool::Arms || failed_.contains(id) ||
        !std::isfinite(feet.x) || !std::isfinite(feet.y) || !std::isfinite(feet.z) ||
        !std::isfinite(yaw) || !std::isfinite(scale) || scale<=0) return;
    const auto path=(std::filesystem::path(asset_root_)/asset->model_path).string();
    const auto info=std::find_if(animation_asset_catalog.begin(),animation_asset_catalog.end(),[&](const auto& entry){return entry.id==id;});
    const bool load_clips=animation && info!=animation_asset_catalog.end() && info->mapped_clips>0;
    auto* model=models_.load(path,[&](Model& value){return prepare(*asset,value);},load_clips);
    if (!model) {
        failed_.insert(id);
        TraceLog(LOG_ERROR,"OUTLAND character unavailable: %s (%s)",id.c_str(),path.c_str());
        return; // No placeholder body or procedural fallback.
    }
    const auto clips=models_.animations(model);
    const auto sample=animation ? animation->sample(*model,clips) : AnimationSample{};
    models_.mark_posed(model,AnimationController::apply_sample(*model,clips,sample,models_.posed(model)));
    DrawModelEx(*model,feet,{0,1,0},yaw+asset->facing_degrees,{scale,scale,scale},WHITE);
}
void CharacterRenderer::draw_player(Vector3 feet,float yaw) {
    const auto* asset=registry_.player();
    if (asset) draw(asset->id,feet,yaw*RAD2DEG,1,&player_animation_);
}
void CharacterRenderer::draw_npcs(const NpcSystem& npcs,Vector3 camera) {
    (void)camera; // NpcSystem owns configurable visibility/activation distances.
    for (const auto& actor:npcs.actors())
        if(actor.visible) draw(actor.character_id,actor.position,actor.yaw_degrees,1,&actor.animation);
}
}
