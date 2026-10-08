#include "outland/creator/CreatorTouchUI.hpp"

#ifdef OUTLAND_DEV_TOOLS

#include <array>
#include <cstdio>

namespace outland::creator {

namespace {

constexpr std::size_t category_count = 7;
constexpr std::size_t assets_per_page = 8;

constexpr std::array<
    CreatorAssetCategory,
    category_count
> categories{{
    CreatorAssetCategory::Building,
    CreatorAssetCategory::BuildingPart,
    CreatorAssetCategory::Nature,
    CreatorAssetCategory::Road,
    CreatorAssetCategory::Prop,
    CreatorAssetCategory::Vehicle,
    CreatorAssetCategory::Gameplay
}};

const char* tool_name(
    const CreatorTouchTool tool
) {
    switch (tool) {
        case CreatorTouchTool::Select:    return "SELECT";
        case CreatorTouchTool::Place:     return "PLACE";
        case CreatorTouchTool::Move:      return "MOVE";
        case CreatorTouchTool::Rotate:    return "ROTATE";
        case CreatorTouchTool::Duplicate: return "DUP";
        case CreatorTouchTool::Delete:    return "DELETE";
    }

    return "?";
}

} // namespace

const CreatorTouchActions&
CreatorTouchUI::actions() const {
    return actions_;
}

void CreatorTouchUI::clear_actions() {
    actions_.clear();
}

bool CreatorTouchUI::inventory_open() const {
    return inventory_open_;
}

void CreatorTouchUI::set_inventory_open(
    const bool open
) {
    inventory_open_ = open;
}

void CreatorTouchUI::toggle_inventory() {
    inventory_open_ = !inventory_open_;
}

CreatorTouchTool
CreatorTouchUI::active_tool() const {
    return active_tool_;
}

void CreatorTouchUI::set_active_tool(
    const CreatorTouchTool tool
) {
    active_tool_ = tool;
}

Rectangle CreatorTouchUI::inventory_button(
    const int screen_width,
    const int screen_height
) const {
    return {
        static_cast<float>(screen_width - 126),
        static_cast<float>(screen_height - 132),
        116.0F,
        48.0F
    };
}

Rectangle CreatorTouchUI::save_button(
    const int screen_width,
    const int
) const {
    return {
        static_cast<float>(screen_width - 108),
        92.0F,
        98.0F,
        44.0F
    };
}

Rectangle CreatorTouchUI::fly_button(
    const int screen_width,
    const int
) const {
    return {
        static_cast<float>(screen_width - 108),
        142.0F,
        98.0F,
        44.0F
    };
}

Rectangle CreatorTouchUI::tool_button(
    const CreatorTouchTool tool,
    const int screen_width,
    const int screen_height
) const {
    constexpr float width = 100.0F;
    constexpr float height = 44.0F;

    switch (tool) {
        case CreatorTouchTool::Select:
            return {
                10.0F,
                static_cast<float>(screen_height - 278),
                width,
                height
            };

        case CreatorTouchTool::Move:
            return {
                10.0F,
                static_cast<float>(screen_height - 228),
                width,
                height
            };

        case CreatorTouchTool::Delete:
            return {
                10.0F,
                static_cast<float>(screen_height - 178),
                width,
                height
            };

        case CreatorTouchTool::Place:
            return {
                static_cast<float>(screen_width - 110),
                static_cast<float>(screen_height - 278),
                width,
                height
            };

        case CreatorTouchTool::Rotate:
            return {
                static_cast<float>(screen_width - 110),
                static_cast<float>(screen_height - 228),
                width,
                height
            };

        case CreatorTouchTool::Duplicate:
            return {
                static_cast<float>(screen_width - 110),
                static_cast<float>(screen_height - 178),
                width,
                height
            };
    }

    return {};
}

Rectangle CreatorTouchUI::hotbar_slot(
    const std::size_t slot,
    const int screen_width,
    const int screen_height
) const {
    constexpr float slot_size = 52.0F;
    constexpr float gap = 5.0F;

    const float total_width =
        static_cast<float>(CreatorController::CreatorController::hotbar_size) * slot_size +
        static_cast<float>(CreatorController::CreatorController::hotbar_size - 1) * gap;

    const float start_x =
        (
            static_cast<float>(screen_width) -
            total_width
        ) * 0.5F;

    return {
        start_x +
            static_cast<float>(slot) *
            (slot_size + gap),
        static_cast<float>(screen_height) - 66.0F,
        slot_size,
        slot_size
    };
}

bool CreatorTouchUI::pressed(
    const Rectangle rectangle
) const {
    const int touch_count =
        GetTouchPointCount();

    for (
        int index = 0;
        index < touch_count;
        ++index
    ) {
        const Vector2 position =
            GetTouchPosition(index);

        if (
            CheckCollisionPointRec(
                position,
                rectangle
            )
        ) {
            return true;
        }
    }

    if (
        IsMouseButtonPressed(
            MOUSE_BUTTON_LEFT
        )
    ) {
        return CheckCollisionPointRec(
            GetMousePosition(),
            rectangle
        );
    }

    return false;
}


// ============================================================
// FRAME UPDATE
// ============================================================

void CreatorTouchUI::update(
    CreatorController& controller,
    const int screen_width,
    const int screen_height
) {
    actions_.clear();

    if (!controller.enabled()) {
        return;
    }

    update_toolbar(
        controller,
        screen_width,
        screen_height
    );

    update_hotbar(
        controller,
        screen_width,
        screen_height
    );

    if (inventory_open_) {
        update_inventory(
            controller,
            screen_width,
            screen_height
        );
    }
}

// ============================================================
// TOOLBAR INPUT
// ============================================================

void CreatorTouchUI::update_toolbar(
    CreatorController& controller,
    const int screen_width,
    const int screen_height
) {
    if (
        pressed(
            inventory_button(
                screen_width,
                screen_height
            )
        )
    ) {
        toggle_inventory();
        return;
    }

    if (
        pressed(
            save_button(
                screen_width,
                screen_height
            )
        )
    ) {
        actions_.save = true;
        return;
    }

    if (
        pressed(
            fly_button(
                screen_width,
                screen_height
            )
        )
    ) {
        actions_.toggle_fly = true;

        controller.state().flying =
            !controller.state().flying;

        controller.state().noclip =
            controller.state().flying;

        return;
    }

    constexpr std::array<
        CreatorTouchTool,
        6
    > tools{{
        CreatorTouchTool::Select,
        CreatorTouchTool::Move,
        CreatorTouchTool::Delete,
        CreatorTouchTool::Place,
        CreatorTouchTool::Rotate,
        CreatorTouchTool::Duplicate
    }};

    for (const auto tool : tools) {
        if (
            !pressed(
                tool_button(
                    tool,
                    screen_width,
                    screen_height
                )
            )
        ) {
            continue;
        }

        active_tool_ = tool;

        switch (tool) {
            case CreatorTouchTool::Select:
                actions_.select = true;
                break;

            case CreatorTouchTool::Place:
                actions_.place = true;
                break;

            case CreatorTouchTool::Move:
                actions_.move = true;
                break;

            case CreatorTouchTool::Rotate:
                actions_.rotate = true;

                controller.rotate_preview(
                    15.0F
                );
                break;

            case CreatorTouchTool::Duplicate:
                actions_.duplicate = true;
                break;

            case CreatorTouchTool::Delete:
                actions_.erase = true;
                break;
        }

        return;
    }
}

// ============================================================
// HOTBAR INPUT
// ============================================================

void CreatorTouchUI::update_hotbar(
    CreatorController& controller,
    const int screen_width,
    const int screen_height
) {
    if (inventory_open_) {
        return;
    }

    for (
        std::size_t slot = 0;
        slot < CreatorController::CreatorController::hotbar_size;
        ++slot
    ) {
        if (
            !pressed(
                hotbar_slot(
                    slot,
                    screen_width,
                    screen_height
                )
            )
        ) {
            continue;
        }

        controller.select_hotbar_slot(
            slot
        );

        active_tool_ =
            CreatorTouchTool::Place;

        return;
    }
}

// ============================================================
// INVENTORY LAYOUT
// ============================================================

Rectangle CreatorTouchUI::drawer_panel(
    const int screen_width,
    const int screen_height
) const {
    return {
        16.0F,
        72.0F,
        static_cast<float>(
            screen_width
        ) - 32.0F,
        static_cast<float>(
            screen_height
        ) - 154.0F
    };
}

Rectangle CreatorTouchUI::category_button(
    const std::size_t category_index,
    const int screen_width,
    const int
) const {
    constexpr float margin = 22.0F;
    constexpr float gap = 4.0F;

    const float available =
        static_cast<float>(
            screen_width
        ) - margin * 2.0F;

    const float width =
        (
            available -
            gap *
            static_cast<float>(
                category_count - 1
            )
        ) /
        static_cast<float>(
            category_count
        );

    return {
        margin +
            static_cast<float>(
                category_index
            ) * (width + gap),
        86.0F,
        width,
        38.0F
    };
}

Rectangle CreatorTouchUI::asset_button(
    const std::size_t visible_index,
    const int screen_width,
    const int
) const {
    constexpr std::size_t columns = 4;
    constexpr float gap = 8.0F;
    constexpr float card_height = 92.0F;

    const std::size_t column =
        visible_index % columns;

    const std::size_t row =
        visible_index / columns;

    const float panel_width =
        static_cast<float>(
            screen_width
        ) - 44.0F;

    const float card_width =
        (
            panel_width -
            gap * 3.0F
        ) / 4.0F;

    return {
        22.0F +
            static_cast<float>(
                column
            ) * (card_width + gap),

        142.0F +
            static_cast<float>(
                row
            ) * (card_height + gap),

        card_width,
        card_height
    };
}

// ============================================================
// INVENTORY INPUT
// ============================================================

void CreatorTouchUI::update_inventory(
    CreatorController& controller,
    const int screen_width,
    const int screen_height
) {
    for (
        std::size_t index = 0;
        index < categories.size();
        ++index
    ) {
        if (
            !pressed(
                category_button(
                    index,
                    screen_width,
                    screen_height
                )
            )
        ) {
            continue;
        }

        drawer_category_ =
            categories[index];

        drawer_page_ = 0;

        controller.set_category(
            drawer_category_
        );

        return;
    }

    const auto category_assets =
        controller.registry().category(
            drawer_category_
        );

    const std::size_t first =
        drawer_page_ *
        assets_per_page;

    for (
        std::size_t visible = 0;
        visible < assets_per_page;
        ++visible
    ) {
        const std::size_t category_index =
            first + visible;

        if (
            category_index >=
            category_assets.size()
        ) {
            break;
        }

        if (
            !pressed(
                asset_button(
                    visible,
                    screen_width,
                    screen_height
                )
            )
        ) {
            continue;
        }

        const auto* chosen =
            category_assets[
                category_index
            ];

        const auto& all =
            controller.registry().assets();

        for (
            std::size_t registry_index = 0;
            registry_index < all.size();
            ++registry_index
        ) {
            if (
                &all[registry_index] !=
                chosen
            ) {
                continue;
            }

            controller.assign_hotbar_slot(
                controller.selected_hotbar_slot(),
                registry_index
            );

            inventory_open_ = false;

            active_tool_ =
                CreatorTouchTool::Place;

            return;
        }
    }
}


// ============================================================
// GENERIC BUTTON DRAW
// ============================================================

void CreatorTouchUI::draw_button(
    const Rectangle rectangle,
    const char* label,
    const bool active
) const {
    const Color fill =
        active
            ? Fade(YELLOW, 0.78F)
            : Fade(BLACK, 0.72F);

    const Color border =
        active
            ? YELLOW
            : Fade(RAYWHITE, 0.65F);

    const Color text =
        active
            ? BLACK
            : RAYWHITE;

    DrawRectangleRec(
        rectangle,
        fill
    );

    DrawRectangleLinesEx(
        rectangle,
        active ? 3.0F : 1.5F,
        border
    );

    constexpr int font_size = 14;

    const int width =
        MeasureText(
            label,
            font_size
        );

    DrawText(
        label,
        static_cast<int>(
            rectangle.x +
            (
                rectangle.width -
                static_cast<float>(width)
            ) * 0.5F
        ),
        static_cast<int>(
            rectangle.y +
            (
                rectangle.height -
                static_cast<float>(font_size)
            ) * 0.5F
        ),
        font_size,
        text
    );
}

// ============================================================
// MAIN DRAW
// ============================================================

void CreatorTouchUI::draw(
    const CreatorController& controller,
    const int screen_width,
    const int screen_height
) const {
    if (!controller.enabled()) {
        return;
    }

    draw_toolbar(
        controller,
        screen_width,
        screen_height
    );

    draw_hotbar(
        controller,
        screen_width,
        screen_height
    );

    if (inventory_open_) {
        draw_inventory(
            controller,
            screen_width,
            screen_height
        );
    }
}

// ============================================================
// TOOLBAR
// ============================================================

void CreatorTouchUI::draw_toolbar(
    const CreatorController& controller,
    const int screen_width,
    const int screen_height
) const {
    constexpr std::array<
        CreatorTouchTool,
        6
    > tools{{
        CreatorTouchTool::Select,
        CreatorTouchTool::Move,
        CreatorTouchTool::Delete,
        CreatorTouchTool::Place,
        CreatorTouchTool::Rotate,
        CreatorTouchTool::Duplicate
    }};

    for (const auto tool : tools) {
        draw_button(
            tool_button(
                tool,
                screen_width,
                screen_height
            ),
            tool_name(tool),
            active_tool_ == tool
        );
    }

    draw_button(
        inventory_button(
            screen_width,
            screen_height
        ),
        inventory_open_
            ? "CLOSE"
            : "ASSETS",
        inventory_open_
    );

    draw_button(
        save_button(
            screen_width,
            screen_height
        ),
        "SAVE",
        false
    );

    draw_button(
        fly_button(
            screen_width,
            screen_height
        ),
        controller.state().flying
            ? "FLY ON"
            : "FLY OFF",
        controller.state().flying
    );
}

// ============================================================
// HOTBAR
// ============================================================

void CreatorTouchUI::draw_hotbar(
    const CreatorController& controller,
    const int screen_width,
    const int screen_height
) const {
    const auto& hotbar =
        controller.hotbar();

    const auto& assets =
        controller.registry().assets();

    for (
        std::size_t slot = 0;
        slot < CreatorController::CreatorController::hotbar_size;
        ++slot
    ) {
        const Rectangle rectangle =
            hotbar_slot(
                slot,
                screen_width,
                screen_height
            );

        const bool active =
            slot ==
            controller.selected_hotbar_slot();

        DrawRectangleRec(
            rectangle,
            active
                ? Fade(YELLOW, 0.82F)
                : Fade(BLACK, 0.78F)
        );

        DrawRectangleLinesEx(
            rectangle,
            active ? 3.0F : 1.0F,
            active
                ? YELLOW
                : LIGHTGRAY
        );

        char slot_text[8];

        std::snprintf(
            slot_text,
            sizeof(slot_text),
            "%zu",
            slot + 1
        );

        DrawText(
            slot_text,
            static_cast<int>(
                rectangle.x + 4.0F
            ),
            static_cast<int>(
                rectangle.y + 3.0F
            ),
            11,
            active ? BLACK : GRAY
        );

        const std::size_t asset_index =
            hotbar[slot];

        if (asset_index >= assets.size()) {
            continue;
        }

        const auto& asset =
            assets[asset_index];

        char abbreviation[3]{
            '?',
            '\0',
            '\0'
        };

        if (!asset.name.empty()) {
            abbreviation[0] =
                asset.name[0];

            if (asset.name.size() > 1) {
                abbreviation[1] =
                    asset.name[1];
            }
        }

        const int text_width =
            MeasureText(
                abbreviation,
                18
            );

        DrawText(
            abbreviation,
            static_cast<int>(
                rectangle.x +
                (
                    rectangle.width -
                    static_cast<float>(
                        text_width
                    )
                ) * 0.5F
            ),
            static_cast<int>(
                rectangle.y + 24.0F
            ),
            18,
            active ? BLACK : RAYWHITE
        );
    }
}

// ============================================================
// ASSET CARD
// ============================================================

void CreatorTouchUI::draw_asset_card(
    const Rectangle rectangle,
    const CreatorAssetDefinition& asset,
    const bool selected
) const {
    DrawRectangleRec(
        rectangle,
        selected
            ? Fade(YELLOW, 0.82F)
            : Fade(DARKGRAY, 0.95F)
    );

    DrawRectangleLinesEx(
        rectangle,
        selected ? 3.0F : 1.0F,
        selected
            ? YELLOW
            : Fade(RAYWHITE, 0.55F)
    );

    const Rectangle preview{
        rectangle.x + 7.0F,
        rectangle.y + 7.0F,
        rectangle.width - 14.0F,
        48.0F
    };

    DrawRectangleRec(
        preview,
        Fade(BLACK, 0.48F)
    );

    char icon[2]{
        asset.name.empty()
            ? '?'
            : asset.name[0],
        '\0'
    };

    const int icon_width =
        MeasureText(
            icon,
            26
        );

    DrawText(
        icon,
        static_cast<int>(
            preview.x +
            (
                preview.width -
                static_cast<float>(
                    icon_width
                )
            ) * 0.5F
        ),
        static_cast<int>(
            preview.y + 10.0F
        ),
        26,
        selected ? YELLOW : RAYWHITE
    );

    DrawText(
        asset.name.c_str(),
        static_cast<int>(
            rectangle.x + 5.0F
        ),
        static_cast<int>(
            rectangle.y + 66.0F
        ),
        11,
        selected ? BLACK : RAYWHITE
    );
}

// ============================================================
// INVENTORY DRAWER
// ============================================================

void CreatorTouchUI::draw_inventory(
    const CreatorController& controller,
    const int screen_width,
    const int screen_height
) const {
    const Rectangle panel =
        drawer_panel(
            screen_width,
            screen_height
        );

    DrawRectangleRec(
        panel,
        Fade(BLACK, 0.94F)
    );

    DrawRectangleLinesEx(
        panel,
        2.0F,
        YELLOW
    );

    DrawText(
        "OUTLAND CREATOR INVENTORY",
        24,
        52,
        18,
        YELLOW
    );

    for (
        std::size_t index = 0;
        index < categories.size();
        ++index
    ) {
        const auto category =
            categories[index];

        draw_button(
            category_button(
                index,
                screen_width,
                screen_height
            ),
            category_name(
                category
            ).data(),
            drawer_category_ ==
                category
        );
    }

    const auto category_assets =
        controller.registry().category(
            drawer_category_
        );

    const std::size_t first =
        drawer_page_ *
        assets_per_page;

    const auto* selected =
        controller.selected_asset();

    for (
        std::size_t visible = 0;
        visible < assets_per_page;
        ++visible
    ) {
        const std::size_t index =
            first + visible;

        if (
            index >=
            category_assets.size()
        ) {
            break;
        }

        const auto* asset =
            category_assets[index];

        draw_asset_card(
            asset_button(
                visible,
                screen_width,
                screen_height
            ),
            *asset,
            selected != nullptr &&
                selected->id == asset->id
        );
    }

    if (category_assets.empty()) {
        DrawText(
            "NO ASSETS REGISTERED",
            32,
            160,
            18,
            GRAY
        );
    }
}

} // namespace outland::creator

#endif // OUTLAND_DEV_TOOLS
