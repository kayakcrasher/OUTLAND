#pragma once

#include <string>

namespace outland::world {
class VerdaRegion;
}

namespace outland::creator {

class CreatorMapIO {
public:
#ifdef OUTLAND_DEV_TOOLS
    [[nodiscard]]
    static bool save(
        const world::VerdaRegion& region,
        const std::string& path
    );

#endif
    [[nodiscard]]
    static bool load(
        world::VerdaRegion& region,
        const std::string& path
    );
};

} // namespace outland::creator
