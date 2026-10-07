#pragma once

#include <raylib.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace outland::creator {

struct CreatorPlacement {
    std::uint64_t id{0};

    std::string asset_id;

    Vector3 position{
        0.0F,
        0.0F,
        0.0F
    };

    float rotation_y{0.0F};
};

class CreatorWorld {
public:
    using PlacementId = std::uint64_t;

    [[nodiscard]]
    PlacementId place(
        std::string_view asset_id,
        Vector3 position,
        float rotation_y
    ) {
        CreatorPlacement placement;

        placement.id = next_id_++;
        placement.asset_id = std::string(asset_id);
        placement.position = position;
        placement.rotation_y = rotation_y;

        placements_.push_back(placement);

        return placement.id;
    }

    [[nodiscard]]
    bool erase(PlacementId id) {
        for (
            auto iterator = placements_.begin();
            iterator != placements_.end();
            ++iterator
        ) {
            if (iterator->id == id) {
                placements_.erase(iterator);
                return true;
            }
        }

        return false;
    }

    void clear() {
        placements_.clear();
        next_id_ = 1;
    }

    [[nodiscard]]
    const std::vector<CreatorPlacement>&
    placements() const {
        return placements_;
    }

    [[nodiscard]]
    std::vector<CreatorPlacement>&
    placements() {
        return placements_;
    }

    [[nodiscard]]
    const CreatorPlacement*
    find(PlacementId id) const {
        for (const auto& placement : placements_) {
            if (placement.id == id) {
                return &placement;
            }
        }

        return nullptr;
    }

    [[nodiscard]]
    std::size_t size() const {
        return placements_.size();
    }

private:
    std::vector<CreatorPlacement> placements_;

    PlacementId next_id_{1};
};

} // namespace outland::creator
