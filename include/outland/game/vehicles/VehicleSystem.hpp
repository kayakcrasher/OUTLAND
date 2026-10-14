#pragma once
#include "outland/game/combat/CombatWorld.hpp"
#include "outland/game/vehicles/VehicleRegistry.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <functional>
#include <unordered_map>
#include <unordered_set>
namespace outland::game::vehicles {
enum class VehicleZone { Body, Window, Engine, Tire0, Tire1, Tire2, Tire3, Occupant };
struct VehicleInput {
    float throttle{}, steer{};
    bool brake{}, parking{}, horn{};
    // brake is the handbrake: it stops the car and lets the back step out (drifting).
    bool boost{};   // more power and a little more top speed, burning more fuel
};
struct VehicleRuntime {
    std::string id;
    Vector3 position{};
    float yaw{}, speed{}, steering{}, wheel_angle{}, pitch{}, roll{};
    float slip{};   // sideways velocity (m/s, + to the car's right): momentum carried through a turn
    bool boosting{false};
    bool sleeping{true};
    float escape{0}; // metres a car that started inside an obstacle may still travel through it
    float wrecked{0}; // seconds a wreck has been out of the player's sight (towed home after tow_seconds)
};
struct VehicleEvents {
    bool started{}, stopped{}, door{}, horn{}, parking{}, collision{};
    float crash{}; // the player's car hit something this frame: the impact speed (m/s)
};
// Backcountry yards keep dead cars as parts donors (marker ids "vehicle_spawn_<type>_donor_..."):
// they never run and sit on blocks where their wheels were.
inline bool parts_donor(const std::string& marker) { return marker.find("_donor_") != std::string::npos; }
class VehicleSystem {
  public:
    // Runtime vehicles at most (parked, driven and spawned together).
    static constexpr std::size_t max_vehicles = 176;
    // Wrecks are towed home and repaired after this long with the player this far away.
    static constexpr float tow_seconds = 90, tow_distance = 70;
    explicit VehicleSystem(const VehicleRegistry &registry) : registry_(registry) {}
    bool reconcile(world::VerdaRegion &region);
    bool spawn(world::VerdaRegion &region, Vector3 near, float yaw,
               std::string_view definition = "hatchback");
    // Nearest usable vehicle whose body (not just its centre) is within `radius` of the player.
    int nearest(const world::VerdaRegion &region, Vector3 player, float radius = 2) const;
    void set_training_structure(bool solid) { training_structure_ = solid; }
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
    // Autopilot: a vehicle driven by someone other than the player (island life's drivers).
    // Its controls are applied every update until cleared; it never sleeps while set.
    void set_autopilot(std::string_view id, VehicleInput input) { autopilot_[std::string(id)] = input; }
    void clear_autopilot(std::string_view id) { autopilot_.erase(std::string(id)); }
    bool autopiloted(std::string_view id) const { return autopilot_.contains(std::string(id)); }
    void clear_autopilots() { autopilot_.clear(); }
    // Index into vehicles(), or -1.
    int find(std::string_view id) const;
    // Move a vehicle to a new pose at rest (off-screen traffic, parking it home). Refused while
    // the player drives it.
    bool place(world::VerdaRegion &region, std::string_view id, Vector3 position, float yaw);
    // Kinematic driving (a bus line): someone else moves the vehicle along its path each frame,
    // with this speed and steering for the wheels; physics leaves it alone until release().
    // Kinematic vehicles can't be entered.
    bool glide(world::VerdaRegion &region, std::string_view id, Vector3 position, float yaw,
               float speed, float steering, float dt);
    void release(std::string_view id) { kinematic_.erase(std::string(id)); }
    bool kinematic(std::string_view id) const { return kinematic_.contains(std::string(id)); }
    void bind_occupant_damage(std::function<void(float)> damage) {
        occupant_damage_ = std::move(damage);
    }
    void bind_fuel(std::function<float(const std::string &, float)> consume) {
        fuel_hook_ = std::move(consume);
    }
    // Match-local damage/driving must not alter the persistent Explore/Creator fleet.
    std::vector<world::WorldAsset> checkpoint(const world::VerdaRegion&) const;
    void restore_checkpoint(world::VerdaRegion&, const std::vector<world::WorldAsset>&);
    void reset_match_fleet(world::VerdaRegion&);
    bool save_state(const world::VerdaRegion &region, const std::string &path,
                    std::string &error) const;
    bool load_state(world::VerdaRegion &region, const std::string &path, std::string &error);
    static Vector3 world_point(Vector3 local, Vector3 origin, float yaw, float scale = 1);

  private:
    bool training_structure_{true};
    const VehicleRegistry &registry_;
    std::vector<VehicleRuntime> vehicles_;
    std::string driver_id_;
    VehicleEvents events_{};
    std::function<void(float)> occupant_damage_;
    std::function<float(const std::string &, float)> fuel_hook_;
    unsigned spawn_counter_{0};
    world::VerdaRegion *owner_{nullptr};
    std::unordered_map<std::string, world::WorldAsset *> assets_;
    std::unordered_map<std::string, VehicleInput> autopilot_;
    std::unordered_set<std::string> kinematic_;
};
} // namespace outland::game::vehicles
