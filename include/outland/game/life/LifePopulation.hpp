#pragma once
#include "outland/game/life/LifeTypes.hpp"
#include <cstdint>

namespace outland::world { class VerdaRegion; struct Building; }
namespace outland::characters { class CharacterRegistry; }

namespace outland::game::life {
struct Island {
    std::vector<Place> places;
    std::vector<Resident> residents;
    std::vector<std::string> settlement_names;
    std::vector<Vector3> settlement_centers;
    std::vector<Car> cars;
};
// A parked car goes to a household with its door within this (metres).
inline constexpr float car_claim_radius = 95.0F;
struct PopulationOptions {
    std::uint64_t seed{0x5e7da11f};
    float density{1};   // multiplies home capacity; >1 packs more people per home
};
// Deterministic: the same region, seed and density always produce the same people.
// Every car on the map (vehicle spawn markers and placed vehicles) belongs to the nearest
// household within car_claim_radius that has fewer than two, and is driven by a member without one.
// Player-built homes (building ids starting "player_") stand empty until newcomers move in.
// Buildings named with a purpose ("church", "pub", "police", "clinic", "hall", "dock",
// "garage", "shop", "home") keep it; others are designated by style and town identity.
Island build_island(const world::VerdaRegion& region,const characters::CharacterRegistry* registry,
    PopulationOptions options={});
// Newcomers: a family of `members` moves into `home` (a player-built house in town `settlement`).
// Adds the home as a place and the family as residents with jobs, haunts and day plans like
// everyone else's. Returns the first new resident's id. Deterministic for the same seed.
int add_household(Island& island,const world::Building& home,int settlement,int members,
    const characters::CharacterRegistry* registry,std::uint64_t seed);
// Day plan lookup; minute is 0..1439.
Activity planned_activity(const Resident& resident,Weekday day,int minute);
// Outside the doorway of a building, in world space.
Vector3 door_position(Vector3 center,Vector3 size,float yaw_degrees,float outside,float local_x=0);
bool inside_footprint(const Place& place,Vector3 position,float margin=.3F);
}
