#pragma once

#ifdef OUTLAND_DEV_TOOLS

#include "outland/creator/CreatorAssetRegistry.hpp"
#include "outland/creator/CreatorController.hpp"

#include <raylib.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace outland::creator {

// ============================================================
// CREATOR TOUCH TOOL
// ============================================================

enum class CreatorTouchTool {
    Select,
    Place,
    Move,
    Rotate,
    Duplicate,
    Delete
};

// ============================================================
// CREATOR TOUCH ACTIONS
//
// UI reports Creator intent.
// CreatorController / world code performs the actual operation.
// ============================================================

struct CreatorTouchActions {
    bool select{false};
    bool place{false};

    bool move{false};
    bool rotate{false};

    bool duplicate{false};
    bool erase{false};

    bool undo{false};
    bool redo{false};

    bool save{false};
    bool load{false};

    bool toggle_fly{false};

    void clear() {
        *this = CreatorTouchActions{};
    }
};

// ============================================================
// GAMEPLAY TOUCH BUTTON
//
// These are gameplay-side controls.
//
// CreatorTouchUI gathers touch intent only.
// Player / Renderer systems decide what the intent actually does.
// ============================================================

enum class GameplayTouchButton {
    Jump,
    Reload,
    Sprint,
    Crouch,
    Inventory,
    Aim,
    Fire,
    Interact
};

// ============================================================
// TOUCH JOYSTICK STATE
//
// Normalized output range:
//
// x = -1 ... +1
// y = -1 ... +1
//
// finger_id is reserved for multi-touch ownership.
// ============================================================

struct TouchJoystickState {
    Vector2 value{
        0.0F,
        0.0F
    };

    bool active{false};

    int finger_id{-1};

    Vector2 origin{
        0.0F,
        0.0F
    };

    Vector2 knob{
        0.0F,
        0.0F
    };

    void clear() {
        value = {
            0.0F,
            0.0F
        };

        active = false;
        finger_id = -1;

        origin = {
            0.0F,
            0.0F
        };

        knob = {
            0.0F,
            0.0F
        };
    }
};

// ============================================================
// GAMEPLAY TOUCH STATE
//
// One clean bundle consumed by the game.
//
// This keeps touch implementation separate from gameplay logic.
// Keyboard / controller / touch can eventually feed the same
// higher-level player input system.
// ============================================================

struct GameplayTouchState {
    float move_x{0.0F};
    float move_y{0.0F};

    float look_x{0.0F};
    float look_y{0.0F};

    bool jump{false};
    bool reload{false};

    bool sprint{false};
    bool crouch{false};

    bool inventory{false};

    bool aim{false};
    bool fire{false};

    bool interact{false};

    void clear_frame_actions() {
        jump = false;
        reload = false;
        inventory = false;
        interact = false;
    }

    void clear() {
        *this = GameplayTouchState{};
    }
};

// ============================================================
// TOUCH UI SETTINGS
//
// These values become the foundation for the future in-game
// mobile control settings screen.
// ============================================================

struct TouchUISettings {
    float joystick_scale{1.0F};
    float button_scale{1.0F};

    float opacity{0.72F};

    float look_sensitivity{1.0F};

    bool gameplay_controls_visible{true};
    bool creator_controls_visible{true};

    void clamp() {
        joystick_scale =
            std::clamp(
                joystick_scale,
                0.65F,
                1.60F
            );

        button_scale =
            std::clamp(
                button_scale,
                0.65F,
                1.60F
            );

        opacity =
            std::clamp(
                opacity,
                0.25F,
                1.0F
            );

        look_sensitivity =
            std::clamp(
                look_sensitivity,
                0.25F,
                3.0F
            );
    }
};

// ============================================================
// CREATOR TOUCH UI
// ============================================================

class CreatorTouchUI {
public:
    CreatorTouchUI() = default;

    void update(
        CreatorController& controller,
        int screen_width,
        int screen_height
    );

    void draw(
        const CreatorController& controller,
        int screen_width,
        int screen_height
    ) const;

    // --------------------------------------------------------
    // CREATOR ACTIONS
    // --------------------------------------------------------

    [[nodiscard]]
    const CreatorTouchActions&
    actions() const;

    void clear_actions();

    // --------------------------------------------------------
    // GAMEPLAY TOUCH OUTPUT
    //
    // Added for the OUTLAND mobile control rebuild.
    //
    // These accessors are inline so the existing .cpp can keep
    // building while we wire the implementation incrementally.
    // --------------------------------------------------------

    [[nodiscard]]
    const GameplayTouchState&
    gameplay_state() const {
        return gameplay_state_;
    }

    [[nodiscard]]
    const TouchJoystickState&
    move_stick() const {
        return move_stick_;
    }

    [[nodiscard]]
    const TouchJoystickState&
    look_stick() const {
        return look_stick_;
    }

    void clear_gameplay_state() {
        gameplay_state_.clear();
        move_stick_.clear();
        look_stick_.clear();
    }

    // --------------------------------------------------------
    // MOBILE UI SETTINGS
    // --------------------------------------------------------

    [[nodiscard]]
    const TouchUISettings&
    touch_settings() const {
        return touch_settings_;
    }

    TouchUISettings&
    touch_settings() {
        return touch_settings_;
    }

    void set_joystick_scale(
        const float scale
    ) {
        touch_settings_.joystick_scale =
            scale;

        touch_settings_.clamp();
    }

    void set_button_scale(
        const float scale
    ) {
        touch_settings_.button_scale =
            scale;

        touch_settings_.clamp();
    }

    void set_touch_opacity(
        const float opacity
    ) {
        touch_settings_.opacity =
            opacity;

        touch_settings_.clamp();
    }

    void set_look_sensitivity(
        const float sensitivity
    ) {
        touch_settings_.look_sensitivity =
            sensitivity;

        touch_settings_.clamp();
    }

    // --------------------------------------------------------
    // INVENTORY DRAWER
    // --------------------------------------------------------

    [[nodiscard]]
    bool inventory_open() const;

    void set_inventory_open(bool open);

    void toggle_inventory();

    // --------------------------------------------------------
    // ACTIVE TOOL
    // --------------------------------------------------------

    [[nodiscard]]
    CreatorTouchTool active_tool() const;

    void set_active_tool(
        CreatorTouchTool tool
    );

    // --------------------------------------------------------
    // UI HIT PROTECTION
    //
    // True when a screen-space point belongs to Creator UI.
    // World picking must ignore these positions.
    // --------------------------------------------------------

    [[nodiscard]]
    bool pointer_over_ui(
        Vector2 position,
        int screen_width,
        int screen_height
    ) const;

private:
    // ========================================================
    // CREATOR STATE
    // ========================================================

    CreatorTouchActions actions_{};

    CreatorTouchTool active_tool_{
        CreatorTouchTool::Place
    };

    bool inventory_open_{false};

    CreatorAssetCategory drawer_category_{
        CreatorAssetCategory::Building
    };

    std::size_t drawer_page_{0};

    // ========================================================
    // MOBILE GAMEPLAY TOUCH STATE
    //
    // Foundation only in 32A.
    //
    // 32B will make these consume real multi-touch input.
    // ========================================================

    GameplayTouchState gameplay_state_{};

    TouchJoystickState move_stick_{};
    TouchJoystickState look_stick_{};

    TouchUISettings touch_settings_{};

    // ========================================================
    // ASSET LIBRARY V2
    //
    // Designed for hundreds of assets.
    // Registry remains source of truth.
    // These fields only control browsing.
    // ========================================================

    std::string asset_search_{};
    std::string asset_tag_filter_{};

    bool search_focused_{false};

    bool favorites_only_{false};
    bool recent_only_{false};

    std::vector<std::string>
        favorite_asset_ids_{};

    std::vector<std::string>
        recent_asset_ids_{};

    static constexpr std::size_t
        max_recent_assets = 16;

    static constexpr std::size_t
        library_columns = 4;

    static constexpr std::size_t
        library_rows = 3;

    static constexpr std::size_t
        assets_per_page =
            library_columns *
            library_rows;

    // ========================================================
    // CREATOR LAYOUT
    // ========================================================

    [[nodiscard]]
    Rectangle inventory_button(
        int screen_width,
        int screen_height
    ) const;

    [[nodiscard]]
    Rectangle save_button(
        int screen_width,
        int screen_height
    ) const;

    [[nodiscard]]
    Rectangle fly_button(
        int screen_width,
        int screen_height
    ) const;

    [[nodiscard]]
    Rectangle tool_button(
        CreatorTouchTool tool,
        int screen_width,
        int screen_height
    ) const;

    [[nodiscard]]
    Rectangle hotbar_slot(
        std::size_t slot,
        int screen_width,
        int screen_height
    ) const;

    [[nodiscard]]
    Rectangle drawer_panel(
        int screen_width,
        int screen_height
    ) const;

    [[nodiscard]]
    Rectangle category_button(
        std::size_t category_index,
        int screen_width,
        int screen_height
    ) const;

    [[nodiscard]]
    Rectangle asset_button(
        std::size_t visible_index,
        int screen_width,
        int screen_height
    ) const;

    [[nodiscard]]
    Rectangle previous_page_button(
        int screen_width,
        int screen_height
    ) const;

    [[nodiscard]]
    Rectangle next_page_button(
        int screen_width,
        int screen_height
    ) const;

    // ========================================================
    // GAMEPLAY TOUCH LAYOUT
    //
    // These declarations intentionally live beside Creator
    // layout instead of replacing it.
    //
    // This lets DEV Creator controls coexist with normal
    // OUTLAND movement/combat controls.
    // ========================================================

    [[nodiscard]]
    Vector2 move_stick_center(
        int screen_width,
        int screen_height
    ) const;

    [[nodiscard]]
    Vector2 look_stick_center(
        int screen_width,
        int screen_height
    ) const;

    [[nodiscard]]
    float joystick_radius(
        int screen_width,
        int screen_height
    ) const;

    [[nodiscard]]
    Rectangle gameplay_button(
        GameplayTouchButton button,
        int screen_width,
        int screen_height
    ) const;

    // ========================================================
    // INPUT
    // ========================================================

    [[nodiscard]]
    Rectangle search_box(
        int screen_width,
        int screen_height
    ) const;

    [[nodiscard]]
    Rectangle favorites_filter_button(
        int screen_width,
        int screen_height
    ) const;

    [[nodiscard]]
    Rectangle recent_filter_button(
        int screen_width,
        int screen_height
    ) const;

    [[nodiscard]]
    std::vector<
        const CreatorAssetDefinition*
    >
    filtered_assets(
        const CreatorController& controller
    ) const;

    void update_search_input();

    [[nodiscard]]
    bool is_favorite(
        const std::string& asset_id
    ) const;

    void toggle_favorite(
        const std::string& asset_id
    );

    void remember_recent(
        const std::string& asset_id
    );

    [[nodiscard]]
    bool pressed(
        Rectangle rectangle
    ) const;

    void update_toolbar(
        CreatorController& controller,
        int screen_width,
        int screen_height
    );

    void update_hotbar(
        CreatorController& controller,
        int screen_width,
        int screen_height
    );

    void update_inventory(
        CreatorController& controller,
        int screen_width,
        int screen_height
    );

    // --------------------------------------------------------
    // GAMEPLAY INPUT
    //
    // Implementation lands in CreatorTouchUI.cpp next.
    // --------------------------------------------------------

    void update_gameplay_controls(
        int screen_width,
        int screen_height
    );

    void update_joystick(
        TouchJoystickState& stick,
        Vector2 center,
        float radius,
        int finger_id,
        Vector2 touch_position
    );

    // ========================================================
    // DRAW
    // ========================================================

    void draw_toolbar(
        const CreatorController& controller,
        int screen_width,
        int screen_height
    ) const;

    void draw_hotbar(
        const CreatorController& controller,
        int screen_width,
        int screen_height
    ) const;

    void draw_inventory(
        const CreatorController& controller,
        int screen_width,
        int screen_height
    ) const;

    void draw_button(
        Rectangle rectangle,
        const char* label,
        bool active
    ) const;

    void draw_asset_card(
        Rectangle rectangle,
        const CreatorAssetDefinition& asset,
        bool selected
    ) const;

    // --------------------------------------------------------
    // GAMEPLAY TOUCH DRAWING
    // --------------------------------------------------------

    void draw_gameplay_controls(
        int screen_width,
        int screen_height
    ) const;

    void draw_joystick(
        const TouchJoystickState& stick,
        Vector2 center,
        float radius,
        const char* label
    ) const;

    void draw_gameplay_button(
        GameplayTouchButton button,
        Rectangle rectangle,
        const char* label,
        bool active
    ) const;
};

} // namespace outland::creator

#endif // OUTLAND_DEV_TOOLS
