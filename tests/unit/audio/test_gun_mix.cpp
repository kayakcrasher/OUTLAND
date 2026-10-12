#include "outland/engine/audio/GunAudio.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
using outland::engine::audio::mix_shot;
using outland::engine::audio::GunAudio;
namespace {
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}
int main() {
    const Vector3 ear{0, 1.7F, 0}, ahead{0, 0, 1}; // looking along +z; right is -x
    const auto close = mix_shot({0, 1.7F, 10}, ear, ahead, 300);
    const auto mid = mix_shot({0, 1.7F, 60}, ear, ahead, 300);
    const auto far = mix_shot({0, 1.7F, 280}, ear, ahead, 300);
    const auto beyond = mix_shot({0, 1.7F, 500}, ear, ahead, 300);
    check(close.audible && !close.distant && close.volume > .4F, "a shot 10 m away is loud and close");
    check(mid.audible && mid.distant && mid.volume < close.volume, "60 m: the distant mix, quieter");
    check(far.audible && far.volume < mid.volume && far.volume > 0, "near the edge of hearing: faint");
    check(!beyond.audible, "out of earshot");
    check(std::abs(far.delay - 280 / GunAudio::speed_of_sound) < .001F && close.delay < .05F, "sound travels at 343 m/s");
    check(std::abs(close.pan) < .05F, "dead ahead is centred");
    const auto right = mix_shot({-30, 1.7F, 0}, ear, ahead, 300), left = mix_shot({30, 1.7F, 0}, ear, ahead, 300);
    check(right.pan > .5F && left.pan < -.5F, "a shooter on the right is heard on the right");
    const auto behind_right = mix_shot({-20, 1.7F, -20}, ear, ahead, 300);
    check(behind_right.pan > .3F, "behind and to the right is still right");
    const auto blast = mix_shot({0, 1.7F, 10}, ear, ahead, 900, 1.6F);
    check(blast.volume > close.volume, "a blast is louder than a shot at the same range");
    check(!mix_shot({0, 0, 0}, ear, ahead, 0).audible, "no hearing radius, no sound");
    // A round cracking past (GunAudio::near_miss: 12 m, loudness 2.5): loud beside the ear,
    // on the side it passed, and nothing from a round that went wide.
    const auto past = mix_shot({-1.5F, 1.7F, .5F}, ear, ahead, 12, 2.5F);
    check(past.audible && past.volume > .8F && past.pan > .2F, "a round past the right ear cracks loud on the right");
    check(!mix_shot({0, 1.7F, 20}, ear, ahead, 12, 2.5F).audible, "a round 20 m off is not heard going by");
    std::cout << "gun audio mixing ok\n";
}
