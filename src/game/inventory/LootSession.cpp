#include "outland/game/inventory/LootSession.hpp"
#include <algorithm>
#include <filesystem>
#include <cmath>
namespace outland::game::inventory {
int LootSession::ammo_count(combat::WeaponId weapon) const {
    int count = 0;
    for (const auto &stack : inventory_.state().stacks) {
        const auto *item = registry_.find(stack.item);
        if (item && item->category == ItemCategory::Ammo && item->weapon == combat::ammo_of(weapon))
            count += stack.quantity;
    }
    return count;
}
int LootSession::take_ammo(combat::WeaponId weapon, int requested) {
    int taken = 0;
    for (const auto &item : registry_.items())
        if (item.category == ItemCategory::Ammo && item.weapon == combat::ammo_of(weapon) && taken < requested)
            taken += inventory_.remove(item.id, requested - taken);
    if (taken > 0)
        changed();
    return taken;
}
LootSnapshot LootSession::snapshot(const combat::WeaponSystem &weapons) const {
    LootSnapshot state{mode_, inventory_.state(), world_.state(), weapons.selected(), {}, economy_};
    for (std::size_t i = 0; i < combat::weapon_count; ++i)
        state.loaded[i] = weapons.ammo(static_cast<combat::WeaponId>(i)).loaded;
    return state;
}
void LootSession::start(GameMode mode, const std::string &map, combat::WeaponSystem &weapons) {
    weapons.unbind_inventory();
    inventory_.clear();
    world_.clear();
    mode_ = mode;
    economy_ = {};
    if(mode != GameMode::Explore) economy_.money=0;
    path_ = LootSave::profile_path(map, mode);
    blocked_save_ = false;
    dirty_ = false;
    quiet_ = reconcile_remaining_ = 0;
    boost_ = auto_wait_ = rest_wait_ = 0;
    auto_message_.clear();
    status_.clear();
    for (std::size_t i = 0; i < combat::weapons.size(); ++i)
        weapons.set_loaded(static_cast<combat::WeaponId>(i), 0);
    // Battle Royale: everyone drops with only a pistol (loaded, and a couple of spare
    // magazines); the rest is found on the ground. Other modes use the catalog's starter kit.
    if (mode == GameMode::BattleRoyale) {
        for (const auto &item : registry_.items())
            if (item.category == ItemCategory::Weapon && item.weapon == combat::WeaponId::Pistol &&
                inventory_.add(registry_, item.id, 1) == 1) {
                weapons.set_loaded(combat::WeaponId::Pistol, combat::definition(combat::WeaponId::Pistol).magazine);
                if (const auto *ammo = registry_.ammo_for(item))
                    inventory_.add(registry_, ammo->id, combat::definition(combat::WeaponId::Pistol).magazine * 2);
                break;
            }
    } else
    for (const auto &item : registry_.items())
        if (item.starting_quantity > 0) {
            const int added = inventory_.add(registry_, item.id, item.starting_quantity);
            if (added != item.starting_quantity)
                status_ = "Starter bag exceeds capacity";
            if (item.category == ItemCategory::Weapon && added > 0)
                weapons.set_loaded(*item.weapon, item.starting_loaded);
        }
    std::error_code ec;
    if (mode != GameMode::BattleRoyale && std::filesystem::exists(path_, ec)) {
        LootSnapshot state;
        state.mode = mode;
        if (LootSave::load(registry_, state, path_, status_)) {
            inventory_.restore(registry_, std::move(state.inventory));
            world_.restore(registry_, std::move(state.world));
            for (std::size_t i = 0; i < state.loaded.size(); ++i)
                weapons.set_loaded(static_cast<combat::WeaponId>(i), state.loaded[i]);
            weapons.select(state.selected);
            economy_ = std::move(state.economy);
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
    if (mode == GameMode::BattleRoyale) weapons.select(combat::WeaponId::Pistol);
}
void LootSession::update(float dt, const world::VerdaRegion &region, Vector3 player,
                         combat::WeaponSystem &weapons) {
    if (!std::isfinite(dt) || dt <= 0 || mode_ == GameMode::Home || mode_ == GameMode::DevLab)
        return;
    reconcile_remaining_ -= dt;
    if (reconcile_remaining_ <= 0) {
        if (world_.reconcile(region, registry_, mode_ == GameMode::ZombieSurvival ? LootMode::Zombie : mode_ == GameMode::BattleRoyale ? LootMode::Royale : LootMode::Normal, player))
            changed();
        reconcile_remaining_ = .5F;
    }
    if (weapons.events().shots > 0)
        changed();
    rest_wait_ = std::max(0.0F, rest_wait_ - dt);
    // Auto pickup, a few times a second.
    auto_wait_ -= dt;
    if (auto_wait_ <= 0) {
        auto_wait_ = .2F;
        auto &pickups = world_.edit().pickups;
        for (std::size_t i = 0; i < pickups.size();) {
            auto &pickup = pickups[i];
            const auto *item = registry_.find(pickup.item);
            const float dx = pickup.position.x - player.x, dz = pickup.position.z - player.z;
            if (!item || dx * dx + dz * dz > auto_radius * auto_radius || std::abs(pickup.position.y - player.y) > 2 ||
                !auto_pickup(*item, pickup)) {++i; continue;}
            const int amount = inventory_.add(registry_, item->id, pickup.quantity);
            if (amount <= 0) {++i; continue;}
            pickup.quantity -= amount;
            auto_message_ = "+" + std::to_string(amount) + " " + item->name;
            changed();
            if (taken) taken(pickup.id, pickup.quantity);
            if (pickup.quantity == 0) pickups.erase(pickups.begin() + static_cast<std::ptrdiff_t>(i));
            else ++i;
        }
    }
    if (dirty_) {
        quiet_ += dt;
        const float delay = status_.starts_with("SAVE FAILED") ? 5 : 1;
        if (quiet_ >= delay)
            save(weapons);
    }
}
bool LootSession::auto_pickup(const ItemDefinition &item, const WorldPickup &pickup) const {
    if (pickup.id.starts_with("drop_")) return false; // what the player put down stays put
    switch (item.category) {
    case ItemCategory::Ammo:
        if (!item.weapon) return false;
        // Shells for any shotgun carried.
        for (std::size_t i = 0; i < combat::weapon_count; ++i)
            if (combat::ammo_of(static_cast<combat::WeaponId>(i)) == *item.weapon &&
                inventory_.owns_weapon(registry_, static_cast<combat::WeaponId>(i)))
                return true;
        return false;
    case ItemCategory::Medical: case ItemCategory::Food: case ItemCategory::Water: return true;
    default: return item.id == "frag_grenade" || item.id == "battery";
    }
}

void LootSession::tick(float dt, Health &health) {
    if (!std::isfinite(dt) || dt <= 0 || boost_ <= 0) return;
    if (health.alive() && health.current() < health.maximum()) health.heal(boost_heal * dt);
    boost_ = std::max(0.0F, boost_ - boost_drain * dt);
}

bool LootSession::can_heal(const Health &health) const {
    if (!health.alive() || health.maximum() - health.current() < 1) return false;
    for (const auto &stack : inventory_.state().stacks)
        if (const auto *item = registry_.find(stack.item);
            item && (item->action == ItemAction::Heal || item->action == ItemAction::Eat || item->action == ItemAction::Drink)) return true;
    return false;
}

std::string LootSession::quick_heal(combat::WeaponSystem &weapons, Health &health) {
    if (!health.alive()) return {};
    const float missing = health.maximum() - health.current();
    if (missing < 1) return "Health is full";
    // The smallest medical item that covers what is missing, else the biggest there is.
    std::size_t best = inventory_.state().stacks.size();
    float best_value = 0;
    for (std::size_t i = 0; i < inventory_.state().stacks.size(); ++i) {
        const auto *item = registry_.find(inventory_.state().stacks[i].item);
        if (!item || item->action != ItemAction::Heal) continue;
        const bool covers = item->value >= missing, best_covers = best_value >= missing;
        if (best == inventory_.state().stacks.size() || (covers && (!best_covers || item->value < best_value)) ||
            (!covers && !best_covers && item->value > best_value)) {best = i; best_value = item->value;}
    }
    if (best < inventory_.state().stacks.size()) return use(best, weapons, health);
    // No medicine: something to eat or drink, for the boost.
    if (boost_ < boost_max * .5F)
        for (std::size_t i = 0; i < inventory_.state().stacks.size(); ++i) {
            const auto *item = registry_.find(inventory_.state().stacks[i].item);
            if (item && (item->action == ItemAction::Eat || item->action == ItemAction::Drink)) return use(i, weapons, health);
        }
    return "No medical supplies";
}

std::string LootSession::trade(const economy::Economy& market, const economy::Shop& shop,
                               const economy::Offer& offer, int quantity, bool selling,
                               combat::WeaponSystem& weapons) {
    if(mode_!=GameMode::Explore) return "Stores are available in Explore";
    if(blocked_save_) return "Saved profile unavailable; trade disabled to protect it";
    const auto* item=registry_.find(offer.item);
    if(!item) return "Unknown store item";
    if(selling && item->category==ItemCategory::Weapon && weapons.ammo(*item->weapon).loaded>0)
        return "Spend loaded rounds or drop the weapon before selling it";
    std::string result=market.trade(economy_,inventory_,registry_,shop,offer,quantity,selling);
    if(!result.starts_with("Bought ") && !result.starts_with("Sold ")) return result;
    if(item->category==ItemCategory::Weapon) {
        weapons.set_loaded(*item->weapon,0);
        if(!selling) weapons.select(*item->weapon);
        else if(!weapons.available()) for(std::size_t i=0;i<combat::weapons.size();++i) {
            const auto id=static_cast<combat::WeaponId>(i);
            if(inventory_.owns_weapon(registry_,id)) {weapons.select(id);break;}
        }
    }
    changed();
    if(!save(weapons)) result+=" - SAVE FAILED; kept in memory, retry before exit";
    return result;
}
bool LootSession::save(const combat::WeaponSystem &weapons) {
    if (mode_ == GameMode::Home || mode_ == GameMode::DevLab || mode_ == GameMode::BattleRoyale) {
        dirty_ = false;
        return true;
    }
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
    if (taken) taken(pickup.id, pickup.quantity);
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
        // Food and drink boost: health comes back slowly while it lasts.
        boost_ = std::min(boost_max, boost_ + item->value);
        changed();
        return "Consumed " + item->name + " - boost " + std::to_string(static_cast<int>(boost_));
    }
    case ItemAction::Heal:
        if (!health.alive() || health.current() >= health.maximum())
            return "Medical item not needed";
        inventory_.remove(id, 1);
        health.heal(item->value);
        changed();
        return "Used " + item->name;
    case ItemAction::Equip:
        // The gun in hand goes back in the bag: unarmed.
        if (weapons.available() && weapons.selected() == *item->weapon) {
            weapons.holster();
            return "Put away " + item->name + " - unarmed";
        }
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
        // Rest on a bedroll or in a tent (when you are not in a hurry): back to full health.
        if (id == "sleeping_bag" || id == "tent") {
            if (rest_wait_ > 0) return "Too restless to sleep again yet (" + std::to_string(static_cast<int>(rest_wait_)) + " s)";
            if (!health.alive() || health.current() >= health.maximum()) return "You are rested";
            health.reset();
            boost_ = std::min(boost_max, boost_ + 40);
            rest_wait_ = rest_cooldown;
            changed();
            return "You rest a while in the " + std::string(id == "tent" ? "tent" : "sleeping bag") + " - full health";
        }
        // A campfire kit and something to light it with: warmth and a hot meal's worth of boost.
        if (id == "bonfire") {
            const bool lighter = inventory_.count("lighter") > 0;
            if (!lighter && inventory_.count("matches") == 0) return "Matches or a lighter are needed to light it";
            inventory_.remove(id, 1);
            if (!lighter) inventory_.remove("matches", 1);
            if (health.alive()) health.heal(30);
            boost_ = std::min(boost_max, boost_ + 50);
            changed();
            return "You sit by the campfire - warmed up";
        }
        return "Carry it, sell it at a store, or drop it";
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
    std::string id;
    if (!drop_prefix.empty()) {
        for (std::uint64_t n = world_.state().next_drop;; ++n)
            if (!world_.find(id = drop_prefix + std::to_string(n))) break;
        world_.edit().next_drop += 1;
    }
    if (!world_.drop(registry_, stack.item, quantity, position, loaded, id))
        return "Cannot drop here - world pickup limit reached";
    if (dropped) dropped(world_.state().pickups.back());
    inventory_.remove(stack.item, quantity);
    if (item->category == ItemCategory::Weapon)
        weapons.set_loaded(*item->weapon, 0);
    if (item->action == ItemAction::Light && inventory_.count(item->id) == 0)
        inventory_.effects().light = false;
    changed();
    return "Dropped " + std::to_string(quantity) + " x " + item->name;
}
} // namespace outland::game::inventory
