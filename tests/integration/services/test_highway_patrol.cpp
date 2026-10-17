// The Ŝosea Patrolo's plan: five outposts on the ring road (the first nearest Verda City), how
// many are open as the law wins the island back, and the beat round the ring.
#include "outland/game/services/HighwayPatrol.hpp"
#include "outland/world/Highways.hpp"
#include <cmath>
#include <iostream>
using namespace outland::game::services;
namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
}

int main() {
    const Vector3 capital{0, 0, 0};
    const auto posts = plan_outposts(capital);
    check(posts.size() == static_cast<std::size_t>(max_outposts), "five outposts planned");
    const outland::world::highways::Line* ring = nullptr;
    for (const auto& line : outland::world::highways::lines()) if (line.loop) ring = &line;
    check(ring != nullptr, "the ring road");
    if (ring && posts.size() == 5) {
        const auto off_road = [&](Vector3 p) {
            float nearest = 1e9F;
            for (const auto& line : outland::world::highways::lines())
                for (const auto& q : line.points) nearest = std::min(nearest, flat(p, q));
            return nearest;
        };
        bool beside = true, apart = true;
        for (std::size_t i = 0; i < posts.size(); ++i) {
            const float d = off_road(posts[i].at);
            beside = beside && d > outland::world::highways::paved_half + 4 && d < 40;
            for (std::size_t j = 0; j < i; ++j) apart = apart && flat(posts[i].at, posts[j].at) > 600;
        }
        check(beside, "each beside the ring road, off its shoulder");
        check(apart, "spread round the loop");
        bool first = true;
        for (std::size_t i = 1; i < posts.size(); ++i) first = first && flat(posts[0].at, capital) <= flat(posts[i].at, capital);
        check(first, "the first outpost is the one by Verda City");
        // The booth faces the road.
        const float a = posts[0].yaw * DEG2RAD;
        const Vector3 front{posts[0].at.x + std::sin(a) * 12, 0, posts[0].at.z + std::cos(a) * 12};
        check(off_road(front) < off_road(posts[0].at), "the outpost looks out on the road");
        // The cruiser waits under the carport, beside the building, nose to the road.
        bool carport = true;
        for (const auto& post : posts)
            carport = carport && std::abs(flat(post.parking, post.at) - outpost_carport) < .01F &&
                      std::abs(off_road(post.parking) - off_road(post.at)) < 3 && post.parking_yaw == post.yaw;
        check(carport, "the cruiser parks under the carport");
        // Kept off ground the game marks as taken (a farm, say).
        const Vector3 taken = posts[0].at;
        const auto moved = plan_outposts(capital, [&](Vector3 p) { return flat(p, taken) > 150; });
        bool away = !moved.empty();
        for (const auto& post : moved) away = away && flat(post.at, taken) > 150 && flat(post.parking, taken) > 150;
        check(away, "outposts stay off taken ground");
    }
    check(outposts_open(0, false) == 1 && outposts_open(.25F, false) == 2 && outposts_open(.5F, false) == 3 &&
          outposts_open(.74F, false) == 3 && outposts_open(.75F, false) == 4 && outposts_open(1, false) == 5 &&
          outposts_open(.3F, true) == 5, "one outpost to start, a fifth when Verda is clean");
    const auto beat = patrol_loop(250);
    check(beat.size() > 10, "a beat round the ring");
    bool steps = true;
    for (std::size_t i = 1; i < beat.size(); ++i) steps = steps && flat(beat[i], beat[i - 1]) > 200 && flat(beat[i], beat[i - 1]) < 400;
    check(steps, "points a few hundred metres apart");
    if (failures) return 1;
    std::cout << "[PASS] Ŝosea Patrolo: outposts on the ring, growing with the law, the beat\n";
    return 0;
}
