#include "outland/game/inventory/LootUI.hpp"
#include "outland/game/combat/CombatRenderer.hpp"
#include <raymath.h>
#include "outland/input/PointerEvents.hpp"
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
    const auto buffered=input::PointerEvents::presses();
    if(focused && !warm_) for(const auto& point:buffered) activate(point);
    std::unordered_set<int> current;
    const int touches = std::max(0, GetTouchPointCount());
    for (int i = 0; i < touches; ++i) {
        const int id = GetTouchPointId(i);
        if (id < 0 || !current.insert(id).second)
            continue;
        if (focused && !warm_ && buffered.empty() && !previous_.contains(id))
            activate(GetTouchPosition(i));
    }
    if (focused && !warm_ && buffered.empty() && touches == 0 && !had_touch_ && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
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
Color LootUI::rarity_color(Rarity rarity) {
    switch (rarity) {
        case Rarity::Epic: return {196, 92, 255, 255};
        case Rarity::Rare: return {70, 150, 255, 255};
        case Rarity::Uncommon: return {90, 220, 110, 255};
        default: return {235, 235, 225, 255};
    }
}

void LootUI::draw_world(const ItemRegistry &registry, const LootWorld &world, Vector3 player,
                        bool light) {
    const float time = static_cast<float>(GetTime());
    // Glow and beams first, without writing depth, so the models stand in them.
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    for (const auto &pickup : world.state().pickups) {
        const float d2 = Vector3DistanceSqr(player, pickup.position);
        const auto rarity = rarity_of(pickup.item);
        // The best finds show from further off: a beam of their colour over them.
        if (d2 > (rarity >= Rarity::Rare ? 140.0F * 140.0F : 60.0F * 60.0F) || rarity == Rarity::Common)
            continue;
        const Color c = rarity_color(rarity);
        const float pulse = .75F + .25F * std::sin(time * 3 + pickup.position.x);
        DrawCylinder({pickup.position.x, pickup.position.y + .01F, pickup.position.z}, .42F, .42F, .02F, 16,
                     Fade(c, .28F * pulse));
        DrawCircle3D({pickup.position.x, pickup.position.y + .03F, pickup.position.z}, .45F, {1, 0, 0}, 90, Fade(c, .9F));
        if (rarity >= Rarity::Rare) {
            const float tall = rarity == Rarity::Epic ? 5.0F : 2.6F;
            DrawCylinder(pickup.position, .05F, .02F, tall, 6, Fade(c, .55F * pulse));
            DrawCylinder(pickup.position, .16F, .02F, tall * .7F, 8, Fade(c, .16F * pulse));
        }
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
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
            // Lying on its side on the ground.
            combat::CombatRenderer::draw_gun(Vector3Add(pickup.position, {0, .025F, 0}), {1, 0, 0}, {0, 0, 1},
                                             *item->weapon, 0, 0, false);
            continue;
        }
        auto *model = models_.load(
            (std::filesystem::path(asset_root_) / item->model).string(), [](Model &value) {
                value.transform = MatrixIdentity();
                const auto bounds = assets::transformed_model_bounds(value);
                const float unit = pickup_unit_scale(bounds);
                if(unit==0) return false;
                value.transform = MatrixScale(unit,unit,unit);
                return true;
            });
        // Anything better than common turns slowly and bobs, catching the eye.
        const bool show = rarity_of(pickup.item) != Rarity::Common;
        const float spin = show ? std::fmod(time * 40 + pickup.position.x * 13, 360.0F) : 0;
        const Vector3 at = show ? Vector3Add(pickup.position, {0, .06F + .04F * std::sin(time * 2.2F + pickup.position.z), 0}) : pickup.position;
        if (model)
            DrawModelEx(*model, at, {0, 1, 0}, spin,
                        {item->size, item->size, item->size}, tint);
    }
    if (light)
        DrawCircle3D({player.x, player.y + .02F, player.z}, 3, {1, 0, 0}, 90, Fade(YELLOW, .35F));
}
} // namespace outland::game::inventory
