#pragma once

#include "outland/world/terrain/TerrainChunk.hpp"

#include <memory>
namespace outland::engine { class ViewFrustum; }
#include <vector>

namespace outland::world::terrain {

class TerrainWorld {
public:
    TerrainWorld();

    void draw(const engine::ViewFrustum* view = nullptr) const;
    static constexpr int build_budget = 2;
    // Chunks streamed round the camera: `radius` rings of 128 m chunks (3 on the ground; more,
    // coarser ones beyond ring 3, when the view reaches far - the Battle Royale drop).
    static constexpr int ground_radius = 3, far_radius = 6;
    void set_radius(int radius) { radius_ = radius < ground_radius ? ground_radius : radius > far_radius ? far_radius : radius; }
    [[nodiscard]] int radius() const { return radius_; }
    // Metres from the camera the streamed land surely reaches (its chunk's far edges aside).
    [[nodiscard]] float reach() const { return static_cast<float>(radius_) * kChunkSize; }
    void update(Vector3 position);

    [[nodiscard]]
    float height_at(
        float world_x,
        float world_z
    ) const;

    [[nodiscard]]
    int chunk_count() const;

    // The grid a chunk is built at, `ring` chunks from the one the camera is over: finer near
    // the highways (kRoadResolution), coarser far away.
    static int resolution_for(int chunk_x, int chunk_z, int ring);

private:
    static constexpr int kRadius = 3;        // the near rings, at full detail
    static constexpr int kFarResolution = 10;
    int radius_{ground_radius};
    int center_x_{0}, center_z_{0};
    bool coastal_layout_{false};
    static constexpr float kChunkSize = 128.0F;
    static constexpr int kResolution = 24;
    // Near the highways, their slip roads, roundabouts and La Ringo: a 2 m grid, the roads' own
    // step, so the land follows their banks and cuttings and never shows through the roadway.
    static constexpr int kRoadResolution = 64;

    std::vector<
        std::unique_ptr<TerrainChunk>
    > chunks_;

    void build_initial_region();
};

} // namespace outland::world::terrain
