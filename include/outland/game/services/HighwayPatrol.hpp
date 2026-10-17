#pragma once
#include <raylib.h>
#include <functional>
#include <vector>

// La Ŝosea Patrolo, Verda's highway patrol: the law on the ring road round the island. Troopers in
// tan shirts and tan campaign hats, tan cruisers with the orange star and "ŜOSEA PATROLO" on the
// doors. They patrol the loop, radio the Policio for backup when they find a wanted man, and ride
// out to any ranch in trouble.
//
// They start with one small outpost on the highway just out of Verda City. As the Policio wins the island
// back (game::mafia::Territory::balance, the law's share), the patrol grows: an outpost more for
// every quarter of the towns cleared, five when Verda is clean.
//
// This file is the plan (where the outposts stand, how many are open, the patrol's loop) and the
// art (the outposts' star sign, the cruisers' door livery); game::services::EmergencyServices drives the cars.
namespace outland::game::services {

struct Outpost {
    Vector3 at{};          // the building's centre on the ground beside the highway
    float yaw{0};          // degrees; its front looks along (sin, cos) - toward the road
    Vector3 parking{};     // where its cruiser waits: under the carport
    float parking_yaw{0};  // degrees, nose toward the road
};

inline constexpr int max_outposts = 5;
// Each outpost is one small modular building from the city kit (tools/generate_city_kit.py): an
// 8 x 6 m block with a door, a counter and a carport on its +X side. Its front is the model's -Z,
// so it stands at rotation yaw + 180; the cruiser parks `outpost_carport` metres to its side.
inline constexpr const char* outpost_model = "assets/verda/city/civic/patrol_outpost.glb";
inline constexpr float outpost_carport = 7;

// Five places beside the highways, off the shoulder: the first on a spoke about half a kilometre
// out of `capital` (Verda City), the other four spread evenly round the ring road, clear of the
// spokes' junctions and anywhere `free` says no (buildings, props, ranches). Ground heights are
// left at 0 (the game sets them).
std::vector<Outpost> plan_outposts(Vector3 capital, const std::function<bool(Vector3)>& free = {});
// How many are open: one at the start, one more for every quarter of the towns the law has
// cleared, all five once Verda is clean.
int outposts_open(float law, bool clean);
// The patrol's beat: points round the ring, `spacing` metres apart, in driving order.
std::vector<Vector3> patrol_loop(float spacing);

// The outposts' roof sign and the cruisers' door livery (needs a GL context; prepare outside drawing).
class HighwayPatrolArt {
public:
    ~HighwayPatrolArt() { unload(); }
    void prepare(const Font& font);
    void unload();
    [[nodiscard]] bool ready() const { return ready_; }
    // The badge: an orange star over "ŜOSEA PATROLO" (doors, signs).
    [[nodiscard]] const Texture2D& badge() const { return badge_.texture; }
    void draw_outpost(const Outpost& post, Vector3 camera) const;      // the roof sign, inside BeginMode3D
    void draw_livery(Vector3 position, float yaw, Vector3 camera) const; // a cruiser's doors and boot

private:
    RenderTexture2D badge_{};
    bool ready_{false};
};

} // namespace outland::game::services
