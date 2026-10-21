#pragma once
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

// Driving for Verda Transporto (Explore): the trucking company at its depot in Verda City's
// industrial quarter. Anyone but a serving police officer can sign on with the boss at the depot
// door (USE, then USE again). He gives out one load at a time:
//   IMPORT       a container off the ship at Porto Luma's cargo terminal, to a town;
//   EXPORT       goods from the depot to the cargo terminal, for the ship;
//   DELIVERY     goods from the depot to a town;
//   CATTLE HAUL  a ranch's cattle (the family brings them to the corral) to the cattle quay at the
//                cargo terminal, where they are shipped and the ranch is paid.
// Take one of the company's rigs from the yard (a box-trailer rig, or the stock-trailer rig for
// cattle), pull up at the pickup and stop: loading takes a few seconds; then the drop; then back to
// the boss, who pays for the trip and has the next load ready.
//
// The rules and the bookkeeping only: the game says where the player is and what they drive, and
// does what the events ask (call cattle to the corral, load them, ship them).
namespace outland::game::freight {

enum class HaulKind : std::uint8_t { None, Import, Export, Delivery, Cattle };
enum class HaulStage : std::uint8_t { None, Pickup, Deliver, Report };
const char* haul_name(HaulKind kind);

struct HaulPlace { std::string name; Vector3 point{}; int ranch{-1}; };

// Where loads come from and go (on the road, where a rig pulls up).
struct HaulWorld {
    Vector3 depot{};                  // the yard road at the depot's door
    Vector3 cargo_quay{};             // the quay lane under the cranes
    Vector3 cattle_quay{};            // the quay lane beside the cattle chute
    bool terminal{false};             // the cargo terminal is there
    std::vector<HaulPlace> towns;     // drops
    std::vector<HaulPlace> ranches;   // cattle pickups (their chutes), ranch = index
};

struct Haul {
    HaulKind kind{HaulKind::None};
    HaulStage stage{HaulStage::None};
    std::string cargo;                // "machine parts", "4 head of cattle"
    std::string from_name, to_name;
    Vector3 from{}, to{};
    int ranch{-1};                    // a cattle haul's ranch
    int head{0};                      // cattle aboard
    int pay{0};
    float work{0};                    // seconds of loading / unloading done
};

struct TruckFrame {
    float dt{0};
    double now{0};
    Vector3 player{};                 // feet
    std::string driving;              // vehicle id at the player's wheel ("" on foot)
    float speed{0};                   // its speed, m/s
};

enum class TruckEvent : std::uint8_t { None, CallCattle, LoadCattle, ShipCattle, Loaded, Delivered };

class Trucking {
public:
    static constexpr float stop_radius = 16;     // pulled up at a pickup or drop
    static constexpr float still = 1.2F;         // ...and stopped (m/s)
    static constexpr float work_seconds = 6;     // loading or unloading
    static constexpr int base_pay = 180, head_pay = 60;
    static constexpr float pay_per_km = 120;
    static constexpr double confirm_seconds = 5;

    // Talking to the boss at the depot door. `cop`: a serving officer (he won't hire one).
    // Returns what he says.
    std::string talk(bool cop, double now, const HaulWorld& world);
    // Each frame; what the game must do about it (CallCattle / LoadCattle / ShipCattle: then report
    // back with cattle_loaded / nothing).
    TruckEvent update(const TruckFrame& frame);
    void cattle_loaded(int head);
    // A rig of the company's that can carry this load (box trailer, or the stock trailer for cattle).
    [[nodiscard]] static bool company_rig(const std::string& vehicle) { return vehicle.find("semi_depot") != std::string::npos; }
    [[nodiscard]] static bool stock_rig(const std::string& vehicle) { return company_rig(vehicle) && vehicle.find("cattle") != std::string::npos; }
    [[nodiscard]] bool right_rig(const std::string& vehicle) const {
        return company_rig(vehicle) && (job_.kind == HaulKind::Cattle) == stock_rig(vehicle);
    }

    [[nodiscard]] bool hired() const { return hired_; }
    [[nodiscard]] const Haul& job() const { return job_; }
    [[nodiscard]] Vector3 target() const;          // where to go now (the depot when reporting)
    [[nodiscard]] std::string title() const;       // "IMPORT", "CATTLE HAUL"...
    [[nodiscard]] std::string detail() const;      // what to do now
    [[nodiscard]] int trips() const { return trips_; }
    [[nodiscard]] std::int64_t earned() const { return earned_; }
    int take_pay() { const int p = pay_; pay_ = 0; return p; }
    [[nodiscard]] const std::string& message() const { return message_; }
    [[nodiscard]] float message_time() const { return message_time_; }
    void quit() { hired_ = false; job_ = {}; }

    void seed(std::uint64_t value) { rng_ = (value ^ 0x51ed270b2a1f7c3dULL) | 1; }
    [[nodiscard]] std::string serialise() const;
    bool parse(const std::string& text);

private:
    Haul next_job(const HaulWorld& world);
    float random();
    void say(std::string text, float seconds = 4) { message_ = std::move(text); message_time_ = seconds; }

    bool hired_{false};
    int trips_{0};
    std::int64_t earned_{0};
    Haul job_;
    Vector3 depot_{};
    int pay_{0};
    double offer_until_{-1};
    bool offer_quit_{false};
    std::uint64_t rng_{0x2545f4914f6cdd1dULL};
    std::string message_;
    float message_time_{0};
    float nag_{0};                    // seconds before the next "wrong rig" reminder
    int cattle_tries_{0};             // waiting at the corral for the family
    bool call_pending_{false};        // the family to be told to bring cattle to the corral
};

} // namespace outland::game::freight
