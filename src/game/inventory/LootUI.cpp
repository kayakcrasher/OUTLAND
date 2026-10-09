#include "outland/game/inventory/LootUI.hpp"
#include "outland/game/combat/CombatRenderer.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
namespace outland::game::inventory {
namespace {
float scale(int height) {
    return std::max(.1F, static_cast<float>(height) / 720);
}
Rectangle scaled(Rectangle r, int height) {
    const float s = scale(height);
    return {r.x * s, r.y * s, r.width * s, r.height * s};
}
bool hit(Vector2 point, Rectangle r) {
    return point.x >= r.x && point.y >= r.y && point.x < r.x + r.width && point.y < r.y + r.height;
}
} // namespace
Rectangle LootUI::row_rect(std::size_t row, int width, int height) {
    const float logical_width = static_cast<float>(width) / scale(height);
    return scaled({20, 140 + static_cast<float>(row) * 55, logical_width - 40, 49}, height);
}
Rectangle LootUI::action_rect(int action, int width, int height) {
    const float logical_width = static_cast<float>(width) / scale(height);
    const float third = (logical_width - 56) / 3;
    switch (action) {
    case 0:
        return scaled({logical_width - 116, 86, 96, 40}, height);
    case 1:
        return scaled({20, 540, third, 48}, height);
    case 2:
        return scaled({28 + third, 540, third, 48}, height);
    case 3:
        return scaled({36 + third * 2, 540, third, 48}, height);
    case 4:
        return scaled({20, 478, 100, 42}, height);
    default:
        return scaled({logical_width - 120, 478, 100, 42}, height);
    }
}
InventoryActions LootUI::update(const Inventory &bag, int width, int height, bool focused) {
    InventoryActions actions;
    const auto size = bag.state().stacks.size();
    if (size == 0) {
        selected_ = page_ = 0;
    } else {
        selected_ = std::min(selected_, size - 1);
        page_ = std::min(page_, (size - 1) / 6);
    }
    const auto activate = [&](Vector2 point) {
        for (std::size_t row = 0; row < 6; ++row)
            if (page_ * 6 + row < size && hit(point, row_rect(row, width, height)))
                selected_ = page_ * 6 + row;
        for (int button = 0; button < 6; ++button)
            if (hit(point, action_rect(button, width, height))) {
                if (button == 0)
                    actions.close = true;
                else if (button == 1)
                    actions.use = true;
                else if (button == 2)
                    actions.drop_one = true;
                else if (button == 3)
                    actions.drop_stack = true;
                else if (button == 4) {
                    if (page_ > 0)
                        --page_;
                    selected_ = page_ * 6;
                } else if (page_ < (size == 0 ? 0 : (size - 1) / 6)) {
                    ++page_;
                    selected_ = page_ * 6;
                }
            }
    };
    std::unordered_set<int> current;
    const int touches = std::max(0, GetTouchPointCount());
    for (int i = 0; i < touches; ++i) {
        const int id = GetTouchPointId(i);
        if (id < 0 || !current.insert(id).second)
            continue;
        if (focused && !warm_ && !previous_.contains(id))
            activate(GetTouchPosition(i));
    }
    if (focused && !warm_ && touches == 0 && !had_touch_ && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        activate(GetMousePosition());
    if (focused && !warm_) {
        if (IsKeyPressed(KEY_DOWN) && size > 0) {
            selected_ = std::min(selected_ + 1, size - 1);
            page_ = selected_ / 6;
        }
        if (IsKeyPressed(KEY_UP) && selected_ > 0) {
            --selected_;
            page_ = selected_ / 6;
        }
        actions.use |= IsKeyPressed(KEY_ENTER);
        if (IsKeyPressed(KEY_DELETE)) {
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                actions.drop_stack = true;
            else
                actions.drop_one = true;
        }
    }
    previous_ = std::move(current);
    had_touch_ = touches > 0;
    warm_ = false;
    actions.selected = selected_;
    return actions;
}
void LootUI::draw_inventory(const ItemRegistry &registry, const LootSession &session,
                            const combat::WeaponSystem &weapons, int width, int height) const {
    const auto &bag = session.inventory();
    const float s = scale(height), logical_width = static_cast<float>(width) / s;
    rlPushMatrix();
    rlScalef(s, s, 1);
    DrawRectangle(0, 80, static_cast<int>(logical_width), 640, Fade(BLACK, .96F));
    DrawText(TextFormat("BAG %d / %d", static_cast<int>(bag.state().stacks.size()),
                        bag.capacity(registry)),
             20, 94, 23, RAYWHITE);
    DrawText(TextFormat("%s  %d loaded / %d loose",
                        weapons.available() ? weapons.weapon().name : "NO WEAPON",
                        weapons.ammo().loaded, weapons.ammo().reserve),
             20, 120, 15, GRAY);
    for (std::size_t row = 0; row < 6; ++row) {
        const auto index = page_ * 6 + row;
        const auto rect = row_rect(row, static_cast<int>(logical_width), 720);
        DrawRectangleRec(rect, Fade(index == selected_ ? DARKGREEN : DARKGRAY, .9F));
        if (index < bag.state().stacks.size()) {
            const auto &stack = bag.state().stacks[index];
            const auto *item = registry.find(stack.item);
            DrawText(TextFormat("%s  x%d", item ? item->name.c_str() : stack.item.c_str(),
                                stack.quantity),
                     30, 148 + static_cast<int>(row) * 55, 19, RAYWHITE);
            if (item)
                DrawText(category_name(item->category), 30, 172 + static_cast<int>(row) * 55, 13,
                         GRAY);
        }
    }
    const char *labels[]{"CLOSE", "USE / EQUIP", "DROP 1", "DROP STACK", "PREV", "NEXT"};
    for (int action = 0; action < 6; ++action) {
        const auto rect = action_rect(action, static_cast<int>(logical_width), 720);
        DrawRectangleRec(rect, DARKGRAY);
        DrawText(labels[action], static_cast<int>(rect.x) + 9, static_cast<int>(rect.y) + 14, 16,
                 RAYWHITE);
    }
    DrawText(TextFormat("Page %d / %d", static_cast<int>(page_) + 1,
                        std::max(1, static_cast<int>((bag.state().stacks.size() + 5) / 6))),
             140, 490, 18, GRAY);
    DrawText("I / CLOSE: close   arrows: select   Enter: use   Delete: drop", 20, 608, 14, GRAY);
    DrawText(session.status().c_str(), 20, 640, 15, GRAY);
    rlPopMatrix();
}
void LootUI::draw_world(const ItemRegistry &registry, const LootWorld &world, Vector3 player,
                        bool light) {
    for (const auto &pickup : world.state().pickups) {
        if (Vector3DistanceSqr(player, pickup.position) > 60 * 60)
            continue;
        const auto *item = registry.find(pickup.item);
        if (!item)
            continue;
        const Color tint = light && Vector3DistanceSqr(player, pickup.position) < 8 * 8
                               ? Color{255, 244, 200, 255}
                               : WHITE;
        if (item->model.empty()) {
            combat::CombatRenderer::draw_gun(Vector3Add(pickup.position,{0,item->height*.8F,0}), {0, 0, 1}, *item->weapon, 0, false,
                                             false);
            continue;
        }
        auto *model = models_.load(
            (std::filesystem::path(asset_root_) / item->model).string(), [](Model &value) {
                value.transform = MatrixIdentity();
                const auto bounds = assets::transformed_model_bounds(value);
                const auto height = bounds.max.y - bounds.min.y;
                if (!std::isfinite(height) || height < .000001F)
                    return false;
                value.transform = MatrixScale(1 / height, 1 / height, 1 / height);
                return true;
            });
        if (model)
            DrawModelEx(*model, pickup.position, {0, 1, 0}, 0,
                        {item->height, item->height, item->height}, tint);
    }
    if (light)
        DrawCircle3D({player.x, player.y + .02F, player.z}, 3, {1, 0, 0}, 90, Fade(YELLOW, .35F));
}
} // namespace outland::game::inventory
