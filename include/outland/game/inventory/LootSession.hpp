#pragma once
#include <utility>
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
    // HEAL (H / the HEAL button): the medical item that best covers what is missing, else a bite or a
    // drink for the boost.
    std::string quick_heal(combat::WeaponSystem &, Health &);
    // Boost (0..100), from food and drink: health comes back slowly while it lasts. Call every frame.
    void tick(float dt, Health &);
    float boost() const { return boost_; }
    // Auto pickup's latest message ("+24 x 9mm rounds"), once; empty when nothing was picked up.
    std::string take_auto_message() { return std::exchange(auto_message_, {}); }
    static constexpr float boost_max = 100, boost_heal = .6F, boost_drain = 1.0F, auto_radius = 1.6F;
    static constexpr float rest_cooldown = 120;
    std::string drop(std::size_t slot, bool whole_stack, Vector3 position, combat::WeaponSystem &,
                     const std::function<bool(Vector3)> &reachable = {});
    std::string trade(const economy::Economy&, const economy::Shop&, const economy::Offer&,
                      int quantity, bool selling, combat::WeaponSystem&);
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
    economy::EconomyState& economy() { return economy_; }
    const economy::EconomyState& economy() const { return economy_; }
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
    economy::EconomyState economy_;
    LootWorld world_;
    GameMode mode_{GameMode::Home};
    std::string path_, status_;
    bool dirty_{false}, blocked_save_{false};
    float quiet_{0}, reconcile_remaining_{0};
    float boost_{0}, auto_wait_{0}, rest_wait_{0};
    std::string auto_message_;
    // Ammo for a gun you carry, medicine, food and drink, grenades and batteries are picked up just
    // by walking over them (not what you dropped yourself).
    bool auto_pickup(const ItemDefinition &, const WorldPickup &) const;
};
} // namespace outland::game::inventory
