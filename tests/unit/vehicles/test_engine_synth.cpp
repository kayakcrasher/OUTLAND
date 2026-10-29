#include "outland/game/vehicles/EngineSynth.hpp"
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
using namespace outland::game::vehicles;
namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
double rms(const std::vector<float>& s) {
    double sum = 0;
    for (const float v : s) sum += v * v;
    return std::sqrt(sum / static_cast<double>(s.size()));
}
} // namespace
int main() {
    constexpr int rate = 22050;
    // Every cylinder fires once every two turns: a four at 3000 rpm fires 100 times a second.
    {
        EngineSynth synth(rate);
        synth.set_profile(engine_profile("hatchback"));
        synth.set({true, 3000, .5F, 0, 0, 1, false});
        std::vector<float> second(rate);
        synth.render(second.data(), rate);
        check(std::abs(static_cast<double>(synth.firings()) - 100) <= 2, "a four at 3000 rpm fires 100 times a second: " + std::to_string(synth.firings()));
        bool finite = true, bounded = true;
        for (const float v : second) { finite &= std::isfinite(v); bounded &= std::abs(v) <= 1; }
        check(finite && bounded, "the samples are finite and in range");
        check(rms(second) > .02, "an engine you can hear");
    }
    // A V8 fires twice as often as a four at the same revs; working hard is louder than idling.
    {
        EngineSynth eight(rate), idle(rate), hard(rate);
        eight.set_profile(engine_profile("muscle"));
        eight.set({true, 3000, .5F, 0, 0, 1, false});
        std::vector<float> buffer(rate);
        eight.render(buffer.data(), rate);
        check(std::abs(static_cast<double>(eight.firings()) - 200) <= 2, "a V8 at 3000 rpm fires 200 times a second");
        idle.set_profile(engine_profile("sedan"));
        hard.set_profile(engine_profile("sedan"));
        idle.set({true, 700, 0, 0, 0, 1, false});
        hard.set({true, 5000, 1, 0, 0, 1, false});
        std::vector<float> a(rate), b(rate);
        idle.render(a.data(), rate);
        hard.render(b.data(), rate);
        check(rms(b) > rms(a) * 2, "full throttle at 5000 is much louder than idle");
        EngineSynth off(rate);
        off.set_profile(engine_profile("sedan"));
        off.set({false, 0, 0, 0, 0, 1, false});
        off.render(a.data(), rate);
        check(rms(a) < 1e-4 && off.firings() == 0, "an engine that isn't running is silent");
    }
    // The parts: straight pipes are louder and pop on the overrun; the racing engine revs higher.
    {
        const auto plain = engine_profile("hatchback"), piped = engine_profile("hatchback", 0, 2), racing = engine_profile("hatchback", 5);
        check(piped.gain > plain.gain && piped.pops && !plain.pops, "straight pipes: louder, and they pop");
        check(racing.redline > plain.redline, "the racing engine revs out further");
        check(engine_profile("hatchback", 2).cylinders == 8, "the big block is a V8");
        check(engine_profile("semi").diesel && engine_profile("semi").redline < 2500, "a big rig is a slow-revving diesel");
        EngineSynth synth(rate);
        synth.set_profile(piped);
        synth.set({true, 5000, 0, 20, 0, 1, false});   // off the gas, high revs: the overrun
        std::vector<float> buffer(rate);
        for (int i = 0; i < 4; ++i) synth.render(buffer.data(), rate);
        check(synth.pops() > 0, "straight pipes crackle on the overrun");
    }
    // The gearbox: pulling away the revs rise, it shifts up through the gears as the speed climbs,
    // and comes back down as it slows; the revs never pass the redline.
    {
        const auto p = engine_profile("sedan");
        Gearbox box;
        box.reset(p);
        float speed = 0, highest = 0;
        for (int i = 0; i < 600; ++i) {
            speed = std::min(1.0F, speed + .002F);
            highest = std::max(highest, box.update(1.0F / 60, speed, 1, p));
        }
        check(box.gear() == p.gears, "all the way up the gears: " + std::to_string(box.gear()));
        check(highest <= p.redline * 1.02F, "never past the redline");
        for (int i = 0; i < 600; ++i) {
            speed = std::max(0.0F, speed - .002F);
            box.update(1.0F / 60, speed, 0, p);
        }
        check(box.gear() <= 2 && box.rpm() < p.idle * 1.5F, "back down the gears to a stop");
        Gearbox belt;
        const auto sxs = engine_profile("sxs");
        belt.reset(sxs);
        for (int i = 0; i < 120; ++i) belt.update(1.0F / 60, 0, 1, sxs);
        check(sxs.cvt && belt.rpm() > sxs.idle * 2, "a belt drive revs up with the throttle");
    }
    if (failures == 0) std::cout << "engine synth ok\n";
    return failures == 0 ? 0 : 1;
}
