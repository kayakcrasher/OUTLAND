#pragma once
#include "outland/game/combat/CombatWorld.hpp"
#include "outland/game/vehicles/VehicleRegistry.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <functional>
#include <unordered_map>
namespace outland::game::vehicles {
enum class VehicleZone { Body, Window, Engine, Tire0, Tire1, Tire2, Tire3, Occupant };
struct VehicleInput {
    float throttle{}, steer{};
    bool brake{}, parking{}, horn{};
};
struct VehicleRuntime {
    std::string id;
    Vector3 position{};
    float yaw{}, speed{}, steering{}, wheel_angle{}, pitch{}, roll{};
    bool sleeping{true};
};
struct VehicleEvents {
    bool started{}, stopped{}, door{}, horn{}, parking{}, collision{};
};
class VehicleSystem {
  public:
    explicit VehicleSystem(const VehicleRegistry &registry) : registry_(registry) {}
    bool reconcile(world::VerdaRegion &region);
    bool spawn(world::VerdaRegion &region, Vector3 near, float yaw,
               std::string_view definition = "hatchback");
    int nearest(const world::VerdaRegion &region, Vector3 player, float radius = 3) const;
    bool enter(world::VerdaRegion &region, int index, Vector3 player);
    bool exit(world::VerdaRegion &region, Vector3 &destination, bool forced = false);
    bool update(float dt, VehicleInput input, world::VerdaRegion &region, Vector3 player,
                bool paused = false);
    combat::BulletHit trace(const world::VerdaRegion &region, Vector3 start, Vector3 end,
                            bool occupants = true) const;
    void damage(world::VerdaRegion &region, const combat::BulletHit &hit, float amount);
    bool blocked(const world::VerdaRegion &region, Vector3 position, float yaw,
                 const VehicleDefinition &definition, float scale,
                 std::string_view ignore = {}) const;
    const VehicleDefinition *definition(const world::WorldAsset &asset) const {
        return registry_.find(asset.vehicle.definition);
    }
    world::WorldAsset *asset(world::VerdaRegion &region, std::string_view id) const;
    const world::WorldAsset *asset(const world::VerdaRegion &region, std::string_view id) const;
    const std::vector<VehicleRuntime> &vehicles() const { return vehicles_; }
    const VehicleRuntime *driver() const;
    Vector3 seat(const world::VerdaRegion &region) const;
    void leave_session() {
        const bool occupied = !driver_id_.empty();
        driver_id_.clear();
        for (auto &v : vehicles_)
            v.speed = 0;
        events_.stopped |= occupied;
    }
    void begin_frame() { events_ = {}; }
    const VehicleEvents &events() const { return events_; }
    void bind_occupant_damage(std::function<void(float)> damage) {
        occupant_damage_ = std::move(damage);
    }
    void bind_fuel(std::function<float(const std::string &, float)> consume) {
        fuel_hook_ = std::move(consume);
    }
    bool save_state(const world::VerdaRegion &region, const std::string &path,
                    std::string &error) const;
    bool load_state(world::VerdaRegion &region, const std::string &path, std::string &error);
    static Vector3 world_point(Vector3 local, Vector3 origin, float yaw, float scale = 1);

  private:
    const VehicleRegistry &registry_;
    std::vector<VehicleRuntime> vehicles_;
    std::string driver_id_;
    VehicleEvents events_{};
    std::function<void(float)> occupant_damage_;
    std::function<float(const std::string &, float)> fuel_hook_;
    unsigned spawn_counter_{0};
    world::VerdaRegion *owner_{nullptr};
    std::unordered_map<std::string, world::WorldAsset *> assets_;
};
} // namespace outland::game::vehicles
