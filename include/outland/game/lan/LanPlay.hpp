#pragma once
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/net/LanProtocol.hpp"
#include <raylib.h>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::characters { class NpcSystem; class CharacterRegistry; }
namespace outland::game::vehicles { class VehicleSystem; }

// The other people in a LAN match, as this game sees them: each a body in NpcSystem (drawn,
// animated and hit like a bot), smoothed between the states they send, at the wheel of whichever
// car they drive. Also the host's side of the bots: snapshots of every bot and moving car for the
// clients, who mirror them instead of running their own.
namespace outland::game::lan {

struct Remote {
    std::uint8_t slot{0};
    std::string name;
    net::PlayerState state;       // the latest
    Vector3 shown{};              // smoothed feet
    float shown_yaw{0};
    double heard{0};
    bool known{false};
    int car{-1};                  // the car they are gliding (released when they stop driving)
    int fired{0};                 // shots heard since last asked (for the gunfire sound)
    std::uint8_t last_flags{0};
};

class LanPlay {
public:
    static constexpr int body_base = 900000;   // remote players' NpcSystem bot ids: body_base + slot
    static constexpr float state_rate = 20, snapshot_rate = 10;

    void clear(characters::NpcSystem* npcs = nullptr);
    void received(std::uint8_t slot, const std::string& name, const net::PlayerState& state, double now);
    void remove(std::uint8_t slot, characters::NpcSystem& npcs, vehicles::VehicleSystem& cars);
    // Smooth toward the latest states; bodies and cars follow.
    void update(float dt, characters::NpcSystem& npcs, const characters::CharacterRegistry* registry,
                vehicles::VehicleSystem& cars, world::VerdaRegion& region);

    [[nodiscard]] static bool is_remote_body(int bot) { return bot >= body_base && bot < body_base + net::max_players; }
    [[nodiscard]] static std::uint8_t slot_of(int bot) { return static_cast<std::uint8_t>(bot - body_base); }
    [[nodiscard]] const std::vector<Remote>& remotes() const { return remotes_; }
    [[nodiscard]] Remote* find(std::uint8_t slot);
    [[nodiscard]] int alive() const;
    // Where each person who fired since last asked is, and with what (their gunfire, heard here).
    std::vector<std::pair<Vector3, std::uint8_t>> take_shots();
    [[nodiscard]] std::vector<ai::OtherPlayer> others() const;
    // Names over the heads of the people in view. Those `friendly` says are on the player's side
    // show green, with how far, from any distance (through walls too).
    void draw_names(const Camera3D& camera, int width, int height, const std::function<bool(std::uint8_t)>& friendly = {}) const;

    // Host: every bot and every car that moved since the last snapshot.
    net::BotSnapshot snapshot(const ai::BattleRoyaleBots& bots, const vehicles::VehicleSystem& cars, float elapsed);
    // Client: the bots and cars as the host has them.
    void apply(const net::BotSnapshot& snapshot, ai::BattleRoyaleBots& bots, vehicles::VehicleSystem& cars,
               world::VerdaRegion& region, float dt);

private:
    std::vector<Remote> remotes_;
    std::vector<Vector3> last_cars_;          // host: where each car was at the last snapshot
    std::vector<int> mirrored_cars_;          // client: cars moved by snapshots
};

}
