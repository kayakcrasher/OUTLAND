#pragma once

#ifdef OUTLAND_DEV_TOOLS

#include "outland/creator/CreatorAssetRegistry.hpp"
#include "outland/creator/CreatorState.hpp"
#include "outland/world/VerdaRegion.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <string>

namespace outland::creator {

// ============================================================
// CREATOR SELECTION
// ============================================================

enum class CreatorSelectionType {
    None,
    Building,
    Road,
    WorldAsset,
    GameplayMarker
};

struct CreatorSelection {
    CreatorSelectionType type{
        CreatorSelectionType::None
    };

    std::string building_id{};
    std::string world_asset_id{};
    std::string gameplay_marker_id{};

    std::size_t settlement_index{0};
    std::size_t road_index{0};

    Vector3 position{
        0.0F,
        0.0F,
        0.0F
    };

    [[nodiscard]]
    bool valid() const {
        return type != CreatorSelectionType::None;
    }

    void clear() {
        type = CreatorSelectionType::None;

        building_id.clear();
        world_asset_id.clear();
        gameplay_marker_id.clear();

        settlement_index = 0;
        road_index = 0;

        position = {
            0.0F,
            0.0F,
            0.0F
        };
    }
};

// ============================================================
// PLACEMENT PREVIEW
// ============================================================

struct CreatorPreview {
    bool valid{false};

    Vector3 position{
        0.0F,
        0.0F,
        0.0F
    };

    float rotation_y{0.0F};
};

// ============================================================
// CREATOR CONTROLLER
// ============================================================

class CreatorController {
public:
    static constexpr std::size_t hotbar_size = 8;

    CreatorController();

    // --------------------------------------------------------
    // MODE
    // --------------------------------------------------------

    void set_enabled(bool enabled);

    [[nodiscard]]
    bool enabled() const;

    // --------------------------------------------------------
    // FRAME
    // --------------------------------------------------------

    void update(
        world::VerdaRegion& region,
        Vector3 camera_position,
        Vector3 camera_forward
    );

    void draw_world_overlay(
        const world::VerdaRegion& region
    ) const;

    void draw_hud() const;

    // --------------------------------------------------------
    // WORLD SELECTION
    // --------------------------------------------------------

    [[nodiscard]]
    const CreatorSelection&
    selection() const;

    [[nodiscard]]
    const CreatorPreview&
    preview() const;

    void clear_selection();

    bool select_target(
        const world::VerdaRegion& region,
        Vector3 origin,
        Vector3 direction
    );

    bool place_selected(
        world::VerdaRegion& region
    );

    bool delete_selected(
        world::VerdaRegion& region
    );

    bool rotate_selected(
        world::VerdaRegion& region,
        float degrees
    );

    bool duplicate_selected(
        world::VerdaRegion& region
    );

    bool move_selected(
        world::VerdaRegion& region
    );

    // --------------------------------------------------------
    // ASSET REGISTRY
    // --------------------------------------------------------

    [[nodiscard]]
    const CreatorAssetRegistry&
    registry() const;

    [[nodiscard]]
    CreatorAssetRegistry&
    registry();

    [[nodiscard]]
    const CreatorAssetDefinition*
    selected_asset() const;

    [[nodiscard]]
    std::size_t
    selected_asset_index() const;

    bool select_asset(
        std::size_t registry_index
    );

    void select_next_asset();

    void select_previous_asset();

    // --------------------------------------------------------
    // CATEGORY BROWSING
    // --------------------------------------------------------

    [[nodiscard]]
    CreatorAssetCategory
    selected_category() const;

    void set_category(
        CreatorAssetCategory category
    );

    void next_category();

    void previous_category();

    // --------------------------------------------------------
    // HOTBAR
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t
    selected_hotbar_slot() const;

    [[nodiscard]]
    const std::array<std::size_t, hotbar_size>&
    hotbar() const;

    bool select_hotbar_slot(
        std::size_t slot
    );

    bool assign_hotbar_slot(
        std::size_t slot,
        std::size_t registry_index
    );

    // --------------------------------------------------------
    // PREVIEW
    // --------------------------------------------------------

    void rotate_preview(float degrees);

    void increase_placement_distance();

    void decrease_placement_distance();

    // --------------------------------------------------------
    // STATE
    // --------------------------------------------------------

    [[nodiscard]]
    CreatorState&
    state();

    [[nodiscard]]
    const CreatorState&
    state() const;

private:
    CreatorState state_{};

    CreatorSelection selection_{};
    CreatorPreview preview_{};

    CreatorAssetRegistry registry_{};

    CreatorAssetCategory selected_category_{
        CreatorAssetCategory::Building
    };

    std::size_t selected_asset_index_{0};
    std::size_t selected_hotbar_slot_{0};

    std::array<
        std::size_t,
        hotbar_size
    > hotbar_{
        0, 1, 2, 3,
        4, 5, 6, 7
    };

    // --------------------------------------------------------
    // INTERNAL UPDATE
    // --------------------------------------------------------

    void update_preview(
        Vector3 camera_position,
        Vector3 camera_forward
    );

    // --------------------------------------------------------
    // INTERNAL WORLD PICKING
    // --------------------------------------------------------

    bool select_building(
        const world::VerdaRegion& region,
        Vector3 origin,
        Vector3 direction,
        float& nearest_distance
    );

    bool select_road(
        const world::VerdaRegion& region,
        Vector3 origin,
        Vector3 direction,
        float& nearest_distance
    );

    bool select_world_asset(
        const world::VerdaRegion& region,
        Vector3 origin,
        Vector3 direction,
        float& nearest_distance
    );


    bool select_gameplay_marker(
        const world::VerdaRegion& region,
        Vector3 origin,
        Vector3 direction,
        float& nearest_distance
    );

    // --------------------------------------------------------
    // INTERNAL INVENTORY HELPERS
    // --------------------------------------------------------

    [[nodiscard]]
    std::size_t find_next_asset_in_category(
        std::size_t start,
        int direction
    ) const;

    void sync_selected_asset_to_hotbar();
};

} // namespace outland::creator

#endif // OUTLAND_DEV_TOOLS
