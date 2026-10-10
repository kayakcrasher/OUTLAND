#pragma once

#include <raylib.h>
#include <unordered_map>
#include <cstdint>
namespace outland::engine { class ViewFrustum; }

namespace outland::world { class VerdaRegion; }

namespace outland::world::foliage {

class FoliageSystem {
public:
    FoliageSystem();

    void draw(
        const Vector3& camera_position,
        const VerdaRegion& region,
        const engine::ViewFrustum* view = nullptr
    ) const;

    void invalidate() const { cells_.clear(); }
    std::size_t cached_cells() const { return cells_.size(); }
    int evaluations_last_frame() const { return evaluations_; }
    static constexpr int evaluation_budget = 24;

private:
    struct Cell { float y; bool allowed; };
    mutable std::unordered_map<std::uint64_t, Cell> cells_;
    mutable const VerdaRegion* region_{nullptr};
    mutable std::size_t object_count_{0};
    mutable bool coastal_{false};
    mutable int evaluations_{0};
    static float hash(
        int x,
        int z,
        int salt
    );

    static void draw_grass_clump(
        Vector3 position,
        float height,
        float width,
        Color color
    );

    static void draw_weed(
        Vector3 position,
        float height,
        Color color
    );

    static void draw_shrub(
        Vector3 position,
        float scale,
        Color color
    );

    static void draw_flower(
        Vector3 position,
        float height,
        Color flower_color
    );
};

} // namespace outland::world::foliage
