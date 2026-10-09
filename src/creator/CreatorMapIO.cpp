#include "outland/creator/CreatorMapIO.hpp"

#include "outland/world/Building.hpp"
#include "outland/world/GameplayMarker.hpp"
#include "outland/world/Settlement.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/WorldAsset.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <unordered_set>
#include <limits>
#include <cmath>

namespace outland::creator {

namespace {

constexpr std::string_view magic =
    "OUTLAND_CREATOR_MAP";

constexpr int version = 5;

constexpr std::string_view creator_prefix =
    "creator_";

constexpr std::string_view creator_building_prefix =
    "creator_building_";

constexpr std::string_view creator_marker_prefix =
    "creator_marker_";

bool is_creator_asset(
    const world::WorldAsset& asset
) {
    return asset.id.starts_with(
        creator_prefix
    );
}

bool is_creator_building(
    const world::Building& building
) {
    return building.id.starts_with(
        creator_building_prefix
    );
}

bool is_creator_marker(
    const world::GameplayMarker& marker
) {
    return marker.id.starts_with(
        creator_marker_prefix
    );
}

} // namespace

std::string CreatorMapIO::writable_path(const std::string& application_directory,const std::string& home,const std::string& override_directory) {
    namespace fs=std::filesystem;
    const fs::path root=!override_directory.empty() ? fs::path(override_directory) :
        !home.empty() ? fs::path(home)/".local/share/outland" : fs::path(application_directory);
    return (root/"maps/verda_creator.map").string();
}
#ifdef OUTLAND_DEV_TOOLS
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

    output << std::setprecision(std::numeric_limits<float>::max_digits10);
    output
        << magic
        << ' '
        << version
        << '\n';
    output << "GEOGRAPHY " << (region.coastal_layout()?1:0) << '\n';

    for (
        const world::Settlement& settlement :
        region.settlements()
    ) {
        output << "SETTLEMENT " << std::quoted(settlement.id) << ' ' << std::quoted(settlement.name)
            << ' ' << settlement.center.x << ' ' << settlement.center.y << ' ' << settlement.center.z
            << ' ' << static_cast<int>(settlement.state) << ' ' << settlement.survivors
            << ' ' << settlement.hostiles << ' ' << settlement.infected << '\n';
        for(const auto& road:settlement.roads)
            output << "ROAD " << road.start.x << ' ' << road.start.y << ' ' << road.start.z << ' '
                << road.end.x << ' ' << road.end.y << ' ' << road.end.z << ' '
                << road.width << ' ' << static_cast<int>(road.type) << '\n';
        // ----------------------------------------------------
        // CREATOR BUILDINGS
        // ----------------------------------------------------

        for (
            const world::Building& building :
            settlement.buildings
        ) {

            output
                << "BUILDING "
                << std::quoted(building.id) << ' '
                << static_cast<int>(
                    building.style
                ) << ' '

                << building.position.x << ' '
                << building.position.y << ' '
                << building.position.z << ' '

                << building.size.x << ' '
                << building.size.y << ' '
                << building.size.z << ' '

                << building.rotation_y << ' '

                << static_cast<int>(
                    building.wall_color.r
                ) << ' '
                << static_cast<int>(
                    building.wall_color.g
                ) << ' '
                << static_cast<int>(
                    building.wall_color.b
                ) << ' '
                << static_cast<int>(
                    building.wall_color.a
                ) << ' '

                << static_cast<int>(
                    building.roof_color.r
                ) << ' '
                << static_cast<int>(
                    building.roof_color.g
                ) << ' '
                << static_cast<int>(
                    building.roof_color.b
                ) << ' '
                << static_cast<int>(
                    building.roof_color.a
                ) << ' '

                << static_cast<int>(
                    building.enterable
                )

                << '\n';
        }

        // ----------------------------------------------------
        // CREATOR WORLD ASSETS
        // ----------------------------------------------------

        for (
            const world::WorldAsset& asset :
            settlement.assets
        ) {

            output
                << "ASSET "
                << std::quoted(asset.id) << ' '
                << static_cast<int>(
                    asset.type
                ) << ' '
                << std::quoted(
                    asset.model_path
                ) << ' '

                << asset.position.x << ' '
                << asset.position.y << ' '
                << asset.position.z << ' '

                << asset.size.x << ' '
                << asset.size.y << ' '
                << asset.size.z << ' '

                << asset.rotation_y << ' '

                << static_cast<int>(
                    asset.primary_color.r
                ) << ' '
                << static_cast<int>(
                    asset.primary_color.g
                ) << ' '
                << static_cast<int>(
                    asset.primary_color.b
                ) << ' '
                << static_cast<int>(
                    asset.primary_color.a
                ) << ' '

                << static_cast<int>(
                    asset.secondary_color.r
                ) << ' '
                << static_cast<int>(
                    asset.secondary_color.g
                ) << ' '
                << static_cast<int>(
                    asset.secondary_color.b
                ) << ' '
                << static_cast<int>(
                    asset.secondary_color.a
                ) << ' '

                << static_cast<int>(
                    asset.collision
                )

                << '\n';
        }

        // ----------------------------------------------------
        // CREATOR GAMEPLAY MARKERS
        // ----------------------------------------------------

        for (
            const world::GameplayMarker& marker :
            settlement.gameplay_markers
        ) {

            output
                << "MARKER "
                << std::quoted(marker.id) << ' '
                << static_cast<int>(
                    marker.type
                ) << ' '

                << marker.position.x << ' '
                << marker.position.y << ' '
                << marker.position.z << ' '

                << marker.size.x << ' '
                << marker.size.y << ' '
                << marker.size.z << ' '

                << marker.rotation_y << ' '

                << static_cast<int>(
                    marker.enabled
                )

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
    if(!output) {std::error_code cleanup;fs::remove(temp_path,cleanup);return false;}

    std::error_code rename_error;

    fs::rename(
        temp_path,
        final_path,
        rename_error
    );

    if (!rename_error) {
        return true;
    }

    std::error_code cleanup_error;
    fs::remove(temp_path,cleanup_error);
    return false; // Keep the previous good destination intact.
}

#endif

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
        (file_version != 3 && file_version != 4 && file_version != version)
    ) {
        return false;
    }

    std::vector<world::Building>
        loaded_buildings;

    std::vector<world::WorldAsset>
        loaded_assets;

    std::vector<world::GameplayMarker>
        loaded_markers;

    std::vector<world::Settlement> snapshot;
    std::unordered_set<std::string> settlement_ids;
    const bool full_snapshot=file_version>=4;
    bool coastal_layout=false;
    if(file_version==5) {
        std::string geography;int flag=-1;
        if(!(input>>geography>>flag) || geography!="GEOGRAPHY" || (flag!=0 && flag!=1)) return false;
        coastal_layout=flag==1;
    }
    std::string record;

    while (input >> record) {
        if(full_snapshot && record=="SETTLEMENT") {
            world::Settlement settlement;int state=0;
            input >> std::quoted(settlement.id) >> std::quoted(settlement.name)
                >> settlement.center.x >> settlement.center.y >> settlement.center.z >> state
                >> settlement.survivors >> settlement.hostiles >> settlement.infected;
            if(!input || settlement.id.empty() || !settlement_ids.insert(settlement.id).second || state<0 || state>5) return false;
            settlement.state=static_cast<world::SettlementState>(state);
            snapshot.push_back(std::move(settlement));continue;
        }
        if(full_snapshot && snapshot.empty()) return false;
        if(full_snapshot && record=="ROAD") {
            world::Road road;int type=0;
            input >> road.start.x >> road.start.y >> road.start.z >> road.end.x >> road.end.y >> road.end.z >> road.width >> type;
            if(!input || road.width<=0 || type<0 || type>2) return false;
            road.type=static_cast<world::RoadType>(type);snapshot.back().roads.push_back(road);continue;
        }
        // ----------------------------------------------------
        // BUILDING
        // ----------------------------------------------------

        if (record == "BUILDING") {
            world::Building building;

            int style_value = 0;
            int enterable_value = 0;

            int wall_r = 0;
            int wall_g = 0;
            int wall_b = 0;
            int wall_a = 0;

            int roof_r = 0;
            int roof_g = 0;
            int roof_b = 0;
            int roof_a = 0;

            input
                >> std::quoted(building.id)
                >> style_value

                >> building.position.x
                >> building.position.y
                >> building.position.z

                >> building.size.x
                >> building.size.y
                >> building.size.z

                >> building.rotation_y

                >> wall_r
                >> wall_g
                >> wall_b
                >> wall_a

                >> roof_r
                >> roof_g
                >> roof_b
                >> roof_a

                >> enterable_value;

            if (!input) {
                return false;
            }

            if (
                !full_snapshot && !building.id.starts_with(
                    creator_building_prefix
                )
            ) {
                return false;
            }

            building.style =
                static_cast<world::BuildingStyle>(
                    style_value
                );

            building.wall_color = Color{
                static_cast<unsigned char>(wall_r),
                static_cast<unsigned char>(wall_g),
                static_cast<unsigned char>(wall_b),
                static_cast<unsigned char>(wall_a)
            };

            building.roof_color = Color{
                static_cast<unsigned char>(roof_r),
                static_cast<unsigned char>(roof_g),
                static_cast<unsigned char>(roof_b),
                static_cast<unsigned char>(roof_a)
            };

            building.enterable =
                enterable_value != 0;

            if(full_snapshot) snapshot.back().buildings.push_back(std::move(building));
            else loaded_buildings.push_back(std::move(building));

            continue;
        }

        // ----------------------------------------------------
        // WORLD ASSET
        // ----------------------------------------------------

        if (record == "ASSET") {
            world::WorldAsset asset;

            int type_value = 0;
            int collision_value = 0;

            int primary_r = 0;
            int primary_g = 0;
            int primary_b = 0;
            int primary_a = 0;

            int secondary_r = 0;
            int secondary_g = 0;
            int secondary_b = 0;
            int secondary_a = 0;

            input
                >> std::quoted(asset.id)
                >> type_value
                >> std::quoted(
                    asset.model_path
                )

                >> asset.position.x
                >> asset.position.y
                >> asset.position.z

                >> asset.size.x
                >> asset.size.y
                >> asset.size.z

                >> asset.rotation_y

                >> primary_r
                >> primary_g
                >> primary_b
                >> primary_a

                >> secondary_r
                >> secondary_g
                >> secondary_b
                >> secondary_a

                >> collision_value;

            if (!input) {
                return false;
            }

            if (
                !full_snapshot && !asset.id.starts_with(
                    creator_prefix
                )
            ) {
                return false;
            }

            asset.type =
                static_cast<world::AssetType>(
                    type_value
                );

            asset.primary_color = Color{
                static_cast<unsigned char>(
                    primary_r
                ),
                static_cast<unsigned char>(
                    primary_g
                ),
                static_cast<unsigned char>(
                    primary_b
                ),
                static_cast<unsigned char>(
                    primary_a
                )
            };

            asset.secondary_color = Color{
                static_cast<unsigned char>(
                    secondary_r
                ),
                static_cast<unsigned char>(
                    secondary_g
                ),
                static_cast<unsigned char>(
                    secondary_b
                ),
                static_cast<unsigned char>(
                    secondary_a
                )
            };

            asset.collision =
                collision_value != 0;

            if(full_snapshot) snapshot.back().assets.push_back(std::move(asset));
            else loaded_assets.push_back(std::move(asset));

            continue;
        }

        // ----------------------------------------------------
        // GAMEPLAY MARKER
        // ----------------------------------------------------

        if (record == "MARKER") {
            world::GameplayMarker marker;

            int type_value = 0;
            int enabled_value = 0;

            input
                >> std::quoted(marker.id)
                >> type_value

                >> marker.position.x
                >> marker.position.y
                >> marker.position.z

                >> marker.size.x
                >> marker.size.y
                >> marker.size.z

                >> marker.rotation_y

                >> enabled_value;

            if (!input) {
                return false;
            }

            if (
                !full_snapshot && !marker.id.starts_with(
                    creator_marker_prefix
                )
            ) {
                return false;
            }

            marker.type =
                static_cast<world::GameplayMarkerType>(
                    type_value
                );

            marker.enabled =
                enabled_value != 0;

            if(full_snapshot) snapshot.back().gameplay_markers.push_back(std::move(marker));
            else loaded_markers.push_back(std::move(marker));

            continue;
        }

        // Unknown record: keep the current world unchanged.
        return false;
    }

    /*
     * Parsing succeeded completely.
     * Only now mutate the live world.
     */
    if(full_snapshot) {
        if(snapshot.empty()) return false;
        region.settlements_.resize(snapshot.size());
        for(std::size_t i=0;i<snapshot.size();++i)region.settlements_[i]=std::move(snapshot[i]);
        region.coastal_layout_=coastal_layout;
        world::terrain::TerrainHeight::set_coastal_layout(coastal_layout);
        return true;
    }
    auto& settlements =
        region.settlements_;

    if (
        settlements.empty() &&
        (
            !loaded_buildings.empty() ||
            !loaded_assets.empty() ||
            !loaded_markers.empty()
        )
    ) {
        return false;
    }

    for (
        world::Settlement& settlement :
        settlements
    ) {
        std::erase_if(
            settlement.buildings,
            [](const world::Building& building) {
                return is_creator_building(
                    building
                );
            }
        );

        std::erase_if(
            settlement.assets,
            [](const world::WorldAsset& asset) {
                return is_creator_asset(
                    asset
                );
            }
        );

        std::erase_if(
            settlement.gameplay_markers,
            [](const world::GameplayMarker& marker) {
                return is_creator_marker(
                    marker
                );
            }
        );
    }

    if (settlements.empty()) {
        return true;
    }

    world::Settlement& destination =
        settlements.front();

    for (
        world::Building& building :
        loaded_buildings
    ) {
        destination.buildings.push_back(
            std::move(building)
        );
    }

    for (
        world::WorldAsset& asset :
        loaded_assets
    ) {
        destination.assets.push_back(
            std::move(asset)
        );
    }

    for (
        world::GameplayMarker& marker :
        loaded_markers
    ) {
        destination.gameplay_markers.push_back(
            std::move(marker)
        );
    }

    return true;
}

} // namespace outland::creator
