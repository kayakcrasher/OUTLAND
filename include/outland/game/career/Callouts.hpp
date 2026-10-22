#pragma once
#include "outland/game/career/Career.hpp"
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::game::ai { class BattleRoyaleBots; class BotDrivers; }
namespace outland::game::vehicles { class VehicleSystem; }
namespace outland::game::mafia { class Mafio; }

// Crime calls for a serving officer (Explore): gang members caught in the act.
//
// The dispatcher puts out a call - a robbery at a shop or a drive-by at a house, in a town not far
// off - with who did it (the gang, by its shirts) and the getaway car (its colour, its model, its
// plate). Two of the nearest crew are at the scene with the car (game::mafia's heists); they run for
// their hideout when the officer gets close. Stop them - stay on their bumper with the lights on
// until they pull over, or wreck the car - and they get out: most put their hands up, some fight
// until they're hurt. USE cuffs a suspect with his hands up; cuffed, he walks with the officer, and
// climbs into the back when the officer gets in a police car close by. The state prison's gate
// books them in (game::career::Career::deliver). A call is done when both suspects are in the car
// or down; it is lost when they make it home.
//
// The first call comes with a partner (Officer Novak) who talks the officer through it.
namespace outland::game::career {

struct CallScene {            // where a crime can happen
    Vector3 door{};           // outside the front door (feet)
    Vector3 out{0, 0, 1};     // the way the front faces (flat, unit)
    std::string place, town;  // "the Armilejo", "Roka"
    bool shop{true};          // a robbery (shops, pubs) or a drive-by (houses)
};

struct Escort {
    int bot{-1}, gang{-1};
    bool suspect{false};      // one of the call's suspects
    bool walking{false};      // moving this frame (the game draws him walking)
};

struct CalloutLine { std::string speaker, text; };

class Callouts {
public:
    static constexpr float scene_near = 180, scene_far = 900;  // from the officer
    static constexpr float crew_reach = 1600;                   // the crew that does it is from near the scene
    static constexpr double call_gap = 40;                      // at least this between calls
    static constexpr float cuff_reach = 3.4F;                   // USE this close to a suspect with his hands up
    static constexpr float board_reach = 14;                    // in the police car with him this close: he gets in
    static constexpr float escort_gap = 1.5F;                   // he walks this far behind
    static constexpr float escort_lost = 170;                   // left this far behind, he slips away
    static constexpr float line_seconds = 5.5F;                 // the partner's lesson, a line at a time

    struct Frame {
        float dt{0};
        double now{0};
        Vector3 officer{};                 // feet
        float yaw{0};                      // the officer's heading (radians, as the player's)
        bool alive{true};
        std::string police_car;            // the police car the officer drives ("" on foot or another car)
        Vector3 car{};                     // ...where it is
    };

    void reset() { *this = Callouts{}; }

    // Puts out a call (true): a crew near one of `scenes` stages a heist with a getaway car parked
    // at the kerb, and the career takes the call.
    bool dispatch(const Frame& frame, const std::vector<CallScene>& scenes, Career& career, mafia::Mafio& mafio,
                  ai::BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region, std::uint64_t seed);
    // Every frame: the call follows its suspects; the cuffed walk with the officer and get in the car.
    // `said` is set when something should be on the screen.
    void update(const Frame& frame, Career& career, mafia::Mafio& mafio, ai::BattleRoyaleBots& bots,
                vehicles::VehicleSystem& cars, world::VerdaRegion& region, std::string& said);

    // The Krimulo within cuffing reach with his hands up (a suspect who gave up), or -1.
    [[nodiscard]] int to_cuff(Vector3 feet, const mafia::Mafio& mafio, ai::BattleRoyaleBots& bots) const;
    // Cuffs him: he walks with the officer from now on (any Krimulo the game lets the officer take).
    bool cuff(int bot, int gang, mafia::Mafio& mafio, ai::BattleRoyaleBots& bots);
    [[nodiscard]] const std::vector<Escort>& escorts() const { return escorts_; }
    [[nodiscard]] bool escorting(int bot) const;
    [[nodiscard]] bool suspect(int bot) const;

    // The call in hand: the getaway car (vehicle id), its plate and what it looks like.
    [[nodiscard]] bool active() const { return active_; }
    [[nodiscard]] const std::string& car() const { return car_; }
    [[nodiscard]] const std::string& plate() const { return plate_; }
    [[nodiscard]] const std::string& look() const { return look_; }
    [[nodiscard]] int gang() const { return gang_; }

    // The partner's lesson on the first call: the line showing (nullptr: none).
    [[nodiscard]] const CalloutLine* line() const { return lesson_.empty() ? nullptr : &lesson_.front(); }
    // The first call wants a partner beside the officer (once).
    bool take_partner_wanted() { const bool w = partner_wanted_; partner_wanted_ = false; return w; }
    // A word from the partner out of turn (a hint as the call goes on).
    void hint(const std::string& text);

    // What a gang's shirts look like, for the radio ("rust-red shirts").
    static const char* shirts(int gang);
    // A plate for a getaway car ("VRD-472") from a seed.
    static std::string make_plate(std::uint64_t seed);

private:
    void end(vehicles::VehicleSystem& cars, world::VerdaRegion& region);
    bool active_{false};
    std::string car_, plate_, look_, stale_car_;
    int gang_{-1};
    std::vector<int> suspects_;          // still to be stopped
    std::vector<Escort> escorts_;
    std::vector<CalloutLine> lesson_;
    float line_time_{0};
    bool partner_wanted_{false}, hinted_hands_{false}, hinted_cuffed_{false}, hinted_aboard_{false}, first_{false};
    double next_call_{0};
};

} // namespace outland::game::career
