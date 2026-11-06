#include "outland/game/vehicles/VehicleAudio.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <unordered_set>
#include <vector>
using namespace outland::game::vehicles;
namespace {
bool ready = false;
int loads = 0, unloads = 0, stream_loads = 0, stream_unloads = 0, starts = 0, stops = 0, one_shots = 0;
std::unordered_set<void *> playing, streaming;
std::map<void *, AudioCallback> callbacks;
long pulled = 0;
float loudest = 0, biggest_step = 0;
// The sound device: every playing engine stream asked for `frames` more (from its callback).
void pull(int frames) {
    std::vector<short> block(static_cast<std::size_t>(frames));
    for (void *stream : streaming) {
        assert(callbacks.contains(stream));
        callbacks[stream](block.data(), static_cast<unsigned>(frames));
        pulled += frames;
        for (std::size_t i = 0; i < block.size(); ++i) {
            loudest = std::max(loudest, std::abs(block[i] / 32768.0F));
            if (i) biggest_step = std::max(biggest_step, std::abs(block[i] - block[i - 1]) / 32768.0F);
        }
    }
}
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
void __wrap_SetAudioStreamCallback(AudioStream a, AudioCallback callback) { callbacks[a.buffer] = callback; }
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
        assert(loads == 6 && stream_loads == 2 && starts == 1 && streaming.size() == 1 && callbacks.size() == 2);
        // Full throttle: the revs climb, the box shifts up, and the device pulls the engine from its
        // callback as it wants it (here 20 ms a frame; also through a 300 ms stall: no gap).
        int top_gear = 1;
        for (int i = 0; i < 400; ++i) {
            cars.begin_frame();
            cars.update(.02F, {1, 0, false, false, true}, region, cars.seat(region));
            audio.update(cars, region, .02F, false);
            pull(i == 200 ? VehicleAudio::sample_rate * 3 / 10 : VehicleAudio::sample_rate / 50);
            top_gear = std::max(top_gear, audio.gearbox().gear());
        }
        assert(top_gear >= 2 && audio.engine().firings() > 100 && audio.gearbox().rpm() > 1000);
        assert(pulled > 400 * VehicleAudio::sample_rate / 50 && loudest > .05F && loudest <= 1);
        // The revs glide: no sample-to-sample jump anywhere near a click.
        assert(biggest_step < .5F);
        assert(starts == 1 && one_shots <= 5);
        // Paused: the engine fades out, then its stream stops.
        audio.update(cars, region, .02F, true);
        assert(playing.empty() && streaming.size() == 1);
        for (int i = 0; i < 20; ++i) { audio.update(cars, region, .02F, true); pull(441); }
        assert(streaming.empty());
        audio.update(cars, region, .02F, false);
        assert(starts == 2 && loads == 6 && stream_loads == 2);
        cars.begin_frame();
        Vector3 exit{};
        assert(cars.exit(region, exit, true));
        for (int i = 0; i < 20; ++i) { audio.update(cars, region, .02F, false); pull(441); }
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
