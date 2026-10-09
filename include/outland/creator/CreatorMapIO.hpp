#pragma once

#include <string>

namespace outland::world {
class VerdaRegion;
}

namespace outland::creator {

class CreatorMapIO {
public:
    // Stable user storage; packaged maps are read-only defaults.
    static std::string writable_path(const std::string& application_directory,const std::string& home={},const std::string& override_directory={});
    // Personal edits override the published world, which overrides legacy packaged maps.
    static std::string startup_map(const std::string& application_directory,const std::string& home={},const std::string& override_directory={},const std::string& working_directory=".");
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
