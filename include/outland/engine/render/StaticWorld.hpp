#pragma once
#include "outland/engine/render/GeometryRecorder.hpp"
#include <raylib.h>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace outland::world { class VerdaRegion; }

namespace outland::engine {
class ViewFrustum;

// The procedural world (houses, street trees, roads) recorded into static meshes the first time
// each piece is seen, then drawn from the GPU: a house is one draw call, a 48 m patch of trees one
// per level of detail, a 128 m stretch of road one. Same distances and levels of detail as the
// immediate drawing it replaces. invalidate() after the map changes (mode start, leaving the
// builder); while building in the Creator, draw the region directly instead.
class StaticWorld {
public:
    void invalidate();
    void draw(const world::VerdaRegion& region, Vector3 camera, const ViewFrustum* view);
    int draws_last_frame() const { return draws_; }
private:
    void index(const world::VerdaRegion& region);
    struct Building { std::size_t settlement, index; Vector3 centre; float radius; std::array<std::unique_ptr<RecordedGeometry>, 2> lods; bool failed{false}; };
    struct Trees { std::vector<Vector3> positions; Vector3 centre{}; std::array<std::unique_ptr<RecordedGeometry>, 3> lods; bool failed{false}; };
    struct RoadChunk { std::size_t settlement, index; int begin, end; Vector3 centre; float radius; std::unique_ptr<RecordedGeometry> mesh; bool failed{false}; };
    bool indexed_{false};
    std::vector<Building> buildings_;
    std::vector<Trees> trees_;
    std::vector<RoadChunk> roads_;
    GeometryRecorder recorder_;
    int draws_{0};
};
}
