#include "outland/engine/render/ViewFrustum.hpp"
#include "outland/assets/RaylibContract.hpp"
#include "outland/characters/CharacterRenderer.hpp"
#include <raymath.h>
#include "outland/characters/AnimationAssetCatalog.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
namespace outland::characters {
namespace {
// Some exports keep bones in a different space from the skinned vertices (the glTF mesh node's
// transform is baked into vertices only); others - rigid PSX pieces parented to bones - already
// share one space. Pick whichever puts the bind skeleton inside the body it deforms.
bool skeleton_needs_skin_space(const Model& model,const Matrix& skin) {
    if(!model.skeleton.bindPose || model.skeleton.boneCount<=0) return false;
    Vector3 lo{1e30F,1e30F,1e30F},hi{-1e30F,-1e30F,-1e30F};
    bool any=false;
    for(int m=0;m<model.meshCount;++m) {
        const auto& mesh=model.meshes[m];
        if(!mesh.vertices) continue;
        for(int v=0;v<mesh.vertexCount;++v) {
            const Vector3 p{mesh.vertices[v*3],mesh.vertices[v*3+1],mesh.vertices[v*3+2]};
            lo=Vector3Min(lo,p);hi=Vector3Max(hi,p);any=true;
        }
    }
    if(!any) return true; // keep the established behaviour when vertices are not on the CPU
    // Compare skeleton size with body size: the right space differs by ~1x, the wrong one by ~100x.
    const float body=Vector3Distance(lo,hi);
    const auto mismatch=[&](bool transformed) {
        Vector3 a{1e30F,1e30F,1e30F},b{-1e30F,-1e30F,-1e30F};
        for(int i=0;i<model.skeleton.boneCount;++i) {
            auto p=model.skeleton.bindPose[i].translation;
            if(transformed) p=Vector3Transform(p,skin);
            a=Vector3Min(a,p);b=Vector3Max(b,p);
        }
        const float size=Vector3Distance(a,b);
        return size>0 && body>0 ? std::abs(std::log(size/body)) : 1e9F;
    };
    return mismatch(true)<mismatch(false);
}
}
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
void CharacterRenderer::draw(const std::string& id,Vector3 feet,float yaw,float scale,const AnimationController* animation,
                             const BodyPose& body,HeldGun* held) {
    if(held) *held={};
    const auto* asset=registry_.find(id);
    if (!asset || asset->pool==CharacterPool::Arms || failed_.contains(id) ||
        !std::isfinite(feet.x) || !std::isfinite(feet.y) || !std::isfinite(feet.z) ||
        !std::isfinite(yaw) || !std::isfinite(scale) || scale<=0) return;
    const auto path=(std::filesystem::path(asset_root_)/asset->model_path).string();
    const auto info=std::find_if(animation_asset_catalog.begin(),animation_asset_catalog.end(),[&](const auto& entry){return entry.id==id;});
    const bool load_clips=animation && info!=animation_asset_catalog.end() && info->mapped_clips>0;
    auto* model=models_.load(path,[&](Model& value){return prepare(*asset,value);},load_clips);
    if (!model) {
        if(models_.deferred()) return;
        failed_.insert(id);
        TraceLog(LOG_ERROR,"OUTLAND character unavailable: %s (%s)",id.c_str(),path.c_str());
        return; // No placeholder body or procedural fallback.
    }
    // Clips are loaded only from this model: raylib 6 clips no longer carry rig topology.
    // Never retarget by bone count alone; the generated audit describes source rigs.
    const auto clips=models_.animations(model);
    const auto sample=animation ? animation->sample(*model,clips) : AnimationSample{};
    bool posed=false;
    if(animation && sample.fallback) {
        auto space=skin_spaces_.find(path);
        if(space==skin_spaces_.end()) {
            Matrix skin=MatrixIdentity();
            const bool found=world::physics::skinned_mesh_transform(path,skin) && skeleton_needs_skin_space(*model,skin);
            space=skin_spaces_.emplace(path,std::pair{found,skin}).first;
        }
        // Skeleton space -> world exactly as the mesh is drawn: the mesh-node transform (when the
        // bones need it), the model's preparation, then its placement.
        const Matrix* skin=space->second.first ? &space->second.second : nullptr;
        const auto placement=MatrixMultiply(MatrixMultiply(MatrixScale(scale,scale,scale),MatrixRotateY((yaw+asset->facing_degrees)*DEG2RAD)),
            MatrixTranslate(feet.x,feet.y,feet.z));
        const auto world=MatrixMultiply(MatrixMultiply(skin ? *skin : MatrixIdentity(),model->transform),placement);
        const float metres=Vector3Distance(Vector3Transform({1,0,0},world),Vector3Transform({0,0,0},world))/scale;
        // A crowd shares one controller: a body already posed exactly so keeps its bones.
        BodyPose shape=body;shape.outfit=0;
        const PoseKey key{animation->action(),animation->gait_cycle(),animation->ground_speed(),animation->elapsed(),metres,shape};
        auto last=last_pose_.find(model);
        if(!held && last!=last_pose_.end() && same_pose(last->second,key)) posed=true;
        else {
            posed=gait_.apply(*model,animation->action(),animation->gait_cycle(),animation->ground_speed(),asset->facing_degrees,skin,
                body,static_cast<float>(animation->elapsed()),metres);
            if(posed) last_pose_[model]=key; else last_pose_.erase(model);
        }
        if(posed && held && gait_.holding()) {
            const auto grip=Vector3Transform(gait_.grip(),world);
            const auto toward=[&](Vector3 d){return Vector3Normalize(Vector3Subtract(Vector3Transform(Vector3Add(gait_.grip(),d),world),grip));};
            held->held=true;held->grip=grip;held->aim=toward(gait_.aim());held->up=toward(gait_.gun_up());
        }
        if(posed && held && gait_.crown_valid()) {
            const auto crown=Vector3Transform(gait_.crown(),world);
            const auto toward=[&](Vector3 d){return Vector3Normalize(Vector3Subtract(Vector3Transform(Vector3Add(gait_.crown(),d),world),crown));};
            held->hat=true;held->crown=crown;held->crown_up=toward(gait_.crown_up());held->crown_forward=toward(gait_.crown_forward());
        }
    }
    if(!posed) {posed=AnimationController::apply_sample(*model,clips,sample,models_.posed(model));last_pose_.erase(model);}
    models_.mark_posed(model,posed);
    // An outfit: the same body in other clothes (its texture swapped in for this draw only).
    std::vector<unsigned int> own;
    if(body.outfit) for(int i=0;i<model->materialCount;++i) {
        auto& map=model->materials[i].maps[MATERIAL_MAP_ALBEDO];
        own.push_back(map.texture.id);
        if(map.texture.id) map.texture.id=body.outfit;
    }
    DrawModelEx(*model,feet,{0,1,0},yaw+asset->facing_degrees,{scale,scale,scale},WHITE);
    for(std::size_t i=0;i<own.size();++i) model->materials[i].maps[MATERIAL_MAP_ALBEDO].texture.id=own[i];
}
HeldGun CharacterRenderer::draw_player(Vector3 feet,float yaw,const BodyPose& body) {
    HeldGun held;
    const auto* asset=player_id_.empty() ? nullptr : registry_.find(player_id_);
    if (!asset) asset=registry_.player();
    if (asset) draw(asset->id,feet,yaw*RAD2DEG,1,&player_animation_,body,&held);
    return held;
}
void CharacterRenderer::draw_npcs(const NpcSystem& npcs,Vector3 camera,const engine::ViewFrustum* view,const Dress& dress,const Armed& armed) {
    (void)camera; // NpcSystem owns configurable visibility/activation distances.
    for (const auto& actor:npcs.actors()) {
        if(!actor.visible || actor.hidden || (view && !view->sphere({actor.position.x,actor.position.y+1,actor.position.z},2))) continue;
        BodyPose body;
        if(actor.pool==CharacterPool::Creature) body.style=BodyStyle::Zombie;
        if(dress) dress(actor,body);
        HeldGun held;
        draw(actor.character_id,actor.position,actor.yaw_degrees,1,&actor.animation,body,&held);
        if((held.held || held.hat) && armed) armed(actor,held);
    }
}
}
