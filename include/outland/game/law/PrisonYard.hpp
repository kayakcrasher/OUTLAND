#pragma once
#include "outland/characters/NpcSystem.hpp"
#include <raylib.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// The people of Ŝtata Malliberejo, the state prison (world::layout::prison_*): armed guards in the
// four corner towers and at the gate, two more walking the yard, and the inmates - the island's law
// breakers and the gang members the Policio has put away - milling about the exercise yard. Every
// prisoner an officer books in at the gate joins them (admit()), up to a full yard.
//
// They are NpcSystem bot bodies with ids from first_bot up (like the Chief): the game spawns a body
// for each and moves it to where update() says; nobody here can be hurt or fights back.
namespace outland::game::law {

struct PrisonPerson {
    int bot{-1};
    std::string character;
    bool guard{false}, gang{false};
    Vector3 local{};              // in the prison's frame (x across, z from the gate (-) to the back), y up
    float yaw{0};                 // in the prison's frame
    Vector3 target{};             // where they are walking (yard), local
    float wait{0};
    bool walking{false};
    Vector3 patrol_a{}, patrol_b{};   // a yard guard's beat
};

class PrisonYard {
public:
    static constexpr int first_bot = 920001;
    static constexpr int max_inmates = 16;
    static constexpr float walk_speed = 1.1F, patrol_speed = 1.3F;
    // Who wears which body: a guard, a gang member or a law breaker (seeded).
    using Chooser = std::function<std::string(bool guard, bool gang, std::uint64_t seed)>;

    // Guards at their posts and the yard's first inmates (law breakers and a few gangsters).
    void start(Vector3 centre, float yaw, std::uint64_t seed, const Chooser& choose);
    void clear() { people_.clear(); }
    // A prisoner booked in: into the yard. Returns the new person's bot id, or -1 when it is full.
    int admit(bool gang, const Chooser& choose);
    void update(float dt);

    // The snitch (a law breaker who talked to the Policio): someone a jailed gang member can put
    // in work on. Picks one from the yard (his bot id), or -1 when there's nobody.
    int mark_snitch();
    [[nodiscard]] int snitch() const { return snitch_; }
    // Stops someone where they stand for `seconds` (they turn to whoever is there).
    void pause(int bot, float seconds);
    // Out of the yard for good (the snitch, shanked).
    bool remove(int bot);
    [[nodiscard]] const PrisonPerson* find(int bot) const;

    [[nodiscard]] const std::vector<PrisonPerson>& people() const { return people_; }
    [[nodiscard]] int inmates() const;
    // A person's place in the world (feet; y from the ground at the prison's centre) and heading.
    [[nodiscard]] Vector3 world(const PrisonPerson& p) const;
    [[nodiscard]] float world_yaw(const PrisonPerson& p) const { return yaw_ + p.yaw; }
    [[nodiscard]] characters::NpcState state(const PrisonPerson& p) const {
        return p.walking ? characters::NpcState::Wander : characters::NpcState::Idle;
    }
    // Inside the walls (any point, world).
    [[nodiscard]] bool inside(Vector3 p) const;

private:
    Vector3 yard_spot();
    float random();
    std::vector<PrisonPerson> people_;
    Vector3 centre_{};
    float yaw_{0};
    std::uint64_t rng_{1}, seed_{0};
    int next_bot_{first_bot};
    int snitch_{-1};
};

} // namespace outland::game::law
