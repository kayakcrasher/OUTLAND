#pragma once

#ifdef OUTLAND_DEV_TOOLS

#include <string>

namespace outland::world {
class VerdaRegion;
}

namespace outland::creator {

class CreatorMapIO {
public:
    [[nodiscard]]
    static bool save(
        const world::VerdaRegion& region,
        const std::string& path
    );

    [[nodiscard]]
    static bool load(
        world::VerdaRegion& region,
        const std::string& path
    );
};

} // namespace outland::creator

#endif // OUTLAND_DEV_TOOLS
