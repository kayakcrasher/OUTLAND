#pragma once

#ifdef OUTLAND_DEV_TOOLS

#include "outland/creator/CreatorAssetRegistry.hpp"
#include "outland/creator/CreatorController.hpp"

#include <raylib.h>

#include <cstddef>
#include <optional>
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
// UI reports intent.
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
    float fly_vertical{0};

    void clear() {
        *this = CreatorTouchActions{};
    }
};

// ============================================================
// CREATOR TOUCH UI
// ============================================================

enum class BuilderControl { Undo, Redo, Load, Ground, Grid, Near, Far, Lower, Raise, Up, Down, All, Search };

class CreatorTouchUI {
public:
    CreatorTouchUI() = default;
    Rectangle control_button(BuilderControl control,int width,int height) const;
    void set_search(std::string query) {search_=std::move(query);drawer_page_=0;}
    void show_all_assets() {all_categories_=true;pack_filter_=0;drawer_page_=0;}
    std::vector<const CreatorAssetDefinition*> drawer_assets(const CreatorController& controller) const;
    void set_status(std::string text) {status_=std::move(text);}

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
    // ACTIONS
    // --------------------------------------------------------

    [[nodiscard]]
    const CreatorTouchActions&
    actions() const;

    void clear_actions();

    [[nodiscard]] bool owns_point(Vector2 point, int width, int height) const;

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

private:
    CreatorTouchActions actions_{};
    std::vector<int> previous_touches_;
    std::vector<Vector2> presses_;

    CreatorTouchTool active_tool_{
        CreatorTouchTool::Place
    };

    bool inventory_open_{false};

    CreatorAssetCategory drawer_category_{
        CreatorAssetCategory::Building
    };

    std::size_t drawer_page_{0};
    std::string search_,status_;
    bool all_categories_{true},search_open_{false};
    float input_scale_{1};
    int window_width_{0},window_height_{0};
    Vector2 logical_point(Vector2 point) const {return {point.x/input_scale_,point.y/input_scale_};}
    int vertical_owner_{-1};
    float vertical_direction_{0};
    unsigned pack_filter_{0}; // All, urban, characters, survival, industrial, vehicles.

    // ========================================================
    // LAYOUT
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

    [[nodiscard]] Rectangle page_button(bool next, int width, int height) const;
    [[nodiscard]] Rectangle pack_button(int width, int height) const;

    // ========================================================
    // INPUT
    // ========================================================

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
};

} // namespace outland::creator

#endif // OUTLAND_DEV_TOOLS
