#pragma once
#include "outland/game/outbreak/Outbreak.hpp"
#include "outland/world/Ranches.hpp"
#include "outland/world/roads/RoadGraph.hpp"
#include <raylib.h>
#include <optional>
#include <string>
#include <vector>

namespace outland::characters { class NpcSystem; class CharacterRegistry; }
namespace outland::game::vehicles { class VehicleSystem; }
namespace outland::game::zombies { class ZombieEcology; }

// The outbreak in play (Zombie Survival): the cast on the island (Uncle Ron, Dario, the Garzas,
// the holdouts' leaders and guards), Ron riding along, the scenes at the farm, the guards
// keeping their ground clear, and what the player finds. The game feeds it a frame and acts on
// what comes back (shots to hear, the rifle to hand over, a save, a message); it draws the people
// (NpcSystem bots) and the lines.
namespace outland::game::outbreak {

struct Person {
    int bot{-1};
    std::string name;
    std::string character;
    Vector3 at{};
    float yaw{0};
    int weapon{-1};             // game::combat::WeaponId, -1 none
    int holdout{-1};            // index into Run::holdouts(), -1 the family
    int gang{-1};               // their gang's colours (game::mafia::gangs), -1 none
    bool leader{false};
    bool hidden{false};
    bool dead{false};
};

struct Shot { Vector3 from{}, to{}; int weapon{0}; };

struct RunFrame {
    float dt{0};
    Vector3 feet{};
    bool alive{true};
    std::string driving;        // the vehicle the player drives ("" on foot)
};

struct RunEvents {
    std::vector<Shot> shots;
    std::string message;
    bool rifle{false};          // the barn is theirs: hand over the hunting rifle
    bool save{false};
};

class Run {
public:
    static constexpr int bot_base = 975000;
    static constexpr float talk_range = 3.2F;

    // Puts the cast where the story stands (`state` from the save, or a new game) and says where
    // the player starts (`respawn`: back from the dead - at the hideout once there is one).
    void start(const OutbreakState& state, world::VerdaRegion& region, characters::NpcSystem& npcs,
               vehicles::VehicleSystem& cars, zombies::ZombieEcology& zombies, const characters::CharacterRegistry* registry,
               bool respawn);
    void clear(characters::NpcSystem& npcs);
    // Testing: straight to the drive's end - Ron aboard the pickup, just short of the farm.
    // Returns the pickup (the game puts the player at its wheel).
    std::string skip_to_farm(world::VerdaRegion& region, characters::NpcSystem& npcs, vehicles::VehicleSystem& cars);
    [[nodiscard]] bool active() const { return active_; }

    const RunEvents& update(const RunFrame& frame, world::VerdaRegion& region, characters::NpcSystem& npcs,
                            vehicles::VehicleSystem& cars, zombies::ZombieEcology& zombies);
    // USE: on with whoever is talking, or a word from whoever is close. True if it did something.
    bool use(Vector3 feet);
    [[nodiscard]] std::string prompt(Vector3 feet) const;

    [[nodiscard]] Vector3 start_feet() const { return start_feet_; }
    [[nodiscard]] float start_yaw() const { return start_yaw_; }

    [[nodiscard]] Story& story() { return story_; }
    [[nodiscard]] const Story& story() const { return story_; }
    [[nodiscard]] OutbreakState& state() { return state_; }
    [[nodiscard]] const OutbreakState& state() const { return state_; }
    [[nodiscard]] const std::vector<Person>& people() const { return people_; }
    [[nodiscard]] const Person* person(int bot) const;
    [[nodiscard]] const std::vector<Holdout>& holdouts() const { return holdouts_; }
    [[nodiscard]] bool found(int holdout) const;
    [[nodiscard]] const Tower& tower() const { return tower_; }
    [[nodiscard]] Vector3 hideout() const { return hideout_; }
    [[nodiscard]] bool has_hideout() const { return story_.stage() == Stage::Free; }
    // Where the story wants the player next (the pickup, the farm, the infected), if anywhere.
    [[nodiscard]] std::optional<Vector3> goal() const;
    [[nodiscard]] int infected_left() const;
    // Uncle Ron riding along: in which vehicle (the game draws him in its passenger seat).
    [[nodiscard]] const std::string& ron_car() const { return ron_car_; }
    // The line on screen: the story's, or a word in free play.
    [[nodiscard]] const Line* line() const;
    [[nodiscard]] bool line_is_story() const { return story_.talking(); }
    [[nodiscard]] float dark() const { return story_.fade(); }
    [[nodiscard]] std::vector<Clearing> clearings(Vector3 feet) const;

private:
    int spawn(characters::NpcSystem& npcs, Person person);
    void place(characters::NpcSystem& npcs, Person& p, Vector3 at, float yaw);
    Person* find(const std::string& name);
    void hold_ground(float dt, characters::NpcSystem& npcs, zombies::ZombieEcology& zombies);
    void farm(const RunFrame& frame, world::VerdaRegion& region, characters::NpcSystem& npcs,
              vehicles::VehicleSystem& cars, zombies::ZombieEcology& zombies);
    void say(const Line& line, float seconds = 7);
    std::vector<std::pair<Vector3, float>> safe_ground() const;
    void roam(float dt, world::VerdaRegion& region, vehicles::VehicleSystem& cars, const std::string& driving);

    bool active_{false};
    Story story_;
    OutbreakState state_;
    std::vector<Person> people_;
    std::vector<Holdout> holdouts_;
    std::vector<int> leader_talks_;
    Tower tower_;
    world::ranches::Frame ranch_;
    Vector3 hideout_{}, start_feet_{};
    float start_yaw_{0};
    const characters::CharacterRegistry* registry_{nullptr};
    std::string ron_car_;
    std::vector<int> infected_;          // the story's: the Garzas and their hands
    std::vector<int> ranchers_;          // people_ indices
    float fire_clock_{-1}, guard_clock_{0}, dario_clock_{0}, said_for_{0};
    Line said_{};
    bool saying_{false};
    std::uint64_t rng_{0x2545f4914f6cdd1dULL};
    RunEvents events_;
    // The Brotherhood on the road: two rigs going town to town.
    struct Rig { std::string vehicle; world::roads::Route route; float progress{0}, stuck{0}, reverse{0}; int attempts{0}, leg{0}; };
    std::vector<Rig> rigs_;
    world::roads::RoadGraph roads_;
    std::vector<Vector3> stops_;
};

} // namespace outland::game::outbreak
