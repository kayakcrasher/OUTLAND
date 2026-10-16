#pragma once
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

// A life in Verda's underworld or on its police force (Explore).
//
// The player can walk into a gang's hideout and join it, or into a police station and sign up.
// Either way they earn RESPECT (0-100) doing the work - a gang sends them to kill rivals, rob
// the big rigs and go on drive-bys; the Policio gives them a patrol car and radio calls (patrols,
// raids on the Krimuloj, breaking up gang wars). Respect raises their rank and their pay; failing
// a job, dying or betraying their own costs it.
//
// The one-way door: anyone who has joined a gang has a record, and the Policio never takes a
// player with a record. An officer can always go bad - join a gang, or commit a crime on duty,
// and they are off the force for good.
//
// This class is the rules and the bookkeeping only; the game tells it what happened (kills,
// crimes, where the player is and what they drive) and what the world holds (crews, rigs,
// towns), and reads back the job, the respect and the pay.
namespace outland::game::career {

enum class Side : std::uint8_t { Civilian, Gang, Police };
enum class JobKind : std::uint8_t { None, KillRivals, RobRig, DriveBy, Patrol, Raid, BreakUpWar, Quest, Smuggle, Extort, TakeTurf };
// A recruit's road onto the force: report to the Chief at Verda City HQ, qualify at the training
// camp's range, report back for the badge and the Interceptor, then serve (calls).
enum class PoliceStage : std::uint8_t { Report, Training, Graduated, Serving };
inline constexpr const char* chief_name = "Ĉefo Marko Forta";
const char* job_name(JobKind kind);

struct CrewSpot {                // a gang crew's hideout
    int crew{-1}, gang{-1};
    Vector3 door{};
    std::string town;
    int alive{0};                // members standing
};
struct RigSpot { std::string vehicle; Vector3 position{}; std::string name; };
struct WarSpot { int attackers{-1}, defenders{-1}; Vector3 where{}; std::string town; };
struct TownSpot { std::string name; Vector3 centre{}; };
struct DockSpot { std::string name; Vector3 point{}; };            // where contraband comes ashore
struct RanchSpot { int ranch{-1}; std::string name, family; Vector3 gate{}; bool yours{false}; };
struct TurfSpot { int turf{-1}; std::string name; Vector3 centre{}; int owner{-1}; float hold{100}; };

// What the world holds this frame, and what the player is doing.
struct CareerFrame {
    double now{0};
    Vector3 player{};              // feet
    bool alive{true};
    std::string driving;           // vehicle id at the player's wheel ("" on foot)
    std::vector<CrewSpot> crews;
    std::vector<RigSpot> rigs;
    std::vector<WarSpot> wars;
    std::vector<TownSpot> towns;
    std::vector<std::string> gang_names;   // by gang index
    // The police questline's places: the Chief at Verda City HQ and the training camp.
    Vector3 chief{}, camp{};
    bool chief_known{false}, camp_known{false};
    // The underworld's map: docks, ranches, towns (turf) and who holds them.
    std::vector<DockSpot> docks;
    std::vector<RanchSpot> ranches;
    std::vector<TurfSpot> turfs;
};

struct Job {
    JobKind kind{JobKind::None};
    std::string title, detail;     // "KILL RIVALS", "Take out 3 Montaj Lupoj at Roka"
    Vector3 target{};              // the waypoint
    int gang{-1}, crew{-1};        // whose people (KillRivals, DriveBy, Raid, BreakUpWar)
    int needed{0}, done{0};
    std::string vehicle;           // the rig to rob
    bool taken{false};             // RobRig: the rig is in the player's hands; Smuggle: the load; Extort: the demand made
    int ranch{-1};                 // Extort: the family's ranch
    int turf{-1};                  // TakeTurf: the town
    double deadline{0};
    int pay{0};
    float respect{0};
};

struct CareerState {
    Side side{Side::Civilian};
    int gang{-1};
    float respect{0};
    bool record{false};            // ever a Krimulo: the Policio will not have them
    int jobs_done{0};
    std::string patrol_car;        // the officer's car (vehicle id)
    PoliceStage police_stage{PoliceStage::Report};
};

class Career {
public:
    static constexpr float respect_max = 100;
    static constexpr double job_gap = 12;          // seconds between one job and the next offer
    static constexpr float arrive_radius = 25;     // a patrol point or a delivery, reached
    static constexpr float drop_radius = 22;       // a robbed rig delivered at the hideout

    // Joining. False (with why) when the door is shut.
    bool join_gang(int gang, const std::string& gang_name, std::string& why);
    bool join_police(const std::string& patrol_car, std::string& why);
    // Walk away (the record stays).
    void quit();

    // Something the player did.
    void killed(int gang, bool from_vehicle);        // a Krimulo of `gang` (-1: not a Krimulo)
    void crime(bool serious);                        // a crime someone reported (assault, murder, attacking police)
    void died();
    // The police questline. Talking to the Chief moves a recruit on (and says what he says);
    // `issue_car` comes back true when he hands over the Interceptor (the game spawns it and
    // calls issue_car with its id).
    std::string chief_talk(bool& issue_car);
    void issue_car(const std::string& vehicle) { state_.patrol_car = vehicle; }
    // The camp's range: every drill qualified.
    void passed_training();
    // The underworld (Renderer and the territory): the load is aboard and the police got a tip
    // (once); the demand was made at a ranch (once) - pay up or fight; a turf changed hands.
    bool take_tipoff() { const bool t = tipoff_; tipoff_ = false; return t; }
    bool take_extort_arrival() { const bool a = demanded_; demanded_ = false; return a; }
    void extort_result(bool paid, int family_faction);
    void turf_taken(int turf, const std::string& name);
    // The last job finished well (kind, where, ranch), once - the territory pays out on it.
    bool take_finished(Job& out) { if (finished_.kind == JobKind::None) return false; out = finished_; finished_ = {}; return true; }
    [[nodiscard]] PoliceStage police_stage() const { return state_.police_stage; }
    [[nodiscard]] bool serving() const { return on_duty() && state_.police_stage == PoliceStage::Serving; }

    void update(const CareerFrame& frame);

    [[nodiscard]] const CareerState& state() const { return state_; }
    [[nodiscard]] const Job& job() const { return job_; }
    [[nodiscard]] bool on_duty() const { return state_.side == Side::Police; }
    [[nodiscard]] bool in_gang(int gang) const { return state_.side == Side::Gang && state_.gang == gang; }
    [[nodiscard]] int rank() const { return rank_of(state_.respect); }
    [[nodiscard]] const char* rank_name() const;
    [[nodiscard]] static int rank_of(float respect);
    // Pay for a job scales with rank: +25% a rank.
    [[nodiscard]] float pay_scale() const { return 1.0F + .25F * static_cast<float>(rank()); }
    // Money earned since last asked (paid into the player's wallet).
    int take_pay() { const int p = pay_; pay_ = 0; return p; }
    // A line for the screen (a new job, a job done, a promotion, the sack).
    [[nodiscard]] const std::string& message() const { return message_; }
    [[nodiscard]] float message_time() const { return message_time_; }
    void tick_message(float dt) { message_time_ = message_time_ > dt ? message_time_ - dt : 0; }

    // Persistence (Explore: beside the player's loot profile).
    [[nodiscard]] std::string serialise() const;
    bool parse(const std::string& text);
    void restore(const CareerState& state) { state_ = state; job_ = {}; }

private:
    void offer(const CareerFrame& frame);
    void finish(bool success, const std::string& why);
    void gain(float amount);
    void say(std::string text) { message_ = std::move(text); message_time_ = 5; }
    float random();

    CareerState state_;
    Job job_;
    double next_offer_{0};
    int pay_{0};
    bool tipoff_{false}, demanded_{false};
    Job finished_;
    std::string message_;
    float message_time_{0};
    std::uint64_t rng_{0x2545f4914f6cdd1dULL};
};

} // namespace outland::game::career
