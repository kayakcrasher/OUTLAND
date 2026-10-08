#include "outland/creator/CreatorMapIO.hpp"

#ifdef OUTLAND_DEV_TOOLS

#include "outland/world/Settlement.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/WorldAsset.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <string_view>
#include <vector>

namespace outland::creator {

namespace {

constexpr std::string_view magic =
    "OUTLAND_CREATOR_MAP";

constexpr int version = 1;

constexpr std::string_view creator_prefix =
    "creator_";

bool is_creator_asset(
    const world::WorldAsset& asset
) {
    return asset.id.starts_with(
        creator_prefix
    );
}

} // namespace

bool CreatorMapIO::save(
    const world::VerdaRegion& region,
    const std::string& path
) {
    namespace fs = std::filesystem;

    if (path.empty()) {
        return false;
    }

    const fs::path final_path(path);
    const fs::path temp_path =
        final_path.string() + ".tmp";

    if (
        final_path.has_parent_path() &&
        !final_path.parent_path().empty()
    ) {
        std::error_code directory_error;

        fs::create_directories(
            final_path.parent_path(),
            directory_error
        );

        if (directory_error) {
            return false;
        }
    }

    std::ofstream output(
        temp_path,
        std::ios::trunc
    );

    if (!output) {
        return false;
    }

    output
        << magic
        << ' '
        << version
        << '\n';

    for (
        const world::Settlement& settlement :
        region.settlements()
    ) {
        for (
            const world::WorldAsset& asset :
            settlement.assets
        ) {
            if (!is_creator_asset(asset)) {
                continue;
            }

            output
                << "ASSET "
                << std::quoted(asset.id) << ' '
                << static_cast<int>(asset.type) << ' '
                << std::quoted(asset.model_path) << ' '

                << asset.position.x << ' '
                << asset.position.y << ' '
                << asset.position.z << ' '

                << asset.size.x << ' '
                << asset.size.y << ' '
                << asset.size.z << ' '

                << asset.rotation_y << ' '
                << static_cast<int>(asset.collision)
                << '\n';
        }
    }

    output.flush();

    if (!output) {
        output.close();

        std::error_code remove_error;
        fs::remove(
            temp_path,
            remove_error
        );

        return false;
    }

    output.close();

    std::error_code rename_error;

    fs::rename(
        temp_path,
        final_path,
        rename_error
    );

    if (!rename_error) {
        return true;
    }

    /*
     * Some filesystems will not replace an
     * existing destination during rename.
     */
    std::error_code remove_error;

    fs::remove(
        final_path,
        remove_error
    );

    rename_error.clear();

    fs::rename(
        temp_path,
        final_path,
        rename_error
    );

    if (rename_error) {
        std::error_code cleanup_error;

        fs::remove(
            temp_path,
            cleanup_error
        );

        return false;
    }

    return true;
}

bool CreatorMapIO::load(
    world::VerdaRegion& region,
    const std::string& path
) {
    if (path.empty()) {
        return false;
    }

    std::ifstream input(path);

    if (!input) {
        return false;
    }

    std::string file_magic;
    int file_version = 0;

    input
        >> file_magic
        >> file_version;

    if (
        file_magic != magic ||
        file_version != version
    ) {
        return false;
    }

    std::vector<world::WorldAsset>
        loaded_assets;

    std::string record;

    while (input >> record) {
        if (record != "ASSET") {
            return false;
        }

        world::WorldAsset asset;
        int type_value = 0;
        int collision_value = 0;

        input
            >> std::quoted(asset.id)
            >> type_value
            >> std::quoted(asset.model_path)

            >> asset.position.x
            >> asset.position.y
            >> asset.position.z

            >> asset.size.x
            >> asset.size.y
            >> asset.size.z

            >> asset.rotation_y
            >> collision_value;

        if (!input) {
            return false;
        }

        if (
            !asset.id.starts_with(
                creator_prefix
            )
        ) {
            return false;
        }

        asset.type =
            static_cast<world::AssetType>(
                type_value
            );

        asset.collision =
            collision_value != 0;

        loaded_assets.push_back(
            std::move(asset)
        );
    }

    /*
     * Parsing succeeded completely.
     * Only now mutate the live world.
     */
    auto& settlements =
        region.editable_settlements();

    for (
        world::Settlement& settlement :
        settlements
    ) {
        std::erase_if(
            settlement.assets,
            [](const world::WorldAsset& asset) {
                return is_creator_asset(asset);
            }
        );
    }

    if (
        loaded_assets.empty()
    ) {
        return true;
    }

    if (settlements.empty()) {
        return false;
    }

    for (
        world::WorldAsset& asset :
        loaded_assets
    ) {
        settlements.front().assets.push_back(
            std::move(asset)
        );
    }

    return true;
}

} // namespace outland::creator

#endif // OUTLAND_DEV_TOOLS
