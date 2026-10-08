#include "outland/creator/CreatorTouchUI.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#ifdef OUTLAND_DEV_TOOLS

#include <array>
#include <cstdio>

namespace outland::creator {

namespace {

constexpr std::size_t category_count = 7;
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


bool CreatorTouchUI::pointer_over_ui(
    const Vector2 position,
    const int screen_width,
    const int screen_height
) const {

    // --------------------------------------------------------
    // GAMEPLAY TOUCH HIT PROTECTION
    // --------------------------------------------------------

    if (
        touch_settings_.gameplay_controls_visible
    ) {
        const float radius =
            joystick_radius(
                screen_width,
                screen_height
            );

        if (
            CheckCollisionPointCircle(
                position,
                move_stick_center(
                    screen_width,
                    screen_height
                ),
                radius
            ) ||
            CheckCollisionPointCircle(
                position,
                look_stick_center(
                    screen_width,
                    screen_height
                ),
                radius
            )
        ) {
            return true;
        }

        constexpr GameplayTouchButton buttons[]{
            GameplayTouchButton::Jump,
            GameplayTouchButton::Reload,
            GameplayTouchButton::Sprint,
            GameplayTouchButton::Crouch,
            GameplayTouchButton::Inventory,
            GameplayTouchButton::Aim,
            GameplayTouchButton::Fire,
            GameplayTouchButton::Interact
        };

        for (
            const GameplayTouchButton button :
            buttons
        ) {
            if (
                CheckCollisionPointRec(
                    position,
                    gameplay_button(
                        button,
                        screen_width,
                        screen_height
                    )
                )
            ) {
                return true;
            }
        }
    }

    if (
        CheckCollisionPointRec(
            position,
            inventory_button(
                screen_width,
                screen_height
            )
        ) ||
        CheckCollisionPointRec(
            position,
            save_button(
                screen_width,
                screen_height
            )
        ) ||
        CheckCollisionPointRec(
            position,
            fly_button(
                screen_width,
                screen_height
            )
        )
    ) {
        return true;
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

    for (const CreatorTouchTool tool : tools) {
        if (
            CheckCollisionPointRec(
                position,
                tool_button(
                    tool,
                    screen_width,
                    screen_height
                )
            )
        ) {
            return true;
        }
    }

    if (!inventory_open_) {
        for (
            std::size_t slot = 0;
            slot <
                CreatorController::hotbar_size;
            ++slot
        ) {
            if (
                CheckCollisionPointRec(
                    position,
                    hotbar_slot(
                        slot,
                        screen_width,
                        screen_height
                    )
                )
            ) {
                return true;
            }
        }

        return false;
    }

    // While the asset drawer is open, the whole drawer owns
    // its screen area. World selection behind it is disabled.
    return CheckCollisionPointRec(
        position,
        drawer_panel(
            screen_width,
            screen_height
        )
    );
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

    touch_settings_.clamp();

    update_gameplay_controls(
        screen_width,
        screen_height
    );

    /*
     * Gameplay controls are independent from Creator mode.
     *
     * Creator may be disabled while normal OUTLAND movement,
     * look and combat touch controls remain active.
     */
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
                // Emit intent only.
                // Renderer decides whether this rotates the selected
                // object or the current placement preview.
                actions_.rotate = true;
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
    const int screen_height
) const {
    const Rectangle panel =
        drawer_panel(
            screen_width,
            screen_height
        );

    constexpr float gap = 8.0F;
    constexpr float side_padding = 14.0F;
    // Search + Favorites/Recent controls own the top strip.
    constexpr float top_offset = 142.0F;

    // Bottom strip belongs to PREV / PAGE / NEXT.
    constexpr float bottom_padding = 62.0F;

    const std::size_t column =
        visible_index % library_columns;

    const std::size_t row =
        visible_index / library_columns;

    const float available_width =
        panel.width -
        side_padding * 2.0F -
        gap * static_cast<float>(
            library_columns - 1
        );

    const float available_height =
        panel.height -
        top_offset -
        bottom_padding -
        gap * static_cast<float>(
            library_rows - 1
        );

    const float card_width =
        available_width /
        static_cast<float>(library_columns);

    const float card_height =
        available_height /
        static_cast<float>(library_rows);

    return {
        panel.x +
            side_padding +
            static_cast<float>(column) *
                (card_width + gap),

        panel.y +
            top_offset +
            static_cast<float>(row) *
                (card_height + gap),

        card_width,
        card_height
    };
}

Rectangle CreatorTouchUI::previous_page_button(
    const int screen_width,
    const int screen_height
) const {
    const Rectangle panel =
        drawer_panel(
            screen_width,
            screen_height
        );

    return {
        panel.x + 14.0F,
        panel.y + panel.height - 46.0F,
        112.0F,
        34.0F
    };
}

Rectangle CreatorTouchUI::next_page_button(
    const int screen_width,
    const int screen_height
) const {
    const Rectangle panel =
        drawer_panel(
            screen_width,
            screen_height
        );

    return {
        panel.x + panel.width - 126.0F,
        panel.y + panel.height - 46.0F,
        112.0F,
        34.0F
    };
}

Rectangle CreatorTouchUI::search_box(
    const int screen_width,
    const int screen_height
) const {
    const Rectangle panel =
        drawer_panel(
            screen_width,
            screen_height
        );

    return {
        panel.x + 14.0F,
        panel.y + 58.0F,
        panel.width - 28.0F,
        34.0F
    };
}

Rectangle CreatorTouchUI::favorites_filter_button(
    const int screen_width,
    const int screen_height
) const {
    const Rectangle panel =
        drawer_panel(
            screen_width,
            screen_height
        );

    const float width =
        (panel.width - 36.0F) * 0.5F;

    return {
        panel.x + 14.0F,
        panel.y + 98.0F,
        width,
        32.0F
    };
}

Rectangle CreatorTouchUI::recent_filter_button(
    const int screen_width,
    const int screen_height
) const {
    const Rectangle panel =
        drawer_panel(
            screen_width,
            screen_height
        );

    const float width =
        (panel.width - 36.0F) * 0.5F;

    return {
        panel.x + 22.0F + width,
        panel.y + 98.0F,
        width,
        32.0F
    };
}

bool CreatorTouchUI::is_favorite(
    const std::string& asset_id
) const {
    return std::find(
        favorite_asset_ids_.begin(),
        favorite_asset_ids_.end(),
        asset_id
    ) != favorite_asset_ids_.end();
}

void CreatorTouchUI::toggle_favorite(
    const std::string& asset_id
) {
    const auto found = std::find(
        favorite_asset_ids_.begin(),
        favorite_asset_ids_.end(),
        asset_id
    );

    if (
        found !=
        favorite_asset_ids_.end()
    ) {
        favorite_asset_ids_.erase(found);
    } else {
        favorite_asset_ids_.push_back(
            asset_id
        );
    }

    drawer_page_ = 0;
}

void CreatorTouchUI::remember_recent(
    const std::string& asset_id
) {
    recent_asset_ids_.erase(
        std::remove(
            recent_asset_ids_.begin(),
            recent_asset_ids_.end(),
            asset_id
        ),
        recent_asset_ids_.end()
    );

    recent_asset_ids_.insert(
        recent_asset_ids_.begin(),
        asset_id
    );

    if (
        recent_asset_ids_.size() >
        max_recent_assets
    ) {
        recent_asset_ids_.resize(
            max_recent_assets
        );
    }
}

std::vector<const CreatorAssetDefinition*>
CreatorTouchUI::filtered_assets(
    const CreatorController& controller
) const {
    const auto category_assets =
        controller.registry().category(
            drawer_category_
        );

    auto lower = [](std::string value) {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](const unsigned char ch) {
                return static_cast<char>(
                    std::tolower(ch)
                );
            }
        );

        return value;
    };

    const std::string query =
        lower(asset_search_);

    std::vector<
        const CreatorAssetDefinition*
    > result;

    result.reserve(category_assets.size());

    for (const auto* asset : category_assets) {
        if (asset == nullptr) {
            continue;
        }

        if (
            favorites_only_ &&
            !is_favorite(asset->id)
        ) {
            continue;
        }

        if (recent_only_) {
            const auto recent =
                std::find(
                    recent_asset_ids_.begin(),
                    recent_asset_ids_.end(),
                    asset->id
                );

            if (
                recent ==
                recent_asset_ids_.end()
            ) {
                continue;
            }
        }

        bool matches = query.empty();

        if (!matches) {
            matches =
                lower(asset->name).find(query) !=
                    std::string::npos ||
                lower(asset->id).find(query) !=
                    std::string::npos;
        }

        if (!matches) {
            for (const auto& tag : asset->tags) {
                if (
                    lower(tag).find(query) !=
                    std::string::npos
                ) {
                    matches = true;
                    break;
                }
            }
        }

        if (matches) {
            result.push_back(asset);
        }
    }

    if (recent_only_) {
        std::stable_sort(
            result.begin(),
            result.end(),
            [this](
                const CreatorAssetDefinition* left,
                const CreatorAssetDefinition* right
            ) {
                const auto left_position =
                    std::find(
                        recent_asset_ids_.begin(),
                        recent_asset_ids_.end(),
                        left->id
                    );

                const auto right_position =
                    std::find(
                        recent_asset_ids_.begin(),
                        recent_asset_ids_.end(),
                        right->id
                    );

                return left_position <
                    right_position;
            }
        );
    }

    return result;
}

void CreatorTouchUI::update_search_input() {
    if (!search_focused_) {
        return;
    }

    bool changed = false;

    int codepoint = GetCharPressed();

    while (codepoint > 0) {
        if (
            codepoint >= 32 &&
            codepoint <= 126 &&
            asset_search_.size() < 64
        ) {
            asset_search_.push_back(
                static_cast<char>(codepoint)
            );
            changed = true;
        }

        codepoint = GetCharPressed();
    }

    if (
        IsKeyPressed(KEY_BACKSPACE) &&
        !asset_search_.empty()
    ) {
        asset_search_.pop_back();
        changed = true;
    }

    if (
        IsKeyPressed(KEY_ESCAPE) ||
        IsKeyPressed(KEY_ENTER)
    ) {
        search_focused_ = false;
    }

    if (changed) {
        drawer_page_ = 0;
    }
}

// ============================================================
// INVENTORY INPUT
// ============================================================

void CreatorTouchUI::update_inventory(
    CreatorController& controller,
    const int screen_width,
    const int screen_height
) {
    if (
        pressed(
            search_box(
                screen_width,
                screen_height
            )
        )
    ) {
        search_focused_ = true;
        return;
    }

    update_search_input();

    if (
        pressed(
            favorites_filter_button(
                screen_width,
                screen_height
            )
        )
    ) {
        favorites_only_ =
            !favorites_only_;

        if (favorites_only_) {
            recent_only_ = false;
        }

        drawer_page_ = 0;
        return;
    }

    if (
        pressed(
            recent_filter_button(
                screen_width,
                screen_height
            )
        )
    ) {
        recent_only_ =
            !recent_only_;

        if (recent_only_) {
            favorites_only_ = false;
        }

        drawer_page_ = 0;
        return;
    }

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
        filtered_assets(controller);

    const std::size_t page_count =
        category_assets.empty()
            ? 1
            : (
                category_assets.size() +
                assets_per_page - 1
            ) / assets_per_page;

    if (
        drawer_page_ > 0 &&
        pressed(
            previous_page_button(
                screen_width,
                screen_height
            )
        )
    ) {
        --drawer_page_;
        return;
    }

    if (
        drawer_page_ + 1 < page_count &&
        pressed(
            next_page_button(
                screen_width,
                screen_height
            )
        )
    ) {
        ++drawer_page_;
        return;
    }

    if (drawer_page_ >= page_count) {
        drawer_page_ = page_count - 1;
    }

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

        const Rectangle card =
            asset_button(
                visible,
                screen_width,
                screen_height
            );

        const Rectangle star_button{
            card.x + card.width - 34.0F,
            card.y + 4.0F,
            30.0F,
            30.0F
        };

        if (pressed(star_button)) {
            const auto* favorite =
                category_assets[
                    category_index
                ];

            if (favorite != nullptr) {
                toggle_favorite(
                    favorite->id
                );
            }

            return;
        }

        if (!pressed(card)) {
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

            remember_recent(
                chosen->id
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

    draw_gameplay_controls(
        screen_width,
        screen_height
    );

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

    const Rectangle search_rectangle =
        search_box(
            screen_width,
            screen_height
        );

    DrawRectangleRec(
        search_rectangle,
        Fade(DARKGRAY, 0.96F)
    );

    DrawRectangleLinesEx(
        search_rectangle,
        search_focused_ ? 2.0F : 1.0F,
        search_focused_ ? YELLOW : GRAY
    );

    std::string search_text =
        asset_search_.empty()
            ? "SEARCH ASSETS..."
            : "SEARCH: " + asset_search_;

    if (search_focused_) {
        search_text += "_";
    }

    DrawText(
        search_text.c_str(),
        static_cast<int>(
            search_rectangle.x + 10.0F
        ),
        static_cast<int>(
            search_rectangle.y + 8.0F
        ),
        16,
        asset_search_.empty()
            ? GRAY
            : RAYWHITE
    );

    draw_button(
        favorites_filter_button(
            screen_width,
            screen_height
        ),
        "★ FAVORITES",
        favorites_only_
    );

    draw_button(
        recent_filter_button(
            screen_width,
            screen_height
        ),
        "RECENT",
        recent_only_
    );

    const auto category_assets =
        filtered_assets(controller);

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

        const Rectangle card =
            asset_button(
                visible,
                screen_width,
                screen_height
            );

        draw_asset_card(
            card,
            *asset,
            selected != nullptr &&
                selected->id == asset->id
        );

        const Rectangle star_button{
            card.x + card.width - 34.0F,
            card.y + 4.0F,
            30.0F,
            30.0F
        };

        DrawRectangleRec(
            star_button,
            Fade(BLACK, 0.72F)
        );

        DrawRectangleLinesEx(
            star_button,
            1.0F,
            is_favorite(asset->id)
                ? YELLOW
                : GRAY
        );

        DrawText(
            is_favorite(asset->id)
                ? "*"
                : "+",
            static_cast<int>(
                star_button.x + 10.0F
            ),
            static_cast<int>(
                star_button.y + 5.0F
            ),
            20,
            is_favorite(asset->id)
                ? YELLOW
                : LIGHTGRAY
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

    const std::size_t page_count =
        category_assets.empty()
            ? 1
            : (
                category_assets.size() +
                assets_per_page - 1
            ) / assets_per_page;

    draw_button(
        previous_page_button(
            screen_width,
            screen_height
        ),
        "< PREV",
        drawer_page_ > 0
    );

    draw_button(
        next_page_button(
            screen_width,
            screen_height
        ),
        "NEXT >",
        drawer_page_ + 1 < page_count
    );

    const std::string page_text =
        "PAGE " +
        std::to_string(drawer_page_ + 1) +
        " / " +
        std::to_string(page_count);

    constexpr int page_font_size = 16;

    const int page_text_width =
        MeasureText(
            page_text.c_str(),
            page_font_size
        );

    DrawText(
        page_text.c_str(),
        static_cast<int>(
            panel.x +
            (
                panel.width -
                static_cast<float>(page_text_width)
            ) * 0.5F
        ),
        static_cast<int>(
            panel.y +
            panel.height -
            37.0F
        ),
        page_font_size,
        RAYWHITE
    );
}


// ============================================================
// GAMEPLAY TOUCH IMPLEMENTATION 32B
// ============================================================

Vector2 CreatorTouchUI::move_stick_center(
    const int screen_width,
    const int screen_height
) const {
    const float radius =
        joystick_radius(
            screen_width,
            screen_height
        );

    return {
        radius + 28.0F,
        static_cast<float>(screen_height) -
            radius -
            32.0F
    };
}

Vector2 CreatorTouchUI::look_stick_center(
    const int screen_width,
    const int screen_height
) const {
    const float radius =
        joystick_radius(
            screen_width,
            screen_height
        );

    /*
     * Keep LOOK inward from the right edge.
     * The action cluster owns the far-right thumb zone.
     */
    return {
        static_cast<float>(screen_width) -
            radius * 2.65F,
        static_cast<float>(screen_height) -
            radius -
            32.0F
    };
}

float CreatorTouchUI::joystick_radius(
    const int screen_width,
    const int screen_height
) const {
    const float shortest =
        static_cast<float>(
            std::min(
                screen_width,
                screen_height
            )
        );

    return std::clamp(
        shortest *
            0.090F *
            touch_settings_.joystick_scale,
        54.0F,
        118.0F
    );
}

Rectangle CreatorTouchUI::gameplay_button(
    const GameplayTouchButton button,
    const int screen_width,
    const int screen_height
) const {
    const float shortest =
        static_cast<float>(
            std::min(
                screen_width,
                screen_height
            )
        );

    const float unit =
        std::clamp(
            shortest *
                0.058F *
                touch_settings_.button_scale,
            46.0F,
            82.0F
        );

    const float right =
        static_cast<float>(screen_width) -
        16.0F;

    const float bottom =
        static_cast<float>(screen_height) -
        20.0F;

    const float gap =
        unit * 0.16F;

    switch (button) {
        case GameplayTouchButton::Fire: {
            const float size =
                unit * 1.28F;

            return {
                right - size,
                bottom - size,
                size,
                size
            };
        }

        case GameplayTouchButton::Aim:
            return {
                right - unit * 2.18F - gap,
                bottom - unit * 1.48F,
                unit,
                unit
            };

        case GameplayTouchButton::Jump:
            return {
                right - unit,
                bottom - unit * 2.30F - gap,
                unit,
                unit
            };

        case GameplayTouchButton::Reload:
            return {
                right - unit * 2.12F - gap,
                bottom - unit * 0.42F,
                unit,
                unit
            };

        case GameplayTouchButton::Interact:
            return {
                right - unit,
                bottom - unit * 3.45F - gap,
                unit,
                unit
            };

        case GameplayTouchButton::Crouch:
            return {
                right - unit * 2.12F - gap,
                bottom - unit * 2.58F,
                unit,
                unit
            };

        case GameplayTouchButton::Sprint:
            return {
                22.0F,
                bottom -
                    joystick_radius(
                        screen_width,
                        screen_height
                    ) * 2.28F -
                    unit,
                unit * 1.22F,
                unit * 0.82F
            };

        case GameplayTouchButton::Inventory:
            return {
                right - unit * 1.46F,
                18.0F,
                unit * 1.46F,
                unit * 0.78F
            };
    }

    return {};
}

void CreatorTouchUI::update_joystick(
    TouchJoystickState& stick,
    const Vector2 center,
    const float radius,
    const int finger_id,
    const Vector2 touch_position
) {
    stick.active = true;
    stick.finger_id = finger_id;
    stick.origin = center;

    Vector2 delta{
        touch_position.x - center.x,
        touch_position.y - center.y
    };

    const float length =
        std::sqrt(
            delta.x * delta.x +
            delta.y * delta.y
        );

    if (
        length > radius &&
        length > 0.0001F
    ) {
        const float scale =
            radius / length;

        delta.x *= scale;
        delta.y *= scale;
    }

    stick.knob = {
        center.x + delta.x,
        center.y + delta.y
    };

    stick.value = {
        std::clamp(
            delta.x / radius,
            -1.0F,
            1.0F
        ),
        std::clamp(
            delta.y / radius,
            -1.0F,
            1.0F
        )
    };
}

void CreatorTouchUI::update_gameplay_controls(
    const int screen_width,
    const int screen_height
) {
    if (
        !touch_settings_.gameplay_controls_visible
    ) {
        clear_gameplay_state();
        return;
    }

    const Vector2 move_center =
        move_stick_center(
            screen_width,
            screen_height
        );

    const Vector2 look_center =
        look_stick_center(
            screen_width,
            screen_height
        );

    const float radius =
        joystick_radius(
            screen_width,
            screen_height
        );

    /*
     * Frame values are rebuilt from current pointers.
     * Finger IDs remain on the joystick structs so a finger
     * keeps ownership while it stays on-screen.
     */
    gameplay_state_.clear();

    bool move_found = false;
    bool look_found = false;

    const int touch_count =
        GetTouchPointCount();

    // --------------------------------------------------------
    // KEEP EXISTING FINGER OWNERSHIP
    // --------------------------------------------------------

    for (
        int index = 0;
        index < touch_count;
        ++index
    ) {
        const int id =
            GetTouchPointId(index);

        const Vector2 position =
            GetTouchPosition(index);

        if (
            move_stick_.finger_id >= 0 &&
            id == move_stick_.finger_id
        ) {
            update_joystick(
                move_stick_,
                move_center,
                radius,
                id,
                position
            );

            move_found = true;
        }

        if (
            look_stick_.finger_id >= 0 &&
            id == look_stick_.finger_id
        ) {
            update_joystick(
                look_stick_,
                look_center,
                radius,
                id,
                position
            );

            look_found = true;
        }
    }

    if (!move_found) {
        move_stick_.clear();
        move_stick_.origin = move_center;
        move_stick_.knob = move_center;
    }

    if (!look_found) {
        look_stick_.clear();
        look_stick_.origin = look_center;
        look_stick_.knob = look_center;
    }

    // --------------------------------------------------------
    // CLAIM NEW TOUCHES FOR STICKS
    // --------------------------------------------------------

    for (
        int index = 0;
        index < touch_count;
        ++index
    ) {
        const int id =
            GetTouchPointId(index);

        const Vector2 position =
            GetTouchPosition(index);

        if (
            move_stick_.active &&
            id == move_stick_.finger_id
        ) {
            continue;
        }

        if (
            look_stick_.active &&
            id == look_stick_.finger_id
        ) {
            continue;
        }

        if (
            !move_stick_.active &&
            CheckCollisionPointCircle(
                position,
                move_center,
                radius * 1.28F
            )
        ) {
            update_joystick(
                move_stick_,
                move_center,
                radius,
                id,
                position
            );

            continue;
        }

        if (
            !look_stick_.active &&
            CheckCollisionPointCircle(
                position,
                look_center,
                radius * 1.28F
            )
        ) {
            update_joystick(
                look_stick_,
                look_center,
                radius,
                id,
                position
            );
        }
    }

    // --------------------------------------------------------
    // X11 / DESKTOP MOUSE FALLBACK
    //
    // Mouse can operate one virtual stick at a time.
    // Native Android touch will provide true multitouch.
    // --------------------------------------------------------

    if (
        touch_count == 0 &&
        IsMouseButtonDown(
            MOUSE_BUTTON_LEFT
        )
    ) {
        const Vector2 mouse =
            GetMousePosition();

        if (
            CheckCollisionPointCircle(
                mouse,
                move_center,
                radius * 1.28F
            )
        ) {
            update_joystick(
                move_stick_,
                move_center,
                radius,
                -2,
                mouse
            );
        } else if (
            CheckCollisionPointCircle(
                mouse,
                look_center,
                radius * 1.28F
            )
        ) {
            update_joystick(
                look_stick_,
                look_center,
                radius,
                -2,
                mouse
            );
        }
    }

    gameplay_state_.move_x =
        move_stick_.value.x;

    gameplay_state_.move_y =
        move_stick_.value.y;

    gameplay_state_.look_x =
        look_stick_.value.x *
        touch_settings_.look_sensitivity;

    gameplay_state_.look_y =
        look_stick_.value.y *
        touch_settings_.look_sensitivity;

    // --------------------------------------------------------
    // BUTTON STATE
    // --------------------------------------------------------

    const auto button_down =
        [&](const GameplayTouchButton button) {
            const Rectangle bounds =
                gameplay_button(
                    button,
                    screen_width,
                    screen_height
                );

            for (
                int index = 0;
                index < touch_count;
                ++index
            ) {
                if (
                    CheckCollisionPointRec(
                        GetTouchPosition(index),
                        bounds
                    )
                ) {
                    return true;
                }
            }

            if (
                touch_count == 0 &&
                IsMouseButtonDown(
                    MOUSE_BUTTON_LEFT
                ) &&
                CheckCollisionPointRec(
                    GetMousePosition(),
                    bounds
                )
            ) {
                return true;
            }

            return false;
        };

    gameplay_state_.jump =
        button_down(
            GameplayTouchButton::Jump
        );

    gameplay_state_.reload =
        button_down(
            GameplayTouchButton::Reload
        );

    gameplay_state_.sprint =
        button_down(
            GameplayTouchButton::Sprint
        );

    gameplay_state_.crouch =
        button_down(
            GameplayTouchButton::Crouch
        );

    gameplay_state_.inventory =
        button_down(
            GameplayTouchButton::Inventory
        );

    gameplay_state_.aim =
        button_down(
            GameplayTouchButton::Aim
        );

    gameplay_state_.fire =
        button_down(
            GameplayTouchButton::Fire
        );

    gameplay_state_.interact =
        button_down(
            GameplayTouchButton::Interact
        );
}

void CreatorTouchUI::draw_joystick(
    const TouchJoystickState& stick,
    const Vector2 center,
    const float radius,
    const char* label
) const {
    const float opacity =
        touch_settings_.opacity;

    DrawCircleV(
        center,
        radius,
        Fade(
            BLACK,
            opacity * 0.46F
        )
    );

    DrawCircleLines(
        static_cast<int>(center.x),
        static_cast<int>(center.y),
        radius,
        Fade(
            RAYWHITE,
            opacity * 0.82F
        )
    );

    const Vector2 knob =
        stick.active
            ? stick.knob
            : center;

    DrawCircleV(
        knob,
        radius * 0.38F,
        Fade(
            RAYWHITE,
            opacity * 0.30F
        )
    );

    DrawCircleLines(
        static_cast<int>(knob.x),
        static_cast<int>(knob.y),
        radius * 0.38F,
        Fade(
            RAYWHITE,
            opacity
        )
    );

    if (
        label != nullptr &&
        label[0] != '\0'
    ) {
        const int font_size =
            static_cast<int>(
                std::clamp(
                    radius * 0.23F,
                    12.0F,
                    20.0F
                )
            );

        const int width =
            MeasureText(
                label,
                font_size
            );

        DrawText(
            label,
            static_cast<int>(
                center.x -
                static_cast<float>(width) * 0.5F
            ),
            static_cast<int>(
                center.y +
                radius +
                5.0F
            ),
            font_size,
            Fade(
                RAYWHITE,
                opacity
            )
        );
    }
}

void CreatorTouchUI::draw_gameplay_button(
    const GameplayTouchButton,
    const Rectangle rectangle,
    const char* label,
    const bool active
) const {
    const float opacity =
        touch_settings_.opacity;

    const Color fill =
        active
            ? Fade(
                RAYWHITE,
                opacity * 0.34F
            )
            : Fade(
                BLACK,
                opacity * 0.50F
            );

    const Color outline =
        active
            ? Fade(
                YELLOW,
                opacity
            )
            : Fade(
                RAYWHITE,
                opacity * 0.80F
            );

    DrawRectangleRec(
        rectangle,
        fill
    );

    DrawRectangleLinesEx(
        rectangle,
        active ? 3.0F : 2.0F,
        outline
    );

    const int font_size =
        static_cast<int>(
            std::clamp(
                rectangle.height * 0.25F,
                12.0F,
                20.0F
            )
        );

    const int text_width =
        MeasureText(
            label,
            font_size
        );

    DrawText(
        label,
        static_cast<int>(
            rectangle.x +
            rectangle.width * 0.5F -
            static_cast<float>(text_width) *
                0.5F
        ),
        static_cast<int>(
            rectangle.y +
            rectangle.height * 0.5F -
            static_cast<float>(font_size) *
                0.5F
        ),
        font_size,
        outline
    );
}

void CreatorTouchUI::draw_gameplay_controls(
    const int screen_width,
    const int screen_height
) const {
    if (
        !touch_settings_.gameplay_controls_visible
    ) {
        return;
    }

    const float radius =
        joystick_radius(
            screen_width,
            screen_height
        );

    draw_joystick(
        move_stick_,
        move_stick_center(
            screen_width,
            screen_height
        ),
        radius,
        "MOVE"
    );

    /*
     * Deliberately unlabeled to match the reference:
     * right thumb area should read visually as camera control,
     * not another UI button.
     */
    draw_joystick(
        look_stick_,
        look_stick_center(
            screen_width,
            screen_height
        ),
        radius,
        ""
    );

    const auto draw_action =
        [&](const GameplayTouchButton button,
            const char* label,
            const bool active) {
            draw_gameplay_button(
                button,
                gameplay_button(
                    button,
                    screen_width,
                    screen_height
                ),
                label,
                active
            );
        };

    draw_action(
        GameplayTouchButton::Sprint,
        "SPRINT",
        gameplay_state_.sprint
    );

    draw_action(
        GameplayTouchButton::Inventory,
        "INV",
        gameplay_state_.inventory
    );

    draw_action(
        GameplayTouchButton::Crouch,
        "CROUCH",
        gameplay_state_.crouch
    );

    draw_action(
        GameplayTouchButton::Reload,
        "RELOAD",
        gameplay_state_.reload
    );

    draw_action(
        GameplayTouchButton::Aim,
        "AIM",
        gameplay_state_.aim
    );

    draw_action(
        GameplayTouchButton::Jump,
        "JUMP",
        gameplay_state_.jump
    );

    draw_action(
        GameplayTouchButton::Interact,
        "USE",
        gameplay_state_.interact
    );

    draw_action(
        GameplayTouchButton::Fire,
        "FIRE",
        gameplay_state_.fire
    );
}

} // namespace outland::creator

#endif // OUTLAND_DEV_TOOLS
