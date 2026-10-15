#pragma once
#include "outland/game/life/LifeTypes.hpp"
#include "outland/world/Ranches.hpp"
#include <raylib.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::game::vehicles { class VehicleSystem; }
namespace outland::game::ai { class BattleRoyaleBots; }
namespace outland::characters { class CharacterRegistry; class NpcSystem; }

// The Vakeros (Explore): the ranching families on Verda's five ranches (world/Ranches.hpp). Each
// family of four wears cattleman hats and carries shotguns. By day they work: one rides herd on
// the cattle in the pasture, one works the fields, one drives the family's side-by-side round the
// farm, one minds the barn and the corral; at night they are home in the farmhouse. Each ranch
// runs a herd of beef cattle (Herefords and Angus) that graze and wander the pasture; for the
// cattle truck (game::freight) the family drives a few into the corral by the road, and they go
// up the chute into the trailer. A calf or two is born every morning.
//
// Vakeros keep themselves to themselves. Shoot at one of them, shoot their cattle or drive off in
// their side-by-side, and the whole family comes for you with shotguns until it blows over.
//
// The family are CombatBots in the shared ai::BattleRoyaleBots (Mafio::update ticks them; the
// Mafio's outsider rule asks hostile_to()); far from the player they sleep.
namespace outland::game::ranch {

enum class CowState : std::uint8_t { Grazing, Walking, ToCorral, InCorral, Loading, Gone, Dead };

struct Cow {
    Vector3 position{}, target{};
    float yaw{0};
    float pause{0};
    float stride{0};          // metres walked (the body's bob)
    float dead_for{0};
    bool angus{false};
    CowState state{CowState::Grazing};
    std::vector<Vector3> path; // waypoints still to walk (to the corral, up the chute)
};

enum class Role : std::uint8_t { Herd, Fields, Driver, Barn };

struct Vakero {
    int bot{-1};
    int ranch{-1};
    Role role{Role::Herd};
    std::string name;         // "Rafael Garza"
    std::string character;    // body
    bool female{false};
};

struct RanchState {
    const world::ranches::Ranch* spec{nullptr};
    world::ranches::Frame frame;
    std::string settlement;
    std::vector<Cow> herd;
    std::vector<int> family;  // indices into vakeros()
    std::string sxs;          // the side-by-side's vehicle id ("" if none)
    std::vector<Vector3> loop; // the farm-road loop the side-by-side runs (world)
    float loop_length{0}, sxs_progress{0}, sxs_speed{0}, sxs_wait{0};
    bool sxs_running{false};
    double provoked_until{-1e9};
    int calved_day{-1};
    int shipped{0};           // cattle sent off so far
    bool calling{false};      // cattle are being brought to the corral for the truck
};

// The stockyard's pen: where shipped cattle end up.
struct Stockyard {
    bool found{false};
    world::ranches::Frame frame;
    std::vector<Cow> pen;
    int received{0};
};

struct RanchFrame {
    float dt{0};
    double now{0};
    life::WorldClock clock{};
    Vector3 player_feet{};
    bool player_alive{true};
    bool player_fired{false};
    Vector3 aim_origin{}, aim_direction{0, 0, 1};
    std::string player_vehicle; // the vehicle the player drives ("" on foot)
};

// Someone at the wheel to draw (the side-by-side's driver).
struct SeatedDriver { std::string vehicle, character; bool hat{true}; };

class Ranches {
public:
    static constexpr int herd_size = 10, family_size = 4;
    static constexpr float wake_radius = 420, cattle_radius = 380;
    static constexpr float provoked_seconds = 90;
    static constexpr float sxs_speed = 6.5F;

    // Finds the ranches in the world, puts each family on its ranch and its cattle in the pasture.
    // Returns the number of ranches.
    int start(const world::VerdaRegion& region, ai::BattleRoyaleBots& bots, vehicles::VehicleSystem& cars,
              const characters::CharacterRegistry* characters, std::uint64_t seed, life::WorldClock clock);
    void clear(vehicles::VehicleSystem& cars);
    [[nodiscard]] bool active() const { return !ranches_.empty(); }

    void update(const RanchFrame& frame, ai::BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region);

    // The player hurt a Vakero: the family turns on the player.
    void provoke(int bot, double now);
    void provoke_ranch(int ranch, double now, const std::string& why);
    void forgive();
    [[nodiscard]] bool hostile_to(int a, int b, double now) const;
    [[nodiscard]] const Vakero* vakero(int bot) const;
    [[nodiscard]] bool angry(int ranch, double now) const;

    // The cattle truck: bring `count` head to the corral / load what is there (returns how many
    // went up the chute) / unload at the stockyard.
    void call_cattle(int ranch, int count);
    int load_cattle(int ranch, int max);
    void unload_cattle(int count);
    [[nodiscard]] int in_corral(int ranch) const;

    [[nodiscard]] const std::vector<RanchState>& ranches() const { return ranches_; }
    [[nodiscard]] const std::vector<Vakero>& vakeros() const { return vakeros_; }
    [[nodiscard]] const Stockyard& stockyard() const { return stockyard_; }
    [[nodiscard]] std::vector<SeatedDriver> drivers() const;
    // The ranch whose land `p` is on (-1 if none).
    [[nodiscard]] int ranch_at(Vector3 p) const;
    [[nodiscard]] const std::string& message() const { return message_; }
    [[nodiscard]] float message_time() const { return message_time_; }

private:
    void say(std::string text) { message_ = std::move(text); message_time_ = 6; }
    float random();
    Vector3 pasture_point(const RanchState& r);
    void update_herd(RanchState& r, int index, float dt, const world::VerdaRegion& region, bool near);
    void update_family(RanchState& r, const RanchFrame& frame, ai::BattleRoyaleBots& bots, bool near);
    void update_sxs(RanchState& r, const RanchFrame& frame, ai::BattleRoyaleBots& bots, vehicles::VehicleSystem& cars,
                    world::VerdaRegion& region, bool near);

    std::vector<RanchState> ranches_;
    std::vector<Vakero> vakeros_;
    Stockyard stockyard_;
    std::uint64_t rng_{0x9a3c51f7d2e4b081ULL};
    std::string message_;
    float message_time_{0};
};

} // namespace outland::game::ranch
