#pragma once
#include <raylib.h>
#include <string>
#include <vector>

// Sovaĝejo's backcountry: the county round the trailer park, its homesteads and Sovaĝa Monto's
// road (world/MountainRoad.hpp), which stay as they are.
//
// A few short, rough dirt tracks branch off that road network to scattered country homes - a
// roadside trailer, a hunting cabin, a family compound, farmhouses by the creek and toward the
// west coast, a fisher's trailer, a workshop family south of the homesteads - and to Fajrejo, a
// tiny settlement east of the trailer park: a gravel lot with a church, a general store and a
// mechanic's shed round it, and four houses. Everyone there is Sovaĝejo's.
//
// Each track's profile follows the land, evened out, never steeper than max_grade, and meets the
// road it leaves at that road's height; the land is cut and filled to it (level across the bed)
// and to a level pad under each home and the village. The mountain road's own bed is left alone.
namespace outland::world::county {

inline constexpr float track_width = 5;         // a rough dirt track
inline constexpr float bed_half = 3.4F;         // its level bed: the track and a verge each side
inline constexpr float max_grade = .12F;
inline constexpr float step = 2;                // metres between a track's samples
inline constexpr float lot_width = 16;          // Fajrejo's gravel lot

// What a track leaves from.
enum class From : unsigned char { Mountain, Homesteads, Village, Track };
struct Track {
    std::string name;
    From from{From::Mountain};
    int parent{-1};                 // From::Track: the track it branches from
    std::vector<Vector3> points;    // every `step` metres; y: the bed's height
    std::vector<float> s;           // metres along
    [[nodiscard]] float length() const { return s.empty() ? 0 : s.back(); }
    [[nodiscard]] Vector3 at(float metres) const;
    [[nodiscard]] Vector3 heading(float metres) const;   // unit, level
};
const std::vector<Track>& tracks();

// A country home and its yard on a level pad. `front` is the way the house faces (toward its track).
enum class Home : unsigned char { RoadsideTrailer, HuntingCabin, Compound, CreekFarm, CreekTrailer, WestFarm, FisherTrailer, Workshop };
struct Site {
    Home kind{Home::RoadsideTrailer};
    std::string name;               // the family's (on its mailbox)
    Vector3 centre{};               // y: the pad's level
    Vector3 front{};                // unit
    float flat{10}, blend{14};
    int dwellings{1};
    [[nodiscard]] Vector3 at(float right, float forward) const;   // from the centre, on the pad
};
const std::vector<Site>& sites();

// Fajrejo: its lot runs from the west node (where the east road comes in) to the east node.
struct Village {
    Vector3 centre{};               // y: the pad's level
    Vector3 west{}, east{};         // the lot's ends (y: the level)
    Vector3 along{}, across{};      // unit: west -> east, and from the lot toward the store (the south side)
    float flat{46}, blend{26};
    [[nodiscard]] Vector3 at(float a, float b) const;   // `a` along the lot from the centre, `b` across it
};
const Village& village();

// The land at (x, z) cut and filled to the tracks and pads, given its height there before
// (already shaped to the mountain road).
float graded(float x, float z, float height);

// Within `margin` of a track, a home's pad or the village (the county's own ground).
bool near(float x, float z, float margin);

} // namespace outland::world::county
