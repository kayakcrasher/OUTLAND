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
    std::cout << "gun audio mixing ok\n";
}
