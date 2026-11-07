#pragma once
#include "outland/game/career/Callouts.hpp"
#include "outland/game/career/Career.hpp"
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::game::ai { class BattleRoyaleBots; class BotDrivers; }
namespace outland::game::vehicles { class VehicleSystem; }

// A patrol officer's routine calls (Explore): the ordinary work every officer does before the Chief
// takes them onto the Gang Task Force (game::career::Career::task_force_calls of them).
//
// - DOMESTIC DISPUTE: shouting at a house; the man is out front. Most calm down and put their
//   hands up; some run; a few pull a gun.
// - DRUNK DRIVER: a car weaving out of a pub's kerb, its colour, model and plate on the radio. Get
//   on its bumper with the lights on until it pulls over (or it crashes, or he gets home); out of
//   the car he nearly always gives up.
// - PHARMACY ROBBERY: an armed robber at a town's clinic pharmacy. He may give up, run, or fight.
// - TRAFFIC STOP: a speeder, colour, model and plate on the radio. Pull him over (bumper, lights);
//   walk up and USE writes the ticket and he drives on - unless he runs for it.
// - SHOPLIFTER: a man legging it from a shop with the goods. Catch him (or stop him) and cuff him.
// - BAR FIGHT: a drunk throwing chairs outside a pub. Mostly he gives up; some run.
// - STOLEN CAR: a car reported stolen, plate on the radio. Pull it over; the thief may give up,
//   run, or fight.
//
// One suspect a call: a civilian (no gang), a CombatBot in the shared ai::BattleRoyaleBots - he
// fights the officer only while he's resisting. Hands up, USE cuffs him; cuffed, he walks with the
// officer and climbs into the back of the police car, and the call is done. A runner gives up when
// caught or hurt; so does a gunman who's hurt. Officers may use lethal force: a suspect shot dead
// counts as stopped, and is never a crime.
namespace outland::game::career {

enum class RoutineKind : std::uint8_t { Domestic, DrunkDriver, Pharmacy, TrafficStop, Shoplifter, BarFight, StolenCar };
const char* routine_name(RoutineKind kind);   // "DOMESTIC DISPUTE"

class RoutineCalls {
public:
    static constexpr float scene_near = 150, scene_far = 900;   // from the officer
    static constexpr float face_off = 18;                       // the suspect decides this close
    static constexpr float caught = 3.5F;                       // a runner caught this close
    static constexpr float tail_distance = 16;                  // a drunk driver tailed this close...
    static constexpr float tail_seconds = 5;                    // ...with the lights on, this long: he pulls over
    static constexpr float give_up_health = 70;                 // hurt this much: he gives up

    enum class Stage : std::uint8_t { Idle, Waiting, Driving, Running, Fighting, HandsUp, Cuffed,
                                      Ticket,     // a traffic stop: he waits by his car for the ticket
                                      Leaving };  // ticketed: he drives on (out of the game when out of sight)

    struct Frame {
        float dt{0};
        double now{0};
        Vector3 officer{};                 // feet
        float yaw{0};                      // the officer's heading (radians, as the player's)
        bool alive{true};
        std::string police_car;            // the police car the officer drives ("" on foot or another car)
        Vector3 car{};                     // ...where it is
        bool lights{false};                // ...its lights on
    };

    // The suspect: one civilian body, added once a session (out of the game until a call).
    void start(ai::BattleRoyaleBots& bots, Vector3 park, std::uint64_t seed);
    void clear() { *this = RoutineCalls{}; }

    // Puts out a routine call (true). `houses` and `pubs`: CallScenes (Callouts'); `pharmacies`:
    // the clinics' doors.
    bool dispatch(const Frame& frame, const std::vector<CallScene>& scenes, const std::vector<CallScene>& pharmacies, Career& career,
                  ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                  std::uint64_t seed);
    void update(const Frame& frame, Career& career, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                vehicles::VehicleSystem& cars, world::VerdaRegion& region, std::string& said);

    // The suspect with his hands up within cuffing reach (his bot id), or -1; cuffing him.
    [[nodiscard]] int to_cuff(Vector3 feet, ai::BattleRoyaleBots& bots) const;
    bool cuff(ai::BattleRoyaleBots& bots);
    // The suspect hit by an officer's taser: down, and he gives up (true when it took).
    bool stun(ai::BattleRoyaleBots& bots, std::string& said);
    // A traffic stop: the driver within reach, waiting for his ticket (his bot id), or -1; writing it.
    [[nodiscard]] int to_ticket(Vector3 feet, ai::BattleRoyaleBots& bots) const;
    bool ticket(Career& career, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers, vehicles::VehicleSystem& cars, world::VerdaRegion& region);
    // The partner's lesson on the first routine call: the line showing (nullptr: none); the
    // partner wanted beside the officer for it (once).
    [[nodiscard]] const CalloutLine* line() const { return lesson_.empty() ? nullptr : &lesson_.front(); }
    bool take_partner_wanted() { const bool w = partner_wanted_; partner_wanted_ = false; return w; }

    [[nodiscard]] bool active() const { return stage_ != Stage::Idle; }
    [[nodiscard]] Stage stage() const { return stage_; }
    [[nodiscard]] RoutineKind kind() const { return kind_; }
    [[nodiscard]] int bot() const { return bot_; }
    [[nodiscard]] bool suspect(int bot) const { return bot >= 0 && bot == bot_; }
    [[nodiscard]] bool hands_up(int bot) const { return suspect(bot) && (stage_ == Stage::HandsUp || stage_ == Stage::Cuffed); }
    [[nodiscard]] static bool driving_call(RoutineKind kind) {
        return kind == RoutineKind::DrunkDriver || kind == RoutineKind::TrafficStop || kind == RoutineKind::StolenCar;
    }
    [[nodiscard]] bool armed() const { return stage_ == Stage::Fighting; }
    [[nodiscard]] bool walking() const { return walking_; }   // cuffed and walking with the officer
    // The suspect fights the officer (the player) while he's resisting.
    [[nodiscard]] bool hostile(int a, int b) const;
    [[nodiscard]] const std::string& car() const { return car_; }
    [[nodiscard]] const std::string& plate() const { return plate_; }

private:
    void decide(ai::BattleRoyaleBots& bots, std::string& said);
    void hint(const std::string& text);
    void end(ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers, vehicles::VehicleSystem& cars);
    int bot_{-1};
    Stage stage_{Stage::Idle};
    RoutineKind kind_{RoutineKind::Domestic};
    Vector3 scene_{}, home_{};
    std::vector<CalloutLine> lesson_;
    float line_time_{0};
    bool partner_wanted_{false}, first_{false};
    std::string car_, plate_, stale_car_;
    float tailed_{0};
    double next_call_{0};
    std::uint64_t seed_{0}, roll_{0};
    bool walking_{false}, told_lethal_{false};
};

} // namespace outland::game::career
