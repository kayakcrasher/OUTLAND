#pragma once
#include "outland/game/inventory/LootSave.hpp"
#include "outland/game/combat/WeaponSystem.hpp"
#include "outland/game/combat/Health.hpp"
namespace outland::game::inventory {
class LootSession {
  public:
    explicit LootSession(const ItemRegistry &registry) : registry_(registry) {}
    LootSession(const LootSession &) = delete;
    LootSession &operator=(const LootSession &) = delete;
    void start(GameMode, const std::string &map_path, combat::WeaponSystem &);
    void update(float dt, const world::VerdaRegion &, Vector3 player, combat::WeaponSystem &);
    std::string interact(Vector3 player, combat::WeaponSystem &,
                         const std::function<bool(Vector3)> &reachable = {});
    std::string use(std::size_t slot, combat::WeaponSystem &, Health &);
    std::string drop(std::size_t slot, bool whole_stack, Vector3 position, combat::WeaponSystem &,
                     const std::function<bool(Vector3)> &reachable = {});
    bool save(const combat::WeaponSystem &);
    void changed() {
        dirty_ = true;
        quiet_ = 0;
    }
    const Inventory &inventory() const {
        return inventory_;
    }
    const LootWorld &world() const {
        return world_;
    }
    Inventory &edit_inventory() {
        return inventory_;
    }
    LootWorld &edit_world() {
        return world_;
    }
    bool dirty() const {
        return dirty_;
    }
    bool can_persist() const {
        return !blocked_save_;
    }
    const std::string &status() const {
        return status_;
    }
    const std::string &path() const {
        return path_;
    }
    LootSnapshot snapshot(const combat::WeaponSystem &) const;
    // Future survival-stat consumers may subscribe; no hunger/thirst simulation yet.
    std::function<void(float food, float water)> consumed;

  private:
    int ammo_count(combat::WeaponId) const;
    int take_ammo(combat::WeaponId, int);
    const ItemRegistry &registry_;
    Inventory inventory_;
    LootWorld world_;
    GameMode mode_{GameMode::Home};
    std::string path_, status_;
    bool dirty_{false}, blocked_save_{false};
    float quiet_{0}, reconcile_remaining_{0};
};
} // namespace outland::game::inventory
