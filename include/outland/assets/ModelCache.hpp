#pragma once

#include <raylib.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <functional>
#include <span>
#include <unordered_map>

namespace outland::assets {

// raylib 5.5 transforms only two AABB corners; this handles rotations correctly.
[[nodiscard]] BoundingBox transformed_model_bounds(const Model& model);

// Lazy GPU model cache. Call clear() before closing the graphics context.
class ModelCache {
public:
    explicit ModelCache(std::size_t capacity = 64);
    ~ModelCache();
    ModelCache(const ModelCache&) = delete;
    ModelCache& operator=(const ModelCache&) = delete;

    // Optional preparation runs before grounding, once per actual GPU load (also after eviction).
    // Use a consistent preparation policy for each path within a cache instance.
    // The pointer remains valid until another load() evicts it or clear() runs.
    [[nodiscard]] Model* load(const std::string& path, const std::function<bool(Model&)>& prepare = {}, bool animations = false);
    std::span<const ModelAnimation> animations(const Model* model) const;
    bool posed(const Model* model) const;
    void mark_posed(const Model* model,bool value);
    [[nodiscard]] std::size_t size() const { return entries_.size(); }
    void clear();

private:
    struct Entry { Model model{}; std::uint64_t last_used{0}; ModelAnimation* clips{nullptr}; int clip_count{0}; bool animation_attempted{false}, posed{false}; };
    std::unordered_map<std::string, Entry> entries_;
    std::size_t capacity_;
    std::uint64_t sequence_{0};
};

} // namespace outland::assets
