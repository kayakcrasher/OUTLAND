#include "outland/game/vehicles/VehicleAudio.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <unordered_set>
using namespace outland::game::vehicles;
namespace {
bool ready = false;
int loads = 0, unloads = 0, stream_loads = 0, stream_unloads = 0, starts = 0, stops = 0, chunks = 0, one_shots = 0;
std::unordered_set<void *> playing, streaming;
} // namespace
extern "C" {
bool __wrap_IsAudioDeviceReady() { return ready; }
bool __wrap_FileExists(const char *) { return true; }
const char *__wrap_GetApplicationDirectory() { return "/"; }
Sound __wrap_LoadSound(const char *) {
    Sound s{};
    s.stream.buffer = reinterpret_cast<decltype(s.stream.buffer)>(static_cast<uintptr_t>(++loads));
    return s;
}
void __wrap_UnloadSound(Sound) { ++unloads; }
void __wrap_SetSoundVolume(Sound, float) {}
bool __wrap_IsSoundPlaying(Sound s) { return playing.contains(s.stream.buffer); }
void __wrap_PlaySound(Sound s) {
    assert(playing.insert(s.stream.buffer).second);
    ++one_shots;
}
void __wrap_StopSound(Sound s) { playing.erase(s.stream.buffer); }
void __wrap_SetAudioStreamBufferSizeDefault(int) {}
AudioStream __wrap_LoadAudioStream(unsigned, unsigned, unsigned) {
    AudioStream a{};
    a.buffer = reinterpret_cast<decltype(a.buffer)>(static_cast<uintptr_t>(1000 + ++stream_loads));
    return a;
}
void __wrap_UnloadAudioStream(AudioStream) { ++stream_unloads; }
void __wrap_UpdateAudioStream(AudioStream, const void *data, int frames) {
    assert(data && frames == VehicleAudio::chunk);
    ++chunks;
}
bool __wrap_IsAudioStreamProcessed(AudioStream) { return true; }   // the device always wants more
void __wrap_PlayAudioStream(AudioStream a) { ++starts; streaming.insert(a.buffer); }
void __wrap_StopAudioStream(AudioStream a) { ++stops; streaming.erase(a.buffer); }
void __wrap_SetAudioStreamVolume(AudioStream, float v) { assert(v >= 0 && v <= 1); }
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
        assert(loads == 0 && stream_loads == 0);
        ready = true;
        audio.update(cars, region, .02F, false);
        assert(loads == 6 && stream_loads == 2 && starts == 1 && streaming.size() == 1);
        // Full throttle: the revs climb, the box shifts up, and the stream is fed in bounded chunks.
        int top_gear = 1;
        for (int i = 0; i < 400; ++i) {
            cars.begin_frame();
            cars.update(.02F, {1, 0, false, false, true}, region, cars.seat(region));
            const int before = chunks;
            audio.update(cars, region, .02F, false);
            assert(chunks - before <= 2);
            top_gear = std::max(top_gear, audio.gearbox().gear());
        }
        assert(top_gear >= 2 && audio.engine().firings() > 100 && audio.gearbox().rpm() > 1000);
        assert(starts == 1 && one_shots <= 5);
        audio.update(cars, region, .02F, true);
        assert(playing.empty() && streaming.empty());
        audio.update(cars, region, .02F, false);
        assert(starts == 2 && loads == 6 && stream_loads == 2);
        cars.begin_frame();
        Vector3 exit{};
        assert(cars.exit(region, exit, true));
        audio.update(cars, region, .02F, false);
        assert(streaming.empty());   // out of the car: the engine is off
        cars.begin_frame();
        audio.update(cars, region, .4F, false);
        const int count = one_shots;
        for (int i = 0; i < 100; ++i)
            audio.update(cars, region, .02F, false);
        assert(one_shots == count && starts == 2);
    }
    assert(unloads == loads && stream_unloads == stream_loads && playing.empty());
    std::cout << "[PASS] Vehicle audio lazy loading, synthesised engine streams fed in bounded chunks, "
                 "gears, non-overlapping effects, pause/resume, shutdown/doors and unload ownership\n";
}
