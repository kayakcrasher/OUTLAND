#pragma once

#include "outland/world/Settlement.hpp"

#include <vector>

namespace outland::world {

class VerdaRegion {
public:
    VerdaRegion();

    void generate_training_region();

    [[nodiscard]]
    const std::vector<Settlement>&
    settlements() const;

    void draw(const Vector3& camera_position) const;

private:
    std::vector<Settlement> settlements_;

    void create_first_village();
};

}
