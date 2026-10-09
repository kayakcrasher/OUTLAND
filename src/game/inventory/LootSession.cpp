#include "outland/game/inventory/LootSession.hpp"
#include <algorithm>
#include <filesystem>
#include <cmath>
namespace outland::game::inventory {
int LootSession::ammo_count(combat::WeaponId weapon) const {
    int count = 0;
    for (const auto &stack : inventory_.state().stacks) {
        const auto *item = registry_.find(stack.item);
        if (item && item->category == ItemCategory::Ammo && item->weapon == weapon)
            count += stack.quantity;
    }
    return count;
}
int LootSession::take_ammo(combat::WeaponId weapon, int requested) {
    int taken = 0;
    for (const auto &item : registry_.items())
        if (item.category == ItemCategory::Ammo && item.weapon == weapon && taken < requested)
            taken += inventory_.remove(item.id, requested - taken);
    if (taken > 0)
        changed();
    return taken;
}
LootSnapshot LootSession::snapshot(const combat::WeaponSystem &weapons) const {
    return {mode_,
            inventory_.state(),
            world_.state(),
            weapons.selected(),
            {weapons.ammo(combat::WeaponId::Pistol).loaded,
             weapons.ammo(combat::WeaponId::Rifle).loaded}};
}
void LootSession::start(GameMode mode, const std::string &map, combat::WeaponSystem &weapons) {
    weapons.unbind_inventory();
    inventory_.clear();
    world_.clear();
    mode_ = mode;
    path_ = LootSave::profile_path(map, mode);
    blocked_save_ = false;
    dirty_ = false;
    quiet_ = reconcile_remaining_ = 0;
    status_.clear();
    for (std::size_t i = 0; i < combat::weapons.size(); ++i)
        weapons.set_loaded(static_cast<combat::WeaponId>(i), 0);
    for (const auto &item : registry_.items())
        if (item.starting_quantity > 0) {
            const int added = inventory_.add(registry_, item.id, item.starting_quantity);
            if (added != item.starting_quantity)
                status_ = "Starter bag exceeds capacity";
            if (item.category == ItemCategory::Weapon && added > 0)
                weapons.set_loaded(*item.weapon, item.starting_loaded);
        }
    std::error_code ec;
    if (std::filesystem::exists(path_, ec)) {
        LootSnapshot state;
        state.mode = mode;
        if (LootSave::load(registry_, state, path_, status_)) {
            inventory_.restore(registry_, std::move(state.inventory));
            world_.restore(registry_, std::move(state.world));
            for (std::size_t i = 0; i < state.loaded.size(); ++i)
                weapons.set_loaded(static_cast<combat::WeaponId>(i), state.loaded[i]);
            weapons.select(state.selected);
            status_ = "Inventory restored";
        } else {
            blocked_save_ = true;
            status_ = "Saved inventory unavailable; existing save kept";
        }
    } else if (ec) {
        blocked_save_ = true;
        status_ = "Inventory storage unavailable";
    }
    weapons.bind_inventory([this](auto id) { return inventory_.owns_weapon(registry_, id); },
                           [this](auto id) { return ammo_count(id); },
                           [this](auto id, int count) { return take_ammo(id, count); });
}
void LootSession::update(float dt, const world::VerdaRegion &region, Vector3 player,
                         combat::WeaponSystem &weapons) {
    if (!std::isfinite(dt) || dt <= 0 || mode_ == GameMode::Home || mode_ == GameMode::DevLab)
        return;
    reconcile_remaining_ -= dt;
    if (reconcile_remaining_ <= 0) {
        if (world_.reconcile(region, registry_, mode_ == GameMode::ZombieSurvival, player))
            changed();
        reconcile_remaining_ = .5F;
    }
    if (weapons.events().shots > 0)
        changed();
    if (dirty_) {
        quiet_ += dt;
        const float delay = status_.starts_with("SAVE FAILED") ? 5 : 1;
        if (quiet_ >= delay)
            save(weapons);
    }
}
bool LootSession::save(const combat::WeaponSystem &weapons) {
    if (mode_ == GameMode::Home || mode_ == GameMode::DevLab)
        return true;
    if (blocked_save_)
        return false;
    std::string error;
    const bool success = LootSave::save(registry_, snapshot(weapons), path_, error);
    if (success) {
        dirty_ = false;
        status_ = "Inventory saved";
    } else
        status_ = "SAVE FAILED - bag kept; retry on exit";
    quiet_ = 0;
    return success;
}
std::string LootSession::interact(Vector3 player, combat::WeaponSystem &weapons,
                                  const std::function<bool(Vector3)> &reachable) {
    const int index = world_.nearest(player, 2.8F, reachable);
    if (index < 0)
        return "Nothing to pick up nearby";
    auto &pickup = world_.edit().pickups[static_cast<std::size_t>(index)];
    const auto *item = registry_.find(pickup.item);
    if (!item)
        return "Unknown pickup";
    const int amount = inventory_.add(registry_, item->id, pickup.quantity);
    if (amount == 0)
        return item->category == ItemCategory::Weapon &&
                       inventory_.owns_weapon(registry_, *item->weapon)
                   ? "You already carry this weapon type"
                   : "Bag full - drop an item or equip a backpack";
    if (item->category == ItemCategory::Weapon)
        weapons.set_loaded(*item->weapon, pickup.loaded_rounds);
    pickup.quantity -= amount;
    const auto message = "Picked up " + std::to_string(amount) + " x " + item->name;
    if (pickup.quantity == 0)
        world_.edit().pickups.erase(world_.edit().pickups.begin() + index);
    changed();
    return message;
}
std::string LootSession::use(std::size_t index, combat::WeaponSystem &weapons, Health &health) {
    if (index >= inventory_.state().stacks.size())
        return "Select an item";
    const auto id = inventory_.state().stacks[index].item;
    const auto *item = registry_.find(id);
    if (!item)
        return "Unknown item";
    switch (item->action) {
    case ItemAction::Eat:
    case ItemAction::Drink: {
        const bool food = item->action == ItemAction::Eat;
        inventory_.remove(id, 1);
        if (food)
            inventory_.effects().food_consumed =
                std::min(1e9F, inventory_.state().food_consumed + item->value);
        else
            inventory_.effects().water_consumed =
                std::min(1e9F, inventory_.state().water_consumed + item->value);
        if (consumed)
            consumed(food ? item->value : 0, food ? 0 : item->value);
        changed();
        return "Consumed " + item->name;
    }
    case ItemAction::Heal:
        if (!health.alive() || health.current() >= health.maximum())
            return "Medical item not needed";
        inventory_.remove(id, 1);
        health.heal(item->value);
        changed();
        return "Used " + item->name;
    case ItemAction::Equip:
        if (!weapons.select(*item->weapon))
            return "Weapon unavailable";
        changed();
        return "Equipped " + item->name;
    case ItemAction::Ammo:
        if (!weapons.available() || item->weapon != weapons.selected())
            return "Equip a compatible weapon first";
        weapons.request_reload();
        return weapons.reload_remaining() > 0 ? "Reloading from bag" : "Magazine is full";
    case ItemAction::Container:
        if (!inventory_.equip_container(registry_, id))
            return "Cannot equip this container";
        changed();
        return "Equipped " + item->name;
    case ItemAction::Light: {
        const bool on = !inventory_.state().light;
        if (on && !item->requires_item.empty() && inventory_.count(item->requires_item) == 0)
            return "A battery is required";
        if (on && !item->requires_item.empty())
            inventory_.remove(item->requires_item, 1);
        inventory_.effects().light = on;
        changed();
        return on ? "Light on - one battery used" : "Light off";
    }
    default:
        return "Carry this item, or drop it into the world";
    }
}
std::string LootSession::drop(std::size_t index, bool whole, Vector3 position,
                              combat::WeaponSystem &weapons,
                              const std::function<bool(Vector3)> &reachable) {
    if (index >= inventory_.state().stacks.size())
        return "Select an item";
    if (reachable && !reachable(position))
        return "Cannot drop through an obstacle";
    const auto stack = inventory_.state().stacks[index];
    const auto *item = registry_.find(stack.item);
    if (!item)
        return "Unknown item";
    const int quantity = whole ? stack.quantity : 1;
    if (!inventory_.can_remove(registry_, index, quantity))
        return "Empty extra bag slots before dropping the backpack";
    const int loaded =
        item->category == ItemCategory::Weapon ? weapons.ammo(*item->weapon).loaded : 0;
    if (!world_.drop(registry_, stack.item, quantity, position, loaded))
        return "Cannot drop here - world pickup limit reached";
    inventory_.remove(stack.item, quantity);
    if (item->category == ItemCategory::Weapon)
        weapons.set_loaded(*item->weapon, 0);
    if (item->action == ItemAction::Light && inventory_.count(item->id) == 0)
        inventory_.effects().light = false;
    changed();
    return "Dropped " + std::to_string(quantity) + " x " + item->name;
}
} // namespace outland::game::inventory
