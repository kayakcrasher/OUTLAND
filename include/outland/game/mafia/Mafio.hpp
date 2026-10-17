#pragma once
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/ai/BotDrivers.hpp"
#include "outland/game/combat/WeaponDefinition.hpp"
#include "outland/game/life/LifeTypes.hpp"
#include "outland/world/Zones.hpp"
#include <raylib.h>
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::game::vehicles { class VehicleSystem; }

// La Mafio: the Krimuloj of Verda (Explore). Five gangs, one for each zono, and a crew of the
// county's gang in every town, holed up in a building they have taken over (a pub, a works, a
// garage or a shop), armed and in plain sight. The police are paid off: nobody calls them about
// the Krimuloj, and the Krimuloj answer for themselves - leave them alone and they leave you
// alone; shoot at one and the whole gang shoots back in "self defence" until it cools off.
//
// Now and then a crew sends a hit squad in a car onto a rival gang's turf and they fight it out.
// The dead come back: a crew member killed respawns at the hideout a while later.
//
// The crews are CombatBots in the shared ai::BattleRoyaleBots (the same brains, bodies, bullets
// and drivers as Battle Royale), with a hostility rule: same gang never, rival gangs only while
// at war, the player only while their gang is provoked. Crews far from the player sleep.
namespace outland::game::mafia {

struct Gang {
    std::string_view name;     // Esperanto
    std::string_view meaning;  // English gloss
    world::zones::ZoneId zone;
    Color color;
    ai::BotLevel level;
    std::array<combat::WeaponId, 3> arms; // handed out in turn
    int crew;                  // members in a town crew (the gang's seat may differ)
};

// Verdkerno's gang, round the capital, is the hardest.
inline constexpr std::array<Gang, world::zones::zone_count> gangs{{
    {"La Verda Mano", "The Green Hand", world::zones::ZoneId::Verdkerno, {46, 204, 96, 255}, ai::BotLevel::Hard,
     {combat::WeaponId::Rifle, combat::WeaponId::Smg, combat::WeaponId::Rifle}, 8},
    {"Montaj Lupoj", "Mountain Wolves", world::zones::ZoneId::Altverdo, {168, 176, 190, 255}, ai::BotLevel::Medium,
     {combat::WeaponId::Rifle, combat::WeaponId::Shotgun, combat::WeaponId::Pistol}, 4},
    {"La Ora Ankro", "The Golden Anchor", world::zones::ZoneId::Lumamaro, {240, 196, 64, 255}, ai::BotLevel::Medium,
     {combat::WeaponId::Smg, combat::WeaponId::Pistol, combat::WeaponId::Rifle}, 4},
    {"Marĉaj Ranoj", "Marsh Frogs", world::zones::ZoneId::Marcfluo, {64, 178, 190, 255}, ai::BotLevel::Easy,
     {combat::WeaponId::Shotgun, combat::WeaponId::Pistol, combat::WeaponId::Smg}, 4},
    {"Feraj Hundoj", "Iron Dogs", world::zones::ZoneId::Ferarbaro, {200, 86, 52, 255}, ai::BotLevel::Medium,
     {combat::WeaponId::Shotgun, combat::WeaponId::Rifle, combat::WeaponId::Pistol}, 4},
}};
int gang_of_zone(world::zones::ZoneId zone);

struct Crew {
    int gang{0};
    int settlement{-1};
    std::string town;          // display name
    std::string building;      // the place they took over
    life::PlaceKind kind{life::PlaceKind::Pub};
    Vector3 door{}, interior{};
    float yaw{0};
    std::vector<int> members;  // bot ids
    bool casino{false};        // the poker table (Verda City only)
    Vector3 table{};
    bool base{false};          // the gang's main base: its county's main town (fixed, the same every game)
    bool disbanded{false};     // broken up by the Policio: nobody comes back to it
};

struct Member {
    int bot{-1}, crew{-1};
    Vector3 post{};
    float post_radius{3}, yaw{0};
    bool dead{false};
    double respawn_at{0};
    bool officer{false};       // the crew's boss and the men at the casino table (dressed sharp)
};

struct War {
    int attackers{-1}, defenders{-1}; // gangs
    int target{-1};                   // crew attacked
    int from{-1};                     // crew that sent the squad
    std::vector<int> squad;
    double until{0};
    bool arrived{false};
};

struct MafioFrame {
    float dt{0};
    double now{0};
    Vector3 player_feet{}, player_velocity{};
    bool player_alive{true}, player_in_vehicle{false};
    bool player_fired{false};
    combat::WeaponId player_weapon{combat::WeaponId::Rifle};
    Vector3 aim_origin{}, aim_direction{0, 0, 1}; // where the player's shots went
};

class Mafio {
public:
    static constexpr float wake_radius = 420;     // crews further than this from the player sleep
    static constexpr float provoked_seconds = 90; // a gang stays hostile this long after a provocation
    static constexpr float respawn_seconds = 75;
    static constexpr float turf_radius = 35;

    // Picks a building in every town (from island life's places) for the county's gang and puts
    // its crew there, in `bots`. The buildings are fixed (the same every game, nearest the middle
    // of town for their kind); each gang's main base is in its county's main town and holds the
    // most men. Returns the number of crews.
    int start(const std::vector<life::Place>& places, const world::VerdaRegion& region, ai::BattleRoyaleBots& bots, std::uint64_t seed);
    void clear(ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers, vehicles::VehicleSystem& cars);
    [[nodiscard]] bool active() const { return !crews_.empty(); }

    void update(const MafioFrame& frame, ai::BattleRoyaleBots& bots, ai::BotWorld world, ai::BotDrivers& drivers,
                vehicles::VehicleSystem& cars, world::VerdaRegion& region);

    // The player hurt a member (their gang turns on the player).
    void provoke(int bot, double now);
    // A whole gang turns on the player (a family they protect was squeezed, their walls tagged).
    void provoke_gang(int gang, double now, const std::string& why);
    // A town changed hands (game::mafia::Territory): the crew there wears its new gang's colours.
    void set_crew_gang(int crew, int gang);
    // The Policio broke this crew up (game::career's campaign): its dead stay dead, it sends no
    // hit squads and nobody attacks it any more.
    void disband(int crew);
    // Members of `crew` still standing.
    [[nodiscard]] int standing(int crew, ai::BattleRoyaleBots& bots) const;
    [[nodiscard]] bool officer(int bot) const;
    // The player died or left: everyone forgets.
    void forgive();
    [[nodiscard]] bool provoked(int gang, double now) const;
    [[nodiscard]] bool hostile(int a, int b, double now) const;
    // Who decides about bots outside the gangs (Paul Acosta): asked whenever one side of a pair
    // is neither the player nor a Krimulo. Unset: they fight nobody.
    void set_outsiders(std::function<bool(int, int, double)> rule) { outsiders_ = std::move(rule); }

    // Sends a hit squad from crew `from` by car to crew `target` (a rival gang's). Returns
    // whether it set off (it needs a usable car near the hideout and a road).
    bool send_hit_squad(int from, int target, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                        vehicles::VehicleSystem& cars, world::VerdaRegion& region, double now);

    [[nodiscard]] int gang_of(int bot) const;   // -1 if not a Krimulo
    [[nodiscard]] int crew_of(int bot) const;
    [[nodiscard]] const std::vector<Crew>& crews() const { return crews_; }
    [[nodiscard]] const std::vector<Member>& members() const { return members_; }
    [[nodiscard]] const std::vector<War>& wars() const { return wars_; }
    [[nodiscard]] const Crew* casino() const;
    // The crew whose hideout `feet` stands at (within `radius` of door or inside), or -1.
    [[nodiscard]] int turf_at(Vector3 feet, float radius = turf_radius) const;
    [[nodiscard]] const std::string& message() const { return message_; }
    [[nodiscard]] float message_time() const { return message_time_; }
    void set_next_war(double at) { next_war_ = at; }
    // How safe Verda has become (the law's share, 0..1): gang wars come that much less often.
    void set_calm(float calm) { calm_ = calm < 0 ? 0 : calm > 1 ? 1 : calm; }
    // The player's own gang (game::career), or -1. Their crews never mind the player's gunfire
    // near them; rival gangs open fire when the player comes onto their turf.
    void set_player_gang(int gang) { player_gang_ = gang; }
    [[nodiscard]] int player_gang() const { return player_gang_; }
    static constexpr float rival_turf_alert = 45;   // metres from a rival hideout
    static constexpr float turf_grace = 5;          // seconds' warning before they shoot

private:
    float calm_{0};
    Member* member(int bot);
    void say(std::string text) { message_ = std::move(text); message_time_ = 6; }
    float random();
    void plan_war(const MafioFrame& frame, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                  vehicles::VehicleSystem& cars, world::VerdaRegion& region);
    void send_home(Member& m, ai::BattleRoyaleBots& bots, const MafioFrame& frame);

    std::vector<Crew> crews_;
    std::vector<Member> members_;
    std::vector<War> wars_;
    std::array<double, world::zones::zone_count> provoked_until_{};
    double next_war_{0};
    std::uint64_t rng_{0x51ed2701f3a5c3b9ULL}, seed_{0};
    std::string message_;
    float message_time_{0};
    std::function<bool(int, int, double)> outsiders_;
    int player_gang_{-1};
    std::array<double, world::zones::zone_count> turf_warned_at_{-1e9, -1e9, -1e9, -1e9, -1e9};
};

} // namespace outland::game::mafia
