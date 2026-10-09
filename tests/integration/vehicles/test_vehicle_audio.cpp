#include "outland/game/vehicles/VehicleAudio.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <unordered_set>
using namespace outland::game::vehicles;
namespace {
bool ready = false;
int loads = 0, unloads = 0, streams = 0, updates = 0, one_shots = 0;
std::unordered_set<void *> playing;
} // namespace
extern "C" {
bool __wrap_IsAudioDeviceReady() { return ready; }
bool __wrap_FileExists(const char *) { return true; }
const char *__wrap_GetApplicationDirectory() { return "/"; }
Music __wrap_LoadMusicStream(const char *) {
    ++loads;
    Music m{};
    m.stream.buffer = reinterpret_cast<decltype(m.stream.buffer)>(1);
    return m;
}
Sound __wrap_LoadSound(const char *) {
    Sound s{};
    s.stream.buffer = reinterpret_cast<decltype(s.stream.buffer)>(static_cast<uintptr_t>(++loads));
    return s;
}
void __wrap_UnloadMusicStream(Music) { ++unloads; }
void __wrap_UnloadSound(Sound) { ++unloads; }
void __wrap_SetMusicVolume(Music, float) {}
void __wrap_SetSoundVolume(Sound, float) {}
void __wrap_SetMusicPitch(Music, float) {}
void __wrap_PlayMusicStream(Music) { ++streams; }
void __wrap_UpdateMusicStream(Music) { ++updates; }
void __wrap_StopMusicStream(Music) {}
bool __wrap_IsSoundPlaying(Sound s) { return playing.contains(s.stream.buffer); }
void __wrap_PlaySound(Sound s) {
    assert(playing.insert(s.stream.buffer).second);
    ++one_shots;
}
void __wrap_StopSound(Sound s) { playing.erase(s.stream.buffer); }
}
int main() {
    VehicleRegistry registry;
    std::string error;
    assert(registry.load(
        std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error));
    outland::world::VerdaRegion region;
    VehicleSystem cars(registry);
    cars.reconcile(region);
    assert(cars.enter(region, 0, {5, 0, 10}));
    {
        VehicleAudio audio;
        audio.update(cars, region, .02F, false);
        assert(loads == 0);
        ready = true;
        audio.update(cars, region, .02F, false);
        assert(loads == 8 && streams == 1);
        for (int i = 0; i < 100; ++i) {
            cars.begin_frame();
            cars.update(.02F, {1, 0, false, false, true}, region, cars.seat(region));
            audio.update(cars, region, .02F, false);
        }
        assert(loads == 8 && streams == 1 && updates == 101 && one_shots <= 5);
        audio.update(cars, region, .02F, true);
        assert(playing.empty());
        audio.update(cars, region, .02F, false);
        assert(streams == 2 && loads == 8);
        cars.begin_frame();
        Vector3 exit{};
        assert(cars.exit(region, exit, true));
        audio.update(cars, region, .02F, false);
        cars.begin_frame();
        audio.update(cars, region, .4F, false);
        const int count = one_shots;
        for (int i = 0; i < 100; ++i)
            audio.update(cars, region, .02F, false);
        assert(one_shots == count && streams == 2);
    }
    assert(unloads == loads && playing.empty());
    std::cout << "[PASS] Vehicle audio lazy loading, one bounded engine stream, non-overlapping "
                 "effects, pause/resume, shutdown/doors and unload ownership\n";
}
