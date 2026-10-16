#include "outland/game/combat/CombatFeedback.hpp"
#include "outland/game/combat/Grenades.hpp"
#include <raymath.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace outland::game::combat;
namespace {
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
// A flat floor at y = 0 and a wall at z = 20.
GrenadeWorld box_world() {
    GrenadeWorld world;
    world.collide = [](Vector3 a, Vector3 b, Vector3& point, Vector3& normal) {
        if (a.y >= 0 && b.y < 0) {
            const float t = a.y / (a.y - b.y);
            point = Vector3Lerp(a, b, t); normal = {0, 1, 0}; return true;
        }
        if (a.z <= 20 && b.z > 20) {
            const float t = (20 - a.z) / (b.z - a.z);
            point = Vector3Lerp(a, b, t); normal = {0, 0, -1}; return true;
        }
        return false;
    };
    return world;
}
}
int main() {
    // Blast falloff: lethal in close, nothing at the edge, monotonic between.
    check(Grenades::damage_at(0) == Grenades::max_damage && Grenades::damage_at(2) == Grenades::max_damage, "lethal close in");
    check(Grenades::damage_at(Grenades::blast_radius) == 0 && Grenades::damage_at(30) == 0, "nothing beyond the radius");
    float last = Grenades::max_damage;
    for (float d = 2.5F; d < Grenades::blast_radius; d += .25F) { const float now = Grenades::damage_at(d); check(now <= last, "falls off"); last = now; }
    check(Grenades::damage_at(5) > 30 && Grenades::damage_at(5) < 110, "a 5 m blast wounds");

    // Throwing: needs grenades, has a cooldown, lobs forward, lands, rolls to rest and explodes on time.
    Grenades grenades;
    check(!grenades.throw_grenade({0, 1.6F, 0}, {0, 0, 1}, {}), "no grenades, no throw");
    grenades.reset(2);
    check(grenades.throw_grenade({0, 1.6F, 0}, {0, 0, 1}, {}), "throw");
    check(grenades.count() == 1 && !grenades.throw_grenade({0, 1.6F, 0}, {0, 0, 1}, {}), "cooldown between throws");
    const auto world = box_world();
    float time = 0;
    bool exploded = false;
    Vector3 blast{};
    while (time < 5 && !exploded) {
        grenades.update(.02F, world);
        time += .02F;
        for (const auto& g : grenades.live()) check(g.position.y >= -.01F && g.position.z <= 20.01F, "never passes through the floor or wall");
        if (!grenades.exploded().empty()) { exploded = true; blast = grenades.exploded().front(); }
    }
    check(exploded && std::abs(time - Grenades::fuse_seconds) < .05F, "explodes after the fuse");
    check(blast.z > 10 && blast.z < 20 && blast.y < .3F, "landed downrange, short of the wall, on the floor");
    check(grenades.live().empty() && grenades.explosions().size() == 1, "one blast, no grenade left");
    // A throw straight at the wall bounces back off it.
    grenades.update(1.0F, world);
    check(grenades.throw_grenade({0, 1.6F, 15}, {0, 0, 1}, {}), "second throw");
    for (int i = 0; i < 40; ++i) grenades.update(.02F, world);
    check(!grenades.live().empty() && grenades.live().front().bounces > 0 && grenades.live().front().position.z < 20, "bounces off the wall");

    // Rockets: fly straight at the wall and go off on it, a bigger blast than a frag.
    {
        Grenades rockets;
        rockets.launch_rocket({0, 1.5F, 0}, {0, 0, 60});
        float flown = 0;
        while (rockets.exploded().empty() && flown < 2) { rockets.update(.02F, world); flown += .02F; }
        check(rockets.exploded().size() == 1 && rockets.exploded_rocket().size() == 1 && rockets.exploded_rocket()[0], "a rocket bursts on what it hits");
        check(rockets.exploded()[0].z > 19 && rockets.exploded()[0].z < 20.01F && rockets.exploded()[0].y > 1.2F, "at the wall, still at shoulder height");
        check(flown < .45F, "60 m/s: there in a third of a second");
        check(Grenades::rocket_damage_at(5) > Grenades::damage_at(5) && Grenades::rocket_damage_at(9) > 0 &&
              Grenades::rocket_damage_at(Grenades::rocket_blast_radius) == 0, "a rocket's blast is bigger");
        rockets.launch_rocket({0, 1.5F, 0}, {0, 8, -60});      // up and away from the wall: gone at the end of its motor
        float life = 0;
        do { rockets.update(.05F, world); life += .05F; } while (rockets.exploded().empty() && life < 8);
        check(std::abs(life - Grenades::rocket_life) < .11F, "a miss bursts when the motor is spent");
    }

    // Feedback: hits merge per target, kills mark and streak, hurt flashes, points and shakes.
    CombatFeedback feedback;
    feedback.hit({5, 1, 5}, 20, false);
    feedback.hit({5.3F, 1.2F, 5}, 22, true);  // second pellet, same body
    feedback.hit({30, 1, 5}, 10, false);       // someone else
    check(feedback.numbers().size() == 2 && std::abs(feedback.numbers()[0].amount - 42) < .01F && feedback.numbers()[0].headshot, "hits merge into one number per target");
    feedback.kill({5, 0, 5});
    check(feedback.numbers()[0].kill && feedback.kill_marker() > 0 && feedback.streak() == 1, "the kill is marked");
    feedback.update(1.0F);
    feedback.kill({30, 0, 5});
    check(feedback.streak() == 2, "a second kill inside the window is a streak");
    feedback.update(5.0F);
    check(feedback.numbers().empty() && feedback.streak() == 0 && feedback.kill_marker() == 0, "numbers fade, streak ends");
    feedback.hurt(30, {10, 1, 0}, true);
    check(feedback.flash() > .5F && feedback.indicator() > 1 && feedback.source().x == 10 && feedback.shake_amount() > 0, "hurt: flash, arrow and shake");
    check(Vector3Length(feedback.shake()) > 0 || feedback.shake_amount() > 0, "the camera shakes");
    feedback.update(3.0F);
    check(feedback.flash() == 0 && feedback.indicator() == 0 && feedback.shake_amount() == 0, "it all settles");
    feedback.blast(.5F);
    check(feedback.shake_amount() >= .5F && feedback.indicator() == 0, "a blast shakes without an arrow");
    std::cout << "grenades and combat feedback ok\n";
}
