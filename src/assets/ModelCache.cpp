#include "outland/assets/RaylibContract.hpp"
#include "outland/assets/ModelCache.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <unordered_set>

namespace outland::assets {
namespace {
void release(Model& model) {
    // raylib UnloadModel does not own material textures. Each LoadModel creates
    // its textures; release them once, preserving raylib's shared default texture.
    std::unordered_set<unsigned int> textures;
    for (int material = 0; material < model.materialCount; ++material) {
        if (!model.materials[material].maps) continue;
        for (int map = MATERIAL_MAP_ALBEDO; map <= MATERIAL_MAP_BRDF; ++map) {
            const auto texture = model.materials[material].maps[map].texture;
            if (texture.id && texture.id != rlGetTextureIdDefault() && textures.insert(texture.id).second)
                UnloadTexture(texture);
        }
    }
    UnloadModel(model);
    model = {};
}
}

BoundingBox transformed_model_bounds(const Model& model) {
    Model raw = model;
    raw.transform = MatrixIdentity();
    const auto local = GetModelBoundingBox(raw);
    BoundingBox result{Vector3Transform(local.min, model.transform),
        Vector3Transform(local.min, model.transform)};
    for (int corner = 1; corner < 8; ++corner) {
        const auto point = Vector3Transform({
            (corner & 1) ? local.max.x : local.min.x,
            (corner & 2) ? local.max.y : local.min.y,
            (corner & 4) ? local.max.z : local.min.z}, model.transform);
        result.min = Vector3Min(result.min, point);
        result.max = Vector3Max(result.max, point);
    }
    return result;
}

ModelCache::ModelCache(std::size_t capacity) : capacity_(std::max<std::size_t>(1, capacity)) {}
ModelCache::~ModelCache() { clear(); }

Model* ModelCache::load(const std::string& path, const std::function<bool(Model&)>& prepare, bool animations) {
    deferred_=false;
    if (path.empty()) return nullptr;
    // A hot draw must not stat the filesystem again (Termux shared storage can be slow).
    auto known=resolved_paths_.find(path);
    if(known==resolved_paths_.end() && remaining_loads_==0) {deferred_=true;return nullptr;}
    const std::string packaged=known==resolved_paths_.end() ? std::string(GetApplicationDirectory())+path : std::string{};
    const std::string resolved=known!=resolved_paths_.end() ? known->second : (FileExists(packaged.c_str()) ? packaged : path);
    if (auto found = entries_.find(resolved); found != entries_.end()) {
        if (animations && !found->second.animation_attempted) {
            found->second.animation_attempted=true;
            found->second.clips=LoadModelAnimations(resolved.c_str(),&found->second.clip_count);
            if(!found->second.clips) found->second.clip_count=0;
        }
        found->second.last_used = ++sequence_;
        return &found->second.model;
    }
    if(remaining_loads_==0) {deferred_=true;return nullptr;}
    if (!FileExists(resolved.c_str())) {
        TraceLog(LOG_WARNING, "OUTLAND asset missing: %s", path.c_str());
        return nullptr;
    }
    --remaining_loads_;
    Model model = LoadModel(resolved.c_str());
    if (model.meshCount <= 0) {
        release(model);
        TraceLog(LOG_WARNING, "OUTLAND asset failed to load: %s", path.c_str());
        return nullptr;
    }
    if(model.skeleton.boneCount>0 && (!model.skeleton.bones || !model.skeleton.bindPose || !model.currentPose || !model.boneMatrices)){release(model);TraceLog(LOG_ERROR,"OUTLAND invalid raylib 6 skeleton: %s",path.c_str());return nullptr;}
    if (prepare && !prepare(model)) {
        release(model);
        TraceLog(LOG_ERROR, "OUTLAND model preparation failed: %s", path.c_str());
        return nullptr;
    }
    // Exported origins vary; ground-centred geometry agrees with placement bounds.
    const BoundingBox bounds = transformed_model_bounds(model);
    model.transform = MatrixMultiply(model.transform, MatrixTranslate(
        -(bounds.min.x + bounds.max.x) * .5F, -bounds.min.y,
        -(bounds.min.z + bounds.max.z) * .5F));
    if (entries_.size() >= capacity_) {
        auto oldest = std::min_element(entries_.begin(), entries_.end(),
            [](const auto& a, const auto& b) { return a.second.last_used < b.second.last_used; });
        if(oldest->second.clips) UnloadModelAnimations(oldest->second.clips,oldest->second.clip_count);
        release(oldest->second.model);
        std::erase_if(resolved_paths_,[&](const auto& alias){return alias.second==oldest->first;});
        entries_.erase(oldest);
    }
    auto result = entries_.emplace(resolved, Entry{model, ++sequence_});
    resolved_paths_[path]=resolved;
    if(animations) {
        result.first->second.animation_attempted=true;
        result.first->second.clips=LoadModelAnimations(resolved.c_str(),&result.first->second.clip_count);
        if(!result.first->second.clips) result.first->second.clip_count=0;
    }
    return &result.first->second.model;
}

std::span<const ModelAnimation> ModelCache::animations(const Model* model) const {
    for(const auto& [path,entry]:entries_) if(&entry.model==model) return {entry.clips,static_cast<std::size_t>(entry.clip_count)};
    return {};
}
bool ModelCache::posed(const Model* model) const {
    for(const auto& [path,entry]:entries_) if(&entry.model==model) return entry.posed;
    return false;
}
void ModelCache::mark_posed(const Model* model,bool value) {
    for(auto& [path,entry]:entries_) if(&entry.model==model) {entry.posed=value;return;}
}

void ModelCache::clear() {
    for (auto& [path, entry] : entries_) {
        if(entry.clips) UnloadModelAnimations(entry.clips,entry.clip_count);
        release(entry.model);
    }
    entries_.clear();
    resolved_paths_.clear();
}
} // namespace outland::assets
