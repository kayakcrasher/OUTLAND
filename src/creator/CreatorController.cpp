#include "outland/creator/CreatorController.hpp"

#ifdef OUTLAND_DEV_TOOLS

#include "outland/world/terrain/TerrainHeight.hpp"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace outland::creator {

namespace {

constexpr float max_select_distance = 120.0F;
constexpr float road_pick_height = 0.8F;
constexpr float minimum_place_distance = 2.0F;
constexpr float maximum_place_distance = 80.0F;

float distance_to_segment_xz(
    const Vector3 point,
    const Vector3 start,
    const Vector3 end
) {
    const float ab_x = end.x - start.x;
    const float ab_z = end.z - start.z;

    const float ap_x = point.x - start.x;
    const float ap_z = point.z - start.z;

    const float length_squared =
        ab_x * ab_x + ab_z * ab_z;

    if (length_squared < 0.0001F) {
        const float dx = point.x - start.x;
        const float dz = point.z - start.z;

        return std::sqrt(dx * dx + dz * dz);
    }

    const float t = std::clamp(
        (ap_x * ab_x + ap_z * ab_z) /
            length_squared,
        0.0F,
        1.0F
    );

    const float closest_x =
        start.x + ab_x * t;

    const float closest_z =
        start.z + ab_z * t;

    const float dx =
        point.x - closest_x;

    const float dz =
        point.z - closest_z;

    return std::sqrt(dx * dx + dz * dz);
}

} // namespace

CreatorController::CreatorController() {
    if (!registry_.empty()) {
        selected_asset_index_ = 0;
        selected_hotbar_slot_ = 0;

        state_.selected_asset =
            selected_asset_index_;
    }
}

// ============================================================
// CREATOR ASSET INVENTORY
// ============================================================

const CreatorAssetRegistry&
CreatorController::registry() const {
    return registry_;
}

CreatorAssetRegistry&
CreatorController::registry() {
    return registry_;
}

const CreatorAssetDefinition*
CreatorController::selected_asset() const {
    const auto& assets = registry_.assets();

    if (
        assets.empty() ||
        selected_asset_index_ >= assets.size()
    ) {
        return nullptr;
    }

    return &assets[selected_asset_index_];
}

std::size_t
CreatorController::selected_asset_index() const {
    return selected_asset_index_;
}

bool CreatorController::select_asset(
    const std::size_t registry_index
) {
    if (registry_index >= registry_.size()) {
        return false;
    }

    selected_asset_index_ = registry_index;

    state_.selected_asset =
        selected_asset_index_;

    sync_selected_asset_to_hotbar();

    return true;
}

std::size_t
CreatorController::find_next_asset_in_category(
    const std::size_t start,
    const int direction
) const {
    const auto& assets = registry_.assets();

    if (assets.empty()) {
        return 0;
    }

    std::size_t index =
        start % assets.size();

    for (
        std::size_t count = 0;
        count < assets.size();
        ++count
    ) {
        if (direction >= 0) {
            index =
                (index + 1) %
                assets.size();
        } else {
            index =
                (
                    index +
                    assets.size() -
                    1
                ) %
                assets.size();
        }

        if (
            assets[index].category ==
            selected_category_
        ) {
            return index;
        }
    }

    return start % assets.size();
}

void CreatorController::select_next_asset() {
    if (registry_.empty()) {
        return;
    }

    select_asset(
        find_next_asset_in_category(
            selected_asset_index_,
            1
        )
    );
}

void CreatorController::select_previous_asset() {
    if (registry_.empty()) {
        return;
    }

    select_asset(
        find_next_asset_in_category(
            selected_asset_index_,
            -1
        )
    );
}

// ============================================================
// CATEGORY BROWSING
// ============================================================

CreatorAssetCategory
CreatorController::selected_category() const {
    return selected_category_;
}

void CreatorController::set_category(
    const CreatorAssetCategory category
) {
    selected_category_ = category;

    const auto& assets = registry_.assets();

    for (
        std::size_t index = 0;
        index < assets.size();
        ++index
    ) {
        if (
            assets[index].category ==
            selected_category_
        ) {
            select_asset(index);
            return;
        }
    }
}

void CreatorController::next_category() {
    constexpr int category_count = 7;

    int value =
        static_cast<int>(
            selected_category_
        );

    value =
        (value + 1) %
        category_count;

    set_category(
        static_cast<CreatorAssetCategory>(
            value
        )
    );
}

void CreatorController::previous_category() {
    constexpr int category_count = 7;

    int value =
        static_cast<int>(
            selected_category_
        );

    value =
        (
            value +
            category_count -
            1
        ) %
        category_count;

    set_category(
        static_cast<CreatorAssetCategory>(
            value
        )
    );
}

// ============================================================
// HOTBAR
// ============================================================

std::size_t
CreatorController::selected_hotbar_slot() const {
    return selected_hotbar_slot_;
}

const std::array<
    std::size_t,
    CreatorController::hotbar_size
>&
CreatorController::hotbar() const {
    return hotbar_;
}

bool CreatorController::select_hotbar_slot(
    const std::size_t slot
) {
    if (slot >= hotbar_size) {
        return false;
    }

    const std::size_t asset_index =
        hotbar_[slot];

    if (asset_index >= registry_.size()) {
        return false;
    }

    selected_hotbar_slot_ = slot;

    selected_asset_index_ =
        asset_index;

    state_.selected_asset =
        selected_asset_index_;

    selected_category_ =
        registry_.assets()[
            selected_asset_index_
        ].category;

    return true;
}

bool CreatorController::assign_hotbar_slot(
    const std::size_t slot,
    const std::size_t registry_index
) {
    if (
        slot >= hotbar_size ||
        registry_index >= registry_.size()
    ) {
        return false;
    }

    hotbar_[slot] =
        registry_index;

    selected_hotbar_slot_ =
        slot;

    selected_asset_index_ =
        registry_index;

    state_.selected_asset =
        selected_asset_index_;

    selected_category_ =
        registry_.assets()[
            registry_index
        ].category;

    return true;
}

void CreatorController::
sync_selected_asset_to_hotbar() {
    for (
        std::size_t slot = 0;
        slot < hotbar_size;
        ++slot
    ) {
        if (
            hotbar_[slot] ==
            selected_asset_index_
        ) {
            selected_hotbar_slot_ =
                slot;

            return;
        }
    }
}

void CreatorController::set_enabled(
    const bool enabled
) {
    state_.enabled = enabled;

    if (!enabled) {
        selection_.clear();
        preview_.valid = false;
    }
}

bool CreatorController::enabled() const {
    return state_.enabled;
}

void CreatorController::update(
    world::VerdaRegion& region,
    const Vector3 camera_position,
    Vector3 camera_forward,
    const bool allow_shortcuts
) {
    (void)region; // Mutations are routed through CreatorSession by the caller.
    if (!state_.enabled) {
        return;
    }

    if(state_.snap_to_ground) camera_forward.y = 0.0F;

    if (Vector3LengthSqr(camera_forward) > 0.0001F) {
        camera_forward =
            Vector3Normalize(camera_forward);
    }

    update_preview(
        camera_position,
        camera_forward
    );

    if(!allow_shortcuts)return;
    // Desktop shortcuts adjust the preview; world edits use CreatorSession.

    if (IsKeyPressed(KEY_Q)) {
        rotate_preview(-15.0F);
    }

    if (IsKeyPressed(KEY_E)) {
        rotate_preview(15.0F);
    }

    if (IsKeyPressed(KEY_EQUAL)) {
        increase_placement_distance();
    }

    if (IsKeyPressed(KEY_MINUS)) {
        decrease_placement_distance();
    }


}

void CreatorController::update_preview(
    const Vector3 camera_position,
    const Vector3 camera_forward
) {
    preview_.valid = false;

    if (Vector3LengthSqr(camera_forward) <
        0.0001F) {
        return;
    }

    preview_.position = Vector3Add(
        camera_position,
        Vector3Scale(
            camera_forward,
            state_.placement_distance
        )
    );

    if(state_.grid_step>0) {
        preview_.position.x=std::round(preview_.position.x/state_.grid_step)*state_.grid_step;
        preview_.position.z=std::round(preview_.position.z/state_.grid_step)*state_.grid_step;
    }
    if(state_.snap_to_ground) preview_.position.y=world::terrain::TerrainHeight::sample(preview_.position.x,preview_.position.z);
    preview_.position.y+=state_.placement_height;

    preview_.rotation_y =
        state_.placement_yaw;

    preview_.valid = true;
}

bool CreatorController::select_target(
    const world::VerdaRegion& region,
    const Vector3 origin,
    Vector3 direction
) {
    selection_.clear();

    if (Vector3LengthSqr(direction) <
        0.0001F) {
        return false;
    }

    direction = Vector3Normalize(direction);

    float nearest_distance =
        std::numeric_limits<float>::max();

    select_building(
        region,
        origin,
        direction,
        nearest_distance
    );

    select_road(
        region,
        origin,
        direction,
        nearest_distance
    );

    select_world_asset(
        region,
        origin,
        direction,
        nearest_distance
    );

    select_gameplay_marker(
        region,
        origin,
        direction,
        nearest_distance
    );

    return selection_.valid();
}

bool CreatorController::select_building(
    const world::VerdaRegion& region,
    const Vector3 origin,
    const Vector3 direction,
    float& nearest_distance
) {
    bool found = false;

    const Ray ray{
        origin,
        direction
    };

    const auto& settlements =
        region.settlements();

    for (
        std::size_t settlement_index = 0;
        settlement_index < settlements.size();
        ++settlement_index
    ) {
        const auto& settlement =
            settlements[settlement_index];

        for (const auto& building :
             settlement.buildings) {
            const float ground_y =
                world::terrain::TerrainHeight::sample(
                    building.position.x,
                    building.position.z
                );

            /*
             * Creator picking uses a conservative
             * world-space box for now.
             *
             * Rendering/collision may rotate the
             * building, but this is intentionally
             * forgiving for editor selection.
             */
            const BoundingBox bounds{
                {
                    building.position.x -
                        building.size.x * 0.5F,
                    ground_y,
                    building.position.z -
                        building.size.z * 0.5F
                },
                {
                    building.position.x +
                        building.size.x * 0.5F,
                    ground_y +
                        building.size.y,
                    building.position.z +
                        building.size.z * 0.5F
                }
            };

            const RayCollision hit =
                GetRayCollisionBox(
                    ray,
                    bounds
                );

            if (!hit.hit) {
                continue;
            }

            if (hit.distance >
                max_select_distance) {
                continue;
            }

            if (hit.distance >=
                nearest_distance) {
                continue;
            }

            nearest_distance =
                hit.distance;

            selection_.type =
                CreatorSelectionType::Building;

            selection_.building_id =
                building.id;

            selection_.settlement_index =
                settlement_index;

            selection_.position =
                building.position;

            found = true;
        }
    }

    return found;
}

bool CreatorController::select_road(
    const world::VerdaRegion& region,
    const Vector3 origin,
    const Vector3 direction,
    float& nearest_distance
) {
    /*
     * Intersect the selection ray with terrain-ish
     * horizontal space, then test that point against
     * each road ribbon.
     */

    if (std::fabs(direction.y) < 0.0001F) {
        return false;
    }

    bool found = false;

    const auto& settlements =
        region.settlements();

    for (
        std::size_t settlement_index = 0;
        settlement_index < settlements.size();
        ++settlement_index
    ) {
        const auto& roads =
            settlements[settlement_index].roads;

        for (
            std::size_t road_index = 0;
            road_index < roads.size();
            ++road_index
        ) {
            const auto& road =
                roads[road_index];

            const Vector3 midpoint{
                (road.start.x + road.end.x) * 0.5F,
                0.0F,
                (road.start.z + road.end.z) * 0.5F
            };

            const float road_y =
                world::terrain::TerrainHeight::sample(
                    midpoint.x,
                    midpoint.z
                ) + road_pick_height;

            const float t =
                (road_y - origin.y) /
                direction.y;

            if (t < 0.0F ||
                t > max_select_distance ||
                t >= nearest_distance) {
                continue;
            }

            const Vector3 point =
                Vector3Add(
                    origin,
                    Vector3Scale(
                        direction,
                        t
                    )
                );

            const float distance =
                distance_to_segment_xz(
                    point,
                    road.start,
                    road.end
                );

            if (distance >
                road.width * 0.5F + 0.75F) {
                continue;
            }

            nearest_distance = t;

            selection_.type =
                CreatorSelectionType::Road;

            selection_.settlement_index =
                settlement_index;

            selection_.road_index =
                road_index;

            selection_.position = midpoint;

            found = true;
        }
    }

    return found;
}

bool CreatorController::place_selected(
    world::VerdaRegion& region
) {
    const CreatorAssetDefinition* asset =
        selected_asset();

    if (asset == nullptr || !preview_.valid) {
        return false;
    }

    // ========================================================
    // PROCEDURAL OUTLAND BUILDINGS
    //
    // Buildings are real Verda Building objects rather than
    // decorative WorldAssets. This keeps them enterable and
    // connected to normal building collision/gameplay.
    // ========================================================

    if (asset->category == CreatorAssetCategory::Building && asset->model_path.empty()) {
        world::BuildingStyle style =
            world::BuildingStyle::RuralHouse;

        if (asset->id == "two_story_house") {
            style = world::BuildingStyle::TwoStoryHouse;
        } else if (asset->id == "shop") {
            style = world::BuildingStyle::Shop;
        } else if (asset->id == "garage") {
            style = world::BuildingStyle::Garage;
        } else if (asset->id == "warehouse") {
            style = world::BuildingStyle::Warehouse;
        } else if (asset->id != "rural_house") {
            return false;
        }

        auto& settlements =
            region.editable_settlements();

        if (settlements.empty()) {
            return false;
        }

        const std::string id_prefix =
            "creator_building_" +
            asset->id +
            "_";

        std::size_t next_id = 1;
        std::string building_id;

        for (;;) {
            const std::string candidate =
                id_prefix +
                std::to_string(next_id);

            bool exists = false;

            for (const auto& settlement : settlements) {
                for (
                    const world::Building& building :
                    settlement.buildings
                ) {
                    if (building.id == candidate) {
                        exists = true;
                        break;
                    }
                }

                if (exists) {
                    break;
                }
            }

            if (!exists) {
                building_id = candidate;
                break;
            }

            ++next_id;
        }

        world::Building building;

        building.id = std::move(building_id);
        building.style = style;
        building.position = preview_.position;

        building.size = {
            asset->footprint.width,
            asset->footprint.height,
            asset->footprint.depth
        };

        building.rotation_y =
            preview_.rotation_y;

        building.wall_color = {
            210,
            195,
            165,
            255
        };

        building.roof_color = {
            90,
            70,
            55,
            255
        };

        building.enterable = true;

        settlements.front().buildings.push_back(
            std::move(building)
        );

        return true;
    }

    // ========================================================
    // GAMEPLAY MARKERS
    //
    // Gameplay markers are editor/world data, not render
    // models. They intentionally do not require model geometry.
    // ========================================================

    if (
        asset->category ==
        CreatorAssetCategory::Gameplay
    ) {
        auto& settlements =
            region.editable_settlements();

        if (settlements.empty()) {
            return false;
        }

        world::GameplayMarkerType marker_type =
            world::GameplayMarkerType::LootSpawn;

        if (asset->id == "loot_spawn" || asset->id.starts_with("loot_spawn_")) {
            marker_type =
                world::GameplayMarkerType::LootSpawn;
        } else if (asset->id == "zombie_spawn") {
            marker_type =
                world::GameplayMarkerType::ZombieSpawn;
        } else if (asset->id == "npc_spawn" || asset->id == "npc_spawn_emergency" ||
                   asset->id == "npc_spawn_hostile" || asset->id == "npc_spawn_creature") {
            marker_type =
                world::GameplayMarkerType::NpcSpawn;
        } else if (asset->id == "vehicle_spawn") {
            marker_type =
                world::GameplayMarkerType::VehicleSpawn;
        } else {
            return false;
        }

        const std::string id_prefix =
            "creator_marker_" +
            asset->id +
            "_";

        std::size_t next_id = 1;
        std::string marker_id;

        for (;;) {
            const std::string candidate =
                id_prefix +
                std::to_string(next_id);

            bool exists = false;

            for (
                const world::Settlement& settlement :
                settlements
            ) {
                for (
                    const world::GameplayMarker& marker :
                    settlement.gameplay_markers
                ) {
                    if (marker.id == candidate) {
                        exists = true;
                        break;
                    }
                }

                if (exists) {
                    break;
                }
            }

            if (!exists) {
                marker_id = candidate;
                break;
            }

            ++next_id;
        }

        world::GameplayMarker marker;

        marker.id =
            std::move(marker_id);

        marker.type =
            marker_type;

        marker.position =
            preview_.position;

        marker.size = {
            asset->footprint.width,
            asset->footprint.height,
            asset->footprint.depth
        };

        marker.rotation_y =
            preview_.rotation_y;

        marker.enabled =
            true;

        settlements.front()
            .gameplay_markers
            .push_back(
                std::move(marker)
            );

        return true;
    }

    // Non-building Creator assets require model geometry.
    if (asset->model_path.empty()) {
        return false;
    }

    std::size_t next_creator_asset_id = 1;

    const std::string id_prefix =
        "creator_" +
        asset->id +
        "_";

    std::string creator_asset_id;

    for (;;) {
        const std::string candidate =
            id_prefix +
            std::to_string(
                next_creator_asset_id
            );

        bool already_exists = false;

        for (
            const world::Settlement& settlement :
            region.settlements()
        ) {
            for (
                const world::WorldAsset& existing :
                settlement.assets
            ) {
                if (existing.id == candidate) {
                    already_exists = true;
                    break;
                }
            }

            if (already_exists) {
                break;
            }
        }

        if (!already_exists) {
            creator_asset_id = candidate;
            break;
        }

        ++next_creator_asset_id;
    }

    world::WorldAsset placed;

    placed.id =
        std::move(creator_asset_id);

    switch (asset->category) {
        case CreatorAssetCategory::Road:
            placed.type = world::AssetType::Road;
            break;

        case CreatorAssetCategory::Nature:
            placed.type = world::AssetType::Tree;
            break;

        case CreatorAssetCategory::BuildingPart:
            placed.type = world::AssetType::Wall;
            break;

        case CreatorAssetCategory::Building:
            placed.type = world::AssetType::House;
            break;

        default:
            placed.type = world::AssetType::Sign;
            break;
    }

    placed.model_path = asset->model_path;
    placed.position = preview_.position;

    placed.size = {
        asset->footprint.width,
        asset->footprint.height,
        asset->footprint.depth
    };

    placed.rotation_y = preview_.rotation_y;
    // Road surfaces, markings and flat ground details must not become circular
    // movement blockers under the existing WorldAsset collision system.
    placed.collision = asset->category != CreatorAssetCategory::Road &&
        asset->footprint.height > 0.25F;

    return region.place_world_asset(
        std::move(placed)
    );
}

bool CreatorController::select_world_asset(
    const world::VerdaRegion& region,
    const Vector3 origin,
    const Vector3 direction,
    float& nearest_distance
) {
    bool found = false;

    const Ray ray{
        origin,
        direction
    };

    const auto& settlements =
        region.settlements();

    for (
        std::size_t settlement_index = 0;
        settlement_index < settlements.size();
        ++settlement_index
    ) {
        const auto& settlement =
            settlements[settlement_index];

        for (
            const world::WorldAsset& asset :
            settlement.assets
        ) {
            /*
             * Creator only edits Creator-owned
             * WorldAssets here. Bootstrap Verda
             * assets remain protected.
             */
            if (!asset.id.starts_with("creator_")) {
                continue;
            }

            const float half_x =
                std::max(
                    asset.size.x * 0.5F,
                    0.25F
                );

            const float half_z =
                std::max(
                    asset.size.z * 0.5F,
                    0.25F
                );

            const float height =
                std::max(
                    asset.size.y,
                    0.5F
                );

            const BoundingBox bounds{
                {
                    asset.position.x - half_x,
                    asset.position.y,
                    asset.position.z - half_z
                },
                {
                    asset.position.x + half_x,
                    asset.position.y + height,
                    asset.position.z + half_z
                }
            };

            const RayCollision hit =
                GetRayCollisionBox(
                    ray,
                    bounds
                );

            if (!hit.hit) {
                continue;
            }

            if (
                hit.distance >
                max_select_distance
            ) {
                continue;
            }

            if (
                hit.distance >=
                nearest_distance
            ) {
                continue;
            }

            nearest_distance =
                hit.distance;

            selection_.type =
                CreatorSelectionType::WorldAsset;

            selection_.world_asset_id =
                asset.id;

            selection_.settlement_index =
                settlement_index;

            selection_.position =
                asset.position;

            found = true;
        }
    }

    return found;
}

bool CreatorController::select_gameplay_marker(
    const world::VerdaRegion& region,
    const Vector3 origin,
    const Vector3 direction,
    float& nearest_distance
) {
    bool found = false;

    const Ray ray{
        origin,
        direction
    };

    const auto& settlements =
        region.settlements();

    for (
        std::size_t settlement_index = 0;
        settlement_index < settlements.size();
        ++settlement_index
    ) {
        const auto& settlement =
            settlements[settlement_index];

        for (
            const world::GameplayMarker& marker :
            settlement.gameplay_markers
        ) {
            /*
             * Only Creator-owned markers are editable here.
             * Future world-authored markers stay protected.
             */
            if (
                !marker.id.starts_with(
                    "creator_marker_"
                )
            ) {
                continue;
            }

            const float half_x =
                std::max(
                    marker.size.x * 0.5F,
                    0.35F
                );

            const float half_z =
                std::max(
                    marker.size.z * 0.5F,
                    0.35F
                );

            const float height =
                std::max(
                    marker.size.y,
                    0.75F
                );

            const BoundingBox bounds{
                {
                    marker.position.x - half_x,
                    marker.position.y,
                    marker.position.z - half_z
                },
                {
                    marker.position.x + half_x,
                    marker.position.y + height,
                    marker.position.z + half_z
                }
            };

            const RayCollision hit =
                GetRayCollisionBox(
                    ray,
                    bounds
                );

            if (!hit.hit) {
                continue;
            }

            if (
                hit.distance >
                max_select_distance
            ) {
                continue;
            }

            if (
                hit.distance >=
                nearest_distance
            ) {
                continue;
            }

            nearest_distance =
                hit.distance;

            selection_.type =
                CreatorSelectionType::GameplayMarker;

            selection_.gameplay_marker_id =
                marker.id;

            selection_.settlement_index =
                settlement_index;

            selection_.position =
                marker.position;

            found = true;
        }
    }

    return found;
}

bool CreatorController::delete_selected(
    world::VerdaRegion& region
) {
    bool deleted = false;

    switch (selection_.type) {
        case CreatorSelectionType::Building:
            deleted =
                region.delete_building(
                    selection_.building_id
                );
            break;

        case CreatorSelectionType::Road:
            deleted =
                region.delete_road(
                    selection_.settlement_index,
                    selection_.road_index
                );
            break;

        case CreatorSelectionType::WorldAsset:
            deleted =
                region.delete_world_asset(
                    selection_.world_asset_id
                );
            break;

        case CreatorSelectionType::GameplayMarker: {
            auto& settlements =
                region.editable_settlements();

            if (
                selection_.settlement_index >=
                settlements.size()
            ) {
                break;
            }

            auto& markers =
                settlements[
                    selection_.settlement_index
                ].gameplay_markers;

            const std::size_t before =
                markers.size();

            std::erase_if(
                markers,
                [this](
                    const world::GameplayMarker& marker
                ) {
                    return
                        marker.id ==
                        selection_.gameplay_marker_id;
                }
            );

            deleted =
                markers.size() != before;

            break;
        }

        case CreatorSelectionType::None:
        default:
            break;
    }

    if (deleted) {
        selection_.clear();
    }

    return deleted;
}

bool CreatorController::move_selected(
    world::VerdaRegion& region
) {
    if (!preview_.valid) {
        return false;
    }

    auto& settlements =
        region.editable_settlements();

    if (
        selection_.settlement_index >=
        settlements.size()
    ) {
        return false;
    }

    world::Settlement& settlement =
        settlements[
            selection_.settlement_index
        ];

    if(selection_.type==CreatorSelectionType::Road) {
        if(selection_.road_index>=settlement.roads.size())return false;
        auto& road=settlement.roads[selection_.road_index];
        const auto midpoint=Vector3Scale(Vector3Add(road.start,road.end),.5F);
        const auto offset=Vector3Subtract(preview_.position,midpoint);
        road.start=Vector3Add(road.start,offset);road.end=Vector3Add(road.end,offset);
        selection_.position=preview_.position;return true;
    }

    // --------------------------------------------------------
    // BUILDING
    // --------------------------------------------------------

    if (
        selection_.type ==
        CreatorSelectionType::Building
    ) {
        for (
            world::Building& building :
            settlement.buildings
        ) {
            if (
                building.id !=
                selection_.building_id
            ) {
                continue;
            }

            building.position =
                preview_.position;

            selection_.position =
                building.position;

            return true;
        }

        return false;
    }

    // --------------------------------------------------------
    // GAMEPLAY MARKER
    // --------------------------------------------------------

    if (
        selection_.type ==
        CreatorSelectionType::GameplayMarker
    ) {
        for (
            world::GameplayMarker& marker :
            settlement.gameplay_markers
        ) {
            if (
                marker.id !=
                selection_.gameplay_marker_id
            ) {
                continue;
            }

            marker.position =
                preview_.position;

            selection_.position =
                marker.position;

            return true;
        }

        return false;
    }

    // --------------------------------------------------------
    // WORLD ASSET
    // --------------------------------------------------------

    if (
        selection_.type ==
        CreatorSelectionType::WorldAsset
    ) {
        for (
            world::WorldAsset& asset :
            settlement.assets
        ) {
            if (
                asset.id !=
                selection_.world_asset_id
            ) {
                continue;
            }

            asset.position =
                preview_.position;

            selection_.position =
                asset.position;

            return true;
        }

        return false;
    }

    return false;
}

bool CreatorController::duplicate_selected(
    world::VerdaRegion& region
) {
    auto& settlements =
        region.editable_settlements();

    if (
        selection_.settlement_index >=
        settlements.size()
    ) {
        return false;
    }

    const std::size_t settlement_index =
        selection_.settlement_index;

    world::Settlement& settlement =
        settlements[settlement_index];

    if(selection_.type==CreatorSelectionType::Road) {
        if(selection_.road_index>=settlement.roads.size())return false;
        if(!preview_.valid)return false;
        auto road=settlement.roads[selection_.road_index];
        const auto offset=Vector3Subtract(preview_.position,Vector3Scale(Vector3Add(road.start,road.end),.5F));
        road.start=Vector3Add(road.start,offset);road.end=Vector3Add(road.end,offset);
        settlement.roads.push_back(road);selection_.road_index=settlement.roads.size()-1;
        selection_.position=preview_.position;return true;
    }

    // --------------------------------------------------------
    // GAMEPLAY MARKER
    // --------------------------------------------------------

    if (
        selection_.type ==
        CreatorSelectionType::GameplayMarker
    ) {
        world::GameplayMarker source;
        bool found = false;

        for (
            const world::GameplayMarker& marker :
            settlement.gameplay_markers
        ) {
            if (
                marker.id ==
                selection_.gameplay_marker_id
            ) {
                source = marker;
                found = true;
                break;
            }
        }

        if (!found) {
            return false;
        }

        std::string base_id =
            source.id;

        const std::size_t last_underscore =
            base_id.find_last_of('_');

        if (
            last_underscore !=
            std::string::npos
        ) {
            const std::string suffix =
                base_id.substr(
                    last_underscore + 1
                );

            bool numeric = !suffix.empty();

            for (const char c : suffix) {
                if (
                    c < '0' ||
                    c > '9'
                ) {
                    numeric = false;
                    break;
                }
            }

            if (numeric) {
                base_id.erase(
                    last_underscore + 1
                );
            } else {
                base_id += "_";
            }
        } else {
            base_id += "_";
        }

        std::size_t next_id = 1;
        std::string new_id;

        for (;;) {
            const std::string candidate =
                base_id +
                std::to_string(next_id);

            bool exists = false;

            for (
                const world::Settlement& scan :
                settlements
            ) {
                for (
                    const world::GameplayMarker& marker :
                    scan.gameplay_markers
                ) {
                    if (
                        marker.id ==
                        candidate
                    ) {
                        exists = true;
                        break;
                    }
                }

                if (exists) {
                    break;
                }
            }

            if (!exists) {
                new_id = candidate;
                break;
            }

            ++next_id;
        }

        world::GameplayMarker duplicate =
            source;

        duplicate.id =
            std::move(new_id);

        if (preview_.valid) {
            duplicate.position =
                preview_.position;
        } else {
            duplicate.position.x +=
                1.0F;
        }

        settlement.gameplay_markers.push_back(
            duplicate
        );

        selection_.type =
            CreatorSelectionType::GameplayMarker;

        selection_.gameplay_marker_id =
            duplicate.id;

        selection_.settlement_index =
            settlement_index;

        selection_.position =
            duplicate.position;

        return true;
    }

    // --------------------------------------------------------
    // BUILDING
    // --------------------------------------------------------

    if (
        selection_.type ==
        CreatorSelectionType::Building
    ) {
        world::Building source;
        bool found = false;

        for (
            const world::Building& building :
            settlement.buildings
        ) {
            if (
                building.id ==
                selection_.building_id
            ) {
                source = building;
                found = true;
                break;
            }
        }

        if (!found) {
            return false;
        }

        std::string base_id =
            selection_.building_id;

        const std::size_t last_underscore =
            base_id.find_last_of('_');

        if (
            last_underscore !=
            std::string::npos
        ) {
            base_id =
                base_id.substr(
                    0,
                    last_underscore
                );
        }

        std::size_t next_id = 1;
        std::string new_id;

        for (;;) {
            const std::string candidate =
                base_id +
                "_" +
                std::to_string(next_id);

            bool exists = false;

            for (
                const world::Settlement& current :
                settlements
            ) {
                for (
                    const world::Building& building :
                    current.buildings
                ) {
                    if (
                        building.id ==
                        candidate
                    ) {
                        exists = true;
                        break;
                    }
                }

                if (exists) {
                    break;
                }
            }

            if (!exists) {
                new_id = candidate;
                break;
            }

            ++next_id;
        }

        world::Building duplicate =
            source;

        duplicate.id =
            new_id;

        duplicate.position.x +=
            1.0F;

        duplicate.position.z +=
            1.0F;

        settlement.buildings.push_back(
            duplicate
        );

        selection_.type =
            CreatorSelectionType::Building;

        selection_.building_id =
            duplicate.id;

        selection_.world_asset_id.clear();

        selection_.settlement_index =
            settlement_index;

        selection_.position =
            duplicate.position;

        return true;
    }

    // --------------------------------------------------------
    // WORLD ASSET
    // --------------------------------------------------------

    if (
        selection_.type !=
        CreatorSelectionType::WorldAsset
    ) {
        return false;
    }

    world::WorldAsset source;
    bool found = false;

    for (
        const world::WorldAsset& asset :
        settlement.assets
    ) {
        if (
            asset.id ==
            selection_.world_asset_id
        ) {
            source = asset;
            found = true;
            break;
        }
    }

    if (!found) {
        return false;
    }

    std::string base_id =
        selection_.world_asset_id;

    const std::size_t last_underscore =
        base_id.find_last_of('_');

    if (
        last_underscore !=
        std::string::npos
    ) {
        base_id =
            base_id.substr(
                0,
                last_underscore
            );
    }

    std::size_t next_id = 1;
    std::string new_id;

    for (;;) {
        const std::string candidate =
            base_id +
            "_" +
            std::to_string(next_id);

        bool exists = false;

        for (
            const world::Settlement& current :
            settlements
        ) {
            for (
                const world::WorldAsset& asset :
                current.assets
            ) {
                if (
                    asset.id ==
                    candidate
                ) {
                    exists = true;
                    break;
                }
            }

            if (exists) {
                break;
            }
        }

        if (!exists) {
            new_id = candidate;
            break;
        }

        ++next_id;
    }

    world::WorldAsset duplicate =
        source;

    duplicate.id =
        new_id;

    duplicate.position.x +=
        1.0F;

    duplicate.position.z +=
        1.0F;

    if (
        !region.place_world_asset(
            duplicate,
            settlement_index
        )
    ) {
        return false;
    }

    selection_.type =
        CreatorSelectionType::WorldAsset;

    selection_.building_id.clear();

    selection_.world_asset_id =
        duplicate.id;

    selection_.settlement_index =
        settlement_index;

    selection_.position =
        duplicate.position;

    return true;
}

bool CreatorController::rotate_selected(
    world::VerdaRegion& region,
    const float degrees
) {
    auto& settlements =
        region.editable_settlements();

    if (
        selection_.settlement_index >=
        settlements.size()
    ) {
        return false;
    }

    world::Settlement& settlement =
        settlements[
            selection_.settlement_index
        ];

    if(selection_.type==CreatorSelectionType::Road) {
        if(selection_.road_index>=settlement.roads.size())return false;
        auto& road=settlement.roads[selection_.road_index];
        const auto center=Vector3Scale(Vector3Add(road.start,road.end),.5F);
        road.start=Vector3Add(center,Vector3RotateByAxisAngle(Vector3Subtract(road.start,center),{0,1,0},degrees*DEG2RAD));
        road.end=Vector3Add(center,Vector3RotateByAxisAngle(Vector3Subtract(road.end,center),{0,1,0},degrees*DEG2RAD));
        selection_.position=center;return true;
    }

    const auto normalize_rotation =
        [](float& rotation) {
            rotation += 0.0F;

            while (rotation >= 360.0F) {
                rotation -= 360.0F;
            }

            while (rotation < 0.0F) {
                rotation += 360.0F;
            }
        };

    // --------------------------------------------------------
    // BUILDING
    // --------------------------------------------------------

    if (
        selection_.type ==
        CreatorSelectionType::Building
    ) {
        for (
            world::Building& building :
            settlement.buildings
        ) {
            if (
                building.id !=
                selection_.building_id
            ) {
                continue;
            }

            building.rotation_y += degrees;
            normalize_rotation(
                building.rotation_y
            );

            selection_.position =
                building.position;

            return true;
        }

        return false;
    }

    // --------------------------------------------------------
    // GAMEPLAY MARKER
    // --------------------------------------------------------

    if (
        selection_.type ==
        CreatorSelectionType::GameplayMarker
    ) {
        for (
            world::GameplayMarker& marker :
            settlement.gameplay_markers
        ) {
            if (
                marker.id !=
                selection_.gameplay_marker_id
            ) {
                continue;
            }

            marker.rotation_y += degrees;

            normalize_rotation(
                marker.rotation_y
            );

            selection_.position =
                marker.position;

            return true;
        }

        return false;
    }

    // --------------------------------------------------------
    // WORLD ASSET
    // --------------------------------------------------------

    if (
        selection_.type ==
        CreatorSelectionType::WorldAsset
    ) {
        for (
            world::WorldAsset& asset :
            settlement.assets
        ) {
            if (
                asset.id !=
                selection_.world_asset_id
            ) {
                continue;
            }

            asset.rotation_y += degrees;
            normalize_rotation(
                asset.rotation_y
            );

            selection_.position =
                asset.position;

            return true;
        }

        return false;
    }

    return false;
}

void CreatorController::rotate_preview(
    const float degrees
) {
    state_.rotate(degrees);

    preview_.rotation_y =
        state_.placement_yaw;
}

void CreatorController::
increase_placement_distance() {
    state_.placement_distance =
        std::min(
            maximum_place_distance,
            state_.placement_distance + 1.0F
        );
}

void CreatorController::
decrease_placement_distance() {
    state_.placement_distance =
        std::max(
            minimum_place_distance,
            state_.placement_distance - 1.0F
        );
}

const CreatorSelection&
CreatorController::selection() const {
    return selection_;
}

const CreatorPreview&
CreatorController::preview() const {
    return preview_;
}

void CreatorController::clear_selection() {
    selection_.clear();
}

CreatorState&
CreatorController::state() {
    return state_;
}

const CreatorState&
CreatorController::state() const {
    return state_;
}

void CreatorController::draw_world_overlay(
    const world::VerdaRegion& region
) const {
    if (!state_.enabled) {
        return;
    }

    if (selection_.type ==
        CreatorSelectionType::Building) {
        for (const auto& settlement :
             region.settlements()) {
            for (const auto& building :
                 settlement.buildings) {
                if (building.id !=
                    selection_.building_id) {
                    continue;
                }

                const float ground_y =
                    world::terrain::TerrainHeight::sample(
                        building.position.x,
                        building.position.z
                    );

                const BoundingBox bounds{
                    {
                        building.position.x -
                            building.size.x * 0.5F,
                        ground_y,
                        building.position.z -
                            building.size.z * 0.5F
                    },
                    {
                        building.position.x +
                            building.size.x * 0.5F,
                        ground_y +
                            building.size.y,
                        building.position.z +
                            building.size.z * 0.5F
                    }
                };

                DrawBoundingBox(
                    bounds,
                    YELLOW
                );

                return;
            }
        }
    }

    if (selection_.type ==
        CreatorSelectionType::Road) {
        const auto& settlements =
            region.settlements();

        if (selection_.settlement_index >=
            settlements.size()) {
            return;
        }

        const auto& roads =
            settlements[
                selection_.settlement_index
            ].roads;

        if (selection_.road_index >=
            roads.size()) {
            return;
        }

        const auto& road =
            roads[selection_.road_index];

        DrawLine3D(
            road.start,
            road.end,
            YELLOW
        );
    }

    if (
        selection_.type ==
        CreatorSelectionType::WorldAsset
    ) {
        const auto& settlements =
            region.settlements();

        if (
            selection_.settlement_index <
            settlements.size()
        ) {
            const auto& assets =
                settlements[
                    selection_.settlement_index
                ].assets;

            for (
                const world::WorldAsset& asset :
                assets
            ) {
                if (
                    asset.id !=
                    selection_.world_asset_id
                ) {
                    continue;
                }

                const float width =
                    std::max(
                        asset.size.x,
                        0.25F
                    );

                const float height =
                    std::max(
                        asset.size.y,
                        0.25F
                    );

                const float depth =
                    std::max(
                        asset.size.z,
                        0.25F
                    );

                DrawCubeWires(
                    {
                        asset.position.x,
                        asset.position.y +
                            height * 0.5F,
                        asset.position.z
                    },
                    width,
                    height,
                    depth,
                    YELLOW
                );

                DrawLine3D(
                    asset.position,
                    {
                        asset.position.x,
                        asset.position.y +
                            height + 0.75F,
                        asset.position.z
                    },
                    YELLOW
                );

                break;
            }
        }
    }

    // --------------------------------------------------------
    // GAMEPLAY MARKER DEV GIZMOS
    // --------------------------------------------------------

    for (
        const world::Settlement& settlement :
        region.settlements()
    ) {
        for (
            const world::GameplayMarker& marker :
            settlement.gameplay_markers
        ) {
            if (
                !marker.id.starts_with(
                    "creator_marker_"
                )
            ) {
                continue;
            }

            Color marker_color =
                SKYBLUE;

            switch (marker.type) {
                case world::GameplayMarkerType::LootSpawn:
                    marker_color = GOLD;
                    break;

                case world::GameplayMarkerType::ZombieSpawn:
                    marker_color = RED;
                    break;

                case world::GameplayMarkerType::NpcSpawn:
                    marker_color = SKYBLUE;
                    break;

                case world::GameplayMarkerType::VehicleSpawn:
                    marker_color = PURPLE;
                    break;
            }

            const float width =
                std::max(
                    marker.size.x,
                    0.6F
                );

            const float height =
                std::max(
                    marker.size.y,
                    0.8F
                );

            const float depth =
                std::max(
                    marker.size.z,
                    0.6F
                );

            const Vector3 center{
                marker.position.x,
                marker.position.y +
                    height * 0.5F,
                marker.position.z
            };

            DrawCubeWires(
                center,
                width,
                height,
                depth,
                marker_color
            );

            DrawLine3D(
                marker.position,
                {
                    marker.position.x,
                    marker.position.y +
                        height + 1.0F,
                    marker.position.z
                },
                marker_color
            );

            DrawSphereWires(
                {
                    marker.position.x,
                    marker.position.y +
                        height + 1.0F,
                    marker.position.z
                },
                0.22F,
                6,
                8,
                marker_color
            );

            // Direction indicator.
            const float radians =
                marker.rotation_y *
                DEG2RAD;

            const Vector3 direction_end{
                marker.position.x +
                    std::sin(radians) * 1.25F,
                marker.position.y + 0.10F,
                marker.position.z +
                    std::cos(radians) * 1.25F
            };

            DrawLine3D(
                {
                    marker.position.x,
                    marker.position.y + 0.10F,
                    marker.position.z
                },
                direction_end,
                marker_color
            );

            if (
                selection_.type ==
                    CreatorSelectionType::GameplayMarker &&
                selection_.gameplay_marker_id ==
                    marker.id
            ) {
                DrawCubeWires(
                    center,
                    width + 0.20F,
                    height + 0.20F,
                    depth + 0.20F,
                    YELLOW
                );

                DrawSphereWires(
                    {
                        marker.position.x,
                        marker.position.y +
                            height + 1.0F,
                        marker.position.z
                    },
                    0.32F,
                    8,
                    10,
                    YELLOW
                );
            }
        }
    }

    if (preview_.valid) {
        DrawCubeWires(
            {
                preview_.position.x,
                preview_.position.y + 1.0F,
                preview_.position.z
            },
            2.0F,
            2.0F,
            2.0F,
            GREEN
        );
    }
}

void CreatorController::draw_hud() const {
    if (!state_.enabled) {
        return;
    }

    DrawRectangle(
        8,
        250,
        360,
        128,
        Fade(
            BLACK,
            0.70F
        )
    );

    DrawText(
        "OUTLAND CREATOR [DEV]",
        18,
        260,
        20,
        YELLOW
    );

    const char* selected = "NONE";

    if (selection_.type ==
        CreatorSelectionType::Building) {
        selected =
            selection_.building_id.c_str();
    } else if (
        selection_.type ==
        CreatorSelectionType::Road
    ) {
        selected = "ROAD";
    } else if (
        selection_.type ==
        CreatorSelectionType::WorldAsset
    ) {
        selected =
            selection_.world_asset_id.c_str();
    } else if (
        selection_.type ==
        CreatorSelectionType::GameplayMarker
    ) {
        selected =
            selection_.gameplay_marker_id.c_str();
    }

    char buffer[160];

    std::snprintf(
        buffer,
        sizeof(buffer),
        "SELECTED: %s",
        selected
    );

    DrawText(
        buffer,
        18,
        288,
        16,
        RAYWHITE
    );

    std::snprintf(
        buffer,
        sizeof(buffer),
        "PLACE DIST %.1f   ROT %.0f",
        state_.placement_distance,
        state_.placement_yaw
    );

    DrawText(
        buffer,
        18,
        312,
        16,
        LIGHTGRAY
    );

    DrawText(
        "Q/E ROTATE  +/- DISTANCE",
        18,
        336,
        14,
        LIGHTGRAY
    );

    DrawText(
        "DELETE/BACKSPACE = DEMOLISH",
        18,
        356,
        14,
        ORANGE
    );
}

} // namespace outland::creator

#endif // OUTLAND_DEV_TOOLS
