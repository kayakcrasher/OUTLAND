#pragma once

#include "outland/world/Settlement.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace outland::creator { class CreatorMapIO; }

namespace outland::world {

class VerdaRegion {
public:
    VerdaRegion();
    explicit VerdaRegion(bool coastal_layout);
    [[nodiscard]] bool coastal_layout() const { return coastal_layout_; }

    void generate_training_region();

    [[nodiscard]]
    const std::vector<Settlement>&
    settlements() const;

#ifdef OUTLAND_DEV_TOOLS

    // DEV ONLY:
    // Creator Mode edits the live Verda world.
    [[nodiscard]]
    std::vector<Settlement>&
    editable_settlements();

    // Insert a data-driven Creator asset into the live
    // Verda world. Returns false if there is no settlement.
    [[nodiscard]]
    bool place_world_asset(
        WorldAsset asset,
        std::size_t settlement_index = 0
    );

    [[nodiscard]]
    bool delete_world_asset(
        std::string_view asset_id
    );

    [[nodiscard]]
    bool delete_building(
        std::string_view building_id
    );

    [[nodiscard]]
    bool delete_road(
        std::size_t settlement_index,
        std::size_t road_index
    );

#endif // OUTLAND_DEV_TOOLS

    void draw(
        const Vector3& camera_position
    ) const;

private:
    friend class outland::creator::CreatorMapIO;
    std::vector<Settlement> settlements_;
    bool coastal_layout_{true};
    void create_coastal_region();

    void create_first_village();
};

} // namespace outland::world
