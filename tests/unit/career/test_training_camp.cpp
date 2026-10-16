// The police training camp: drills start from the firing line, targets pop up and take real
// shots, enough hits qualify, leaving the line stops the drill, three drills graduate.
#include "outland/game/career/TrainingCamp.hpp"
#include <raymath.h>
#include <cmath>
#include <iostream>
using namespace outland::game::career;
namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
// Run a drill, one shot at each target that comes up; every `miss_every`th shot goes high.
int run(TrainingCamp& camp, int drill, int miss_every, Vector3 eye) {
    const Vector3 feet = camp.line();
    check(camp.start(drill), "a drill starts");
    int frames = 0, fired = 0, last = -1;   // one shot per target
    while (camp.phase() != CampPhase::Result && frames++ < 20000) {
        int shots = 0;
        const auto& t = camp.target();
        if (camp.phase() == CampPhase::Running && t.up && !t.hit && t.raise >= 1 && camp.shown() != last) {
            last = camp.shown();
            ++fired;
            Vector3 aim{t.base.x, t.base.y + 1.0F, t.base.z};
            if (miss_every > 0 && fired % miss_every == 0) aim.y += 3;     // over the top
            const Vector3 far = Vector3Add(eye, Vector3Scale(Vector3Normalize(Vector3Subtract(aim, eye)), 60));
            const auto hit = camp.trace(eye, far);
            if (hit.hit()) camp.hit(hit.target, hit.headshot);
            shots = 1;
        }
        camp.update(1.0F / 60, feet, shots);
    }
    return camp.hits();
}
}

int main() {
    TrainingCamp camp;
    camp.place({100, 5, 200}, 90, [](float, float) { return 5.0F; });   // downrange is +x
    check(camp.placed() && std::abs(camp.local(0, 10).x - 110) < .01F && std::abs(camp.local(0, 10).z - 200) < .01F, "downrange along the yaw");
    check(camp.at_line({101, 5, 201}) && !camp.at_line({120, 5, 200}), "the firing line");
    const Vector3 eye{100, 6.6F, 200};

    // A drill: no target before the countdown ends, then a real hit drops it.
    check(camp.start(0) && camp.phase() == CampPhase::Countdown, "countdown");
    check(!camp.start(1), "one drill at a time");
    for (int i = 0; i < 60 * 3 + 5; ++i) camp.update(1.0F / 60, camp.line(), 0);
    check(camp.phase() == CampPhase::Running && camp.target().up && camp.shown() == 1, "the first target pops up");
    // Leaving the line stops it.
    camp.update(1.0F / 60, {130, 5, 200}, 0);
    check(camp.phase() == CampPhase::Idle, "off the line: stopped");

    // Shoot well: every target hit -> qualified.
    check(run(camp, 0, 0, eye) == 10 && camp.qualified(0) && camp.shots() == 10, "pistol drill qualified, 10/10");
    // Shoot badly: miss every other -> 5/10, not enough on the rifle drill.
    check(run(camp, 1, 2, eye) == 5 && !camp.qualified(1) && camp.best(1) == 5, "rifle drill failed at 5/10");
    check(camp.next_drill() == 1, "the failed drill is next");
    check(run(camp, 1, 5, eye) >= 7 && camp.qualified(1), "rifle drill qualified on a retry");
    check(!camp.graduated() && !camp.take_graduation(), "not yet");
    // Movers: the target slides, a shot at where it is still hits.
    check(run(camp, 2, 0, eye) == 8 && camp.qualified(2), "movers qualified");
    check(camp.graduated() && camp.take_graduation() && !camp.take_graduation(), "graduated, once");

    // A shot past a target that is down hits nothing; heads count as heads.
    TrainingCamp other;
    other.place({0, 0, 0}, 0, [](float, float) { return 0.0F; });
    check(!other.trace({0, 1.6F, 0}, {0, 1.6F, 50}).hit(), "nothing up, nothing hit");
    other.start(0);
    for (int i = 0; i < 60 * 4; ++i) other.update(1.0F / 60, other.line(), 0);
    const auto& t = other.target();
    const Vector3 head{t.base.x, t.base.y + 1.55F, t.base.z};
    const auto shot = other.trace({0, 1.55F, 0}, Vector3Add({0, 1.55F, 0}, Vector3Scale(Vector3Normalize(Vector3Subtract(head, {0, 1.55F, 0})), 40)));
    check(shot.hit() && shot.headshot, "a head shot");
    if (failures) return 1;
    std::cout << "[PASS] Police training camp: drills, hits, qualifying, graduation\n";
    return 0;
}
