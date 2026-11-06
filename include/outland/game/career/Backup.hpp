#pragma once
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::game::ai { class BattleRoyaleBots; class BotDrivers; }
namespace outland::game::vehicles { class VehicleSystem; }

// An officer's radio call for backup (Explore, on the force: Z / the BACKUP button).
//
// One squad car with two armed officers comes from the nearest police station - by road, to where
// the player called from - and they get out to fight beside them: they go for the Krimuloj of any
// gang at war with the officer, and stay at the player's side in that area. A player who drives
// off leaves them to hold it (they are never moved to the player); gone from the area a while, or
// after a few minutes (longer while there is shooting), they go back to the station. Dispatch has one squad to spare: after they leave (or fall) it takes a while
// before the next call is answered.
//
// The officers are CombatBots in the shared ai::BattleRoyaleBots (the Mafio's hostility rule asks
// officer() who they fight); off duty they are out of the game and out of sight. The squad car is
// a "police" vehicle the call spawns at the station the first time and reuses after.
//
// The truckers' brotherhood answers the same way on the CB (set_truckers): two Kamionistoj in a
// pickup from the nearest truck stop, to fight beside a member.
namespace outland::game::career {

enum class BackupPhase : std::uint8_t { Idle, Driving, OnScene };

class Backup {
public:
    static constexpr int officers = 2;
    static constexpr double cooldown = 40;       // seconds after they leave before the next call is answered
    static constexpr double stay = 150;          // on the scene this long (from arrival)
    static constexpr float follow_radius = 7;    // they keep about this close to the player
    static constexpr double stuck_seconds = 8;   // a squad car with no headway this long: they get out and run
    static constexpr float area_radius = 80;     // the area they were called to: they follow a player on foot within it
    static constexpr float leave_radius = 250;   // the player this far from it for 20 s: they go back

    // The two officers, added once a session (after the Mafio's crews): out of the game until called.
    void start(ai::BattleRoyaleBots& bots, Vector3 park, std::uint64_t seed);
    void clear() { ids_.clear(); phase_ = BackupPhase::Idle; car_.clear(); ready_at_ = 0; }
    // The brotherhood on the CB instead of the Policio's dispatch: their pickup ("truck"), their words.
    void set_truckers() { truckers_ = true; vehicle_ = "truck"; }
    [[nodiscard]] bool truckers() const { return truckers_; }
    // Reuse a squad car left from an earlier session (a "police" vehicle the call spawned).
    void set_car(std::string id) { car_ = std::move(id); }

    // The call: from the nearest station (`station`, facing `yaw`). Returns what dispatch says.
    std::string call(Vector3 player, double now, Vector3 station, float yaw, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                     vehicles::VehicleSystem& cars, world::VerdaRegion& region);
    // Each frame: arrive, follow the player, and go home when done. Returns a line for the
    // screen when something happens ("" otherwise).
    std::string update(Vector3 player, double now, bool player_alive, bool fighting, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                       vehicles::VehicleSystem& cars, const world::VerdaRegion& region, bool player_driving = false);
    void dismiss(ai::BattleRoyaleBots& bots, double now);

    [[nodiscard]] bool officer(int bot) const;
    [[nodiscard]] bool active() const { return phase_ != BackupPhase::Idle; }
    [[nodiscard]] BackupPhase phase() const { return phase_; }
    [[nodiscard]] const std::vector<int>& ids() const { return ids_; }
    [[nodiscard]] const std::string& car() const { return car_; }
    [[nodiscard]] double ready_at() const { return ready_at_; }

private:
    void on_foot(Vector3 player, Vector3 from, ai::BattleRoyaleBots& bots, const world::VerdaRegion& region);
    std::vector<int> ids_;
    BackupPhase phase_{BackupPhase::Idle};
    std::string car_;
    double ready_at_{0}, until_{0}, last_moved_{0};
    Vector3 last_seen_{}, scene_{};
    double left_at_{0};
    std::uint64_t seed_{0};
    bool truckers_{false};
    std::string vehicle_{"police"};
    [[nodiscard]] std::string line(const char* police, const char* trucker) const { return truckers_ ? trucker : police; }
};

} // namespace outland::game::career
