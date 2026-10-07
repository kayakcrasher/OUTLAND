#pragma once

#include "outland/world/Settlement.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace outland::world {

class VerdaRegion {
public:
    VerdaRegion();

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
    std::vector<Settlement> settlements_;

    void create_first_village();
};

} // namespace outland::world
