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
// Honour and notoriety: a gang that kills other gangsters and defends its towns is HONOURABLE.
// A gang member who hurts the people of Verda or its police (reported: assault, murder) keeps his
// honour with the gang - but the crime goes on his record for good, and his NOTORIETY (0-100)
// rises: the island learns his name (and a stint in prison grows with it). The Policio never takes a player with a record, nor one still
// running with a gang (they leave first), and no gang takes a cop (the badge goes back first).
// A crime on duty puts an officer off the force for good.
//
// The police campaign: once serving, the Chief's orders are to rid Verda of the gangs, a town at
// a time - every crew broken up for good. He will not have Verda City touched until every other
// town is clean; then the capital, and Verda is free.
//
// The other road's summit: a gang member who reaches Leŭtenanto may get the boss's order to kill
// the Chief himself - late in the game, once the gang runs at least three quarters of the island. Do it and the gang takes the whole island (and 500 000 kronoj); the Policio
// is left without a head and takes no more recruits.
//
// This class is the rules and the bookkeeping only; the game tells it what happened (kills,
// crimes, where the player is and what they drive) and what the world holds (crews, rigs,
// towns), and reads back the job, the respect and the pay.
namespace outland::game::career {

enum class Side : std::uint8_t { Civilian, Gang, Police };
enum class JobKind : std::uint8_t { None, KillRivals, RobRig, DriveBy, Patrol, Raid, BreakUpWar, Quest, Smuggle, Extort, TakeTurf, CleanUp, HitChief, Callout,
                                     RobStore, TagUp };
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
    bool capital{false};         // Verda City's (the Chief leaves it for last)
    bool cleared{false};         // broken up by the Policio for good
};
struct RigSpot { std::string vehicle; Vector3 position{}; std::string name; };
struct WarSpot { int attackers{-1}, defenders{-1}; Vector3 where{}; std::string town; };
struct TownSpot { std::string name; Vector3 centre{}; };            // patrol stops (towns, not ranches)
struct DockSpot { std::string name; Vector3 point{}; };            // where contraband comes ashore
struct RanchSpot { int ranch{-1}; std::string name, family; Vector3 gate{}; bool yours{false}; };
struct ShopSpot { std::string name, town; Vector3 till{}; };          // a shop that can be robbed (its counter)
struct TurfSpot { int turf{-1}; std::string name; Vector3 centre{}; int owner{-1}; float hold{100}; bool cleared{false}; };

// What the world holds this frame, and what the player is doing.
struct CareerFrame {
    double now{0};
    Vector3 player{};              // feet
    bool alive{true};
    std::string driving;           // vehicle id at the player's wheel ("" on foot)
    bool driving_police{false};    // ...and it is a police car (any: a patrol car, the Interceptor, a cruiser)
    std::vector<CrewSpot> crews;
    std::vector<RigSpot> rigs;
    std::vector<WarSpot> wars;
    std::vector<TownSpot> towns;
    std::vector<std::string> gang_names;   // by gang index
    // The police questline's places: the Chief at Verda City HQ and the training camp.
    Vector3 chief{}, camp{};
    bool chief_known{false}, camp_known{false};
    float crew_share{0};           // the share of the island's towns the player's gang runs (0..1)
    // The underworld's map: docks, ranches, towns (turf) and who holds them.
    std::vector<DockSpot> docks;
    std::vector<RanchSpot> ranches;
    std::vector<TurfSpot> turfs;
    std::vector<ShopSpot> shops;
};

struct Job {
    JobKind kind{JobKind::None};
    std::string title, detail;     // "KILL RIVALS", "Take out 3 Montaj Lupoj at Roka"
    Vector3 target{};              // the waypoint
    int gang{-1}, crew{-1};        // whose people (KillRivals, DriveBy, Raid, BreakUpWar)
    int needed{0}, done{0};
    std::string vehicle;           // the rig to rob
    bool taken{false};             // RobRig: the rig is in the player's hands; Smuggle: the load; Extort: the demand made; RobStore: the till
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
    bool record{false};            // a crime on file: the Policio will not have them
    bool honourable{true};         // the gang's honour (crimes against the people no longer take it)
    float notoriety{0};            // 0-100: how much the island fears the name (crimes, the big hit)
    int defended{0};               // raiders taken down on the gang's own turf
    bool chief_dead{false};        // the Chief was hit (for good): the Policio is leaderless
    int jobs_done{0};
    std::string patrol_car;        // the officer's car (vehicle id)
    PoliceStage police_stage{PoliceStage::Report};
    int callouts{0};               // crime calls answered (the first comes with a partner and a lesson)
    bool prison_toured{false};     // the warden showed the officer how the prison works
    int inmate{0};                 // the player's prison number, once booked (0: never)
    bool inside_told{false};       // a guard told the new inmate how it works inside
};

class Career {
public:
    static constexpr float respect_max = 100;
    static constexpr double job_gap = 12;          // seconds between one job and the next offer
    static constexpr float arrive_radius = 25;     // a delivery, reached
    static constexpr float patrol_radius = 40;     // a town on a patrol, shown the flag
    static constexpr float drop_radius = 22;       // a robbed rig delivered at the hideout

    // Joining. False (with why) when the door is shut.
    bool join_gang(int gang, const std::string& gang_name, std::string& why);
    bool join_police(const std::string& patrol_car, std::string& why);
    // Walk away (the record stays).
    void quit();

    // Something the player did.
    void killed(int gang, bool from_vehicle);        // a Krimulo of `gang` (-1: not a Krimulo)
    // An officer cuffed a Krimulo of `gang` (USE): counts as a takedown for the call and goes in
    // the back of the car - in custody until delivered to the state prison. False (and nothing
    // happens) when the car is full.
    bool arrested(int gang);
    static constexpr int custody_max = 4;
    static constexpr int booking_pay = 120;        // kronoj a prisoner, at the prison
    [[nodiscard]] int custody() const { return custody_; }
    // At the prison's intake in a police car: everyone in custody booked in. Returns how many.
    int deliver();
    // A crime in progress (game::mafia's heists), the call put out by the game: a serving officer
    // with nothing on takes it. `needed` suspects to stop (cuffed and in the car, or down).
    bool callout(Job call);
    // A serving officer with nothing on, due the next call (the game may put out a callout instead).
    [[nodiscard]] bool due_call(double now) const {
        return serving() && !jailed_ && job_.kind == JobKind::None && next_offer_ >= 0 && now >= next_offer_;
    }
    // A suspect stopped: cuffed and in the car (`alive`), or down.
    void suspect_down(bool alive);
    void callout_failed(const std::string& why);
    // The call's waypoint moves with the suspects.
    void track(Vector3 target) { if (job_.kind == JobKind::Callout) job_.target = target; }
    // The officer's first look round the state prison (the warden's tour), once.
    void prison_toured() { state_.prison_toured = true; }
    // Inside: a guard has told the new inmate the ropes (once).
    void inside_told() { state_.inside_told = true; }
    // Inside: the player shanked a snitch for the gang (off to the SHU).
    void shanked_snitch();
    // The player's prison number (given at the first booking, kept for good).
    [[nodiscard]] int inmate_number() const { return state_.inmate; }
    // The player was arrested (one star, caught): off to the state prison. The job in hand is off;
    // a gang member who does his time quietly earns a little respect for it. Returns the sentence
    // (seconds).
    float jailed();
    void released();
    [[nodiscard]] bool in_prison() const { return jailed_; }
    // The crew a CLEAN UP is breaking up (-1: none): its dead stay down until the job ends.
    [[nodiscard]] int besieging() const { return state_.side == Side::Police && job_.kind == JobKind::CleanUp ? job_.crew : -1; }
    void crime(bool serious);
    static constexpr float notoriety_max = 100, notoriety_serious = 8, notoriety_minor = 3, notoriety_hit = 30;                        // a crime someone reported (assault, murder, attacking police)
    // An officer in a gunfight with a gang hit a bystander: crossfire, no crime. Only a trail of
    // dead (`stray_limit` killed before the next job is done) is murder, and costs the badge.
    void stray_round(bool killed);
    static constexpr int stray_limit = 3;
    [[nodiscard]] int strays() const { return strays_; }
    // The Chief went down on a HitChief job.
    void chief_killed();
    static constexpr int hit_pay = 500000;              // kronoj, flat (no rank bonus)
    static constexpr float hit_rank_respect = 70;       // Leŭtenanto and up get the order...
    static constexpr float hit_island_share = .75F;     // ...once the gang runs three quarters of Verda
    // A rival gangster taken down on the player's own gang's turf (defending the town).
    void defended(const std::string& town);
    // An officer shot at Verda City's crew before the Chief allowed it.
    void warned_off_capital(double now);
    // The police campaign: crews still standing outside Verda City, and in it.
    [[nodiscard]] static int towns_left(const CareerFrame& frame);
    [[nodiscard]] static bool capital_allowed(const CareerFrame& frame) { return towns_left(frame) == 0; }
    [[nodiscard]] bool verda_clean() const { return verda_clean_; }
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
    // STORE ROBBERY: the player, gun out at the job's counter, took the till (once): the alarm goes
    // off (take_tipoff) and the takings go home to the hideout. False when there's no robbery on.
    static constexpr float till_reach = 4.5F;      // USE this close to the counter
    bool robbed();
    // TAG UP: a tag sprayed on a wall in `turf` (Territory's index). The job wants `needed` of them
    // in its rival town; the boss hands over the cans with the job (take_cans).
    void tagged(int turf);
    int take_cans() { const int c = cans_; cans_ = 0; return c; }
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
    // Which jobs come up: different every session.
    void seed(std::uint64_t value) {
        std::uint64_t z = value + 0x9e3779b97f4a7c15ULL;   // splitmix64: nearby seeds, unrelated runs
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        rng_ = (z ^ (z >> 31)) | 1;
    }

private:
    void offer(const CareerFrame& frame);
    [[nodiscard]] const CrewSpot* home_of(const CareerFrame& frame) const;   // the player's gang's nearest hideout still standing
    void finish(bool success, const std::string& why);
    void gain(float amount);
    void say(std::string text) { message_ = std::move(text); message_time_ = 5; }
    float random();

    CareerState state_;
    Job job_;
    double next_offer_{0};
    int pay_{0};
    int custody_{0};
    int strays_{0};                  // bystanders killed in crossfire since the last job done
    bool jailed_{false};             // doing time: no work till released
    bool tipoff_{false}, demanded_{false};
    int cans_{0};                    // spray cans the boss hands over with a TAG UP
    bool verda_clean_{false};
    double capital_warned_{-1e9};
    int campaign_left_{-1};
    Job finished_;
    std::string message_;
    float message_time_{0};
    std::uint64_t rng_{0x2545f4914f6cdd1dULL};
};

} // namespace outland::game::career
