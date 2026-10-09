#include "outland/characters/CharacterRenderer.hpp"
#include <raymath.h>
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
void CharacterRenderer::draw(const std::string& id,Vector3 feet,float yaw,float scale) {
    const auto* asset=registry_.find(id);
    if (!asset || asset->pool==CharacterPool::Arms || failed_.contains(id) ||
        !std::isfinite(feet.x) || !std::isfinite(feet.y) || !std::isfinite(feet.z) ||
        !std::isfinite(yaw) || !std::isfinite(scale) || scale<=0) return;
    const auto path=(std::filesystem::path(asset_root_)/asset->model_path).string();
    auto* model=models_.load(path,[&](Model& value){return prepare(*asset,value);});
    if (!model) {
        failed_.insert(id);
        TraceLog(LOG_ERROR,"OUTLAND character unavailable: %s (%s)",id.c_str(),path.c_str());
        return; // No placeholder body or procedural fallback.
    }
    DrawModelEx(*model,feet,{0,1,0},yaw+asset->facing_degrees,{scale,scale,scale},WHITE);
}
void CharacterRenderer::draw_player(Vector3 feet,float yaw) {
    const auto* asset=registry_.player();
    if (asset) draw(asset->id,feet,yaw*RAD2DEG);
}
void CharacterRenderer::draw_npcs(const NpcSystem& npcs,Vector3 camera) {
    constexpr float distance=160.0F;
    for (const auto& actor:npcs.actors()) {
        const auto offset=Vector3Subtract(actor.position,camera);
        if (Vector3LengthSqr(offset)<=distance*distance) draw(actor.character_id,actor.position,actor.yaw_degrees);
    }
}
}
