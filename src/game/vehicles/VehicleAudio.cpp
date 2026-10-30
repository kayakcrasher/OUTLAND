#include "outland/game/vehicles/VehicleAudio.hpp"
#include <algorithm>
#include <cmath>
#include <raymath.h>
namespace outland::game::vehicles {
void VehicleAudio::initialize() {
    if (initialized_ || !IsAudioDeviceReady())
        return;
    initialized_ = true;
    const auto path = [](const char *name) {
        const std::string local = "assets/verda/vehicles/ggbot/audio/" + std::string(name);
        const std::string packaged = std::string(GetApplicationDirectory()) + local;
        return FileExists(packaged.c_str()) ? packaged : local;
    };
    const char *files[]{"Car_Engine_Start_Up.ogg", "Car_Engine_Turning_Off.ogg", "Car_Horn.ogg",
                        "Car_Door_Open.ogg",       "Car_Door_Close.ogg",         "Car_Parking_Brake.ogg"};
    for (std::size_t i = 0; i < sounds_.size(); ++i) {
        const auto source = path(files[i]);
        if (FileExists(source.c_str())) {
            sounds_[i] = LoadSound(source.c_str());
            if (sounds_[i].stream.buffer)
                SetSoundVolume(sounds_[i], .5F);
        }
    }
    // The engines: one stream each, fed a chunk at a time.
    SetAudioStreamBufferSizeDefault(chunk);
    for (auto &voice : voices_)
        voice.stream = LoadAudioStream(sample_rate, 16, 1);
    SetAudioStreamBufferSizeDefault(0);
    buffer_.assign(chunk, 0);
}
void VehicleAudio::play(std::size_t i) {
    if (sounds_[i].stream.buffer && !IsSoundPlaying(sounds_[i]))
        PlaySound(sounds_[i]);
}
void VehicleAudio::silence(Voice &voice) {
    if (voice.playing && voice.stream.buffer)
        StopAudioStream(voice.stream);
    voice.playing = false;
}
void VehicleAudio::stop() {
    for (auto &voice : voices_)
        silence(voice);
    for (auto sound : sounds_)
        if (sound.stream.buffer)
            StopSound(sound);
    door_timer_ = 0;
}
VehicleAudio::~VehicleAudio() {
    stop();
    for (auto &voice : voices_)
        if (voice.stream.buffer)
            UnloadAudioStream(voice.stream);
    for (auto sound : sounds_)
        if (sound.stream.buffer)
            UnloadSound(sound);
}
void VehicleAudio::tune(Voice &voice, const world::WorldAsset &a) {
    const int engine = a.vehicle.mods[1], exhaust = a.vehicle.mods[7];
    const auto key = a.vehicle.definition + ' ' + std::to_string(engine) + ' ' + std::to_string(exhaust);
    if (voice.vehicle == a.id && voice.profile == key)
        return;
    const bool same = voice.vehicle == a.id;
    voice.vehicle = a.id;
    voice.profile = key;
    voice.synth.set_profile(engine_profile(a.vehicle.definition, engine, exhaust));
    if (!same)
        voice.box.reset(voice.synth.profile());
}
void VehicleAudio::feed(Voice &voice, const EngineSynth::Params &params, float level) {
    if (!voice.stream.buffer)
        return;
    voice.synth.set(params);
    if (!voice.playing) {
        // Two chunks queued before it starts, so it never starts on silence.
        for (int i = 0; i < 2; ++i) {
            voice.synth.render(buffer_.data(), chunk);
            UpdateAudioStream(voice.stream, buffer_.data(), chunk);
        }
        PlayAudioStream(voice.stream);
        voice.playing = true;
    }
    for (int i = 0; i < 2 && IsAudioStreamProcessed(voice.stream); ++i) {
        voice.synth.render(buffer_.data(), chunk);
        UpdateAudioStream(voice.stream, buffer_.data(), chunk);
    }
    SetAudioStreamVolume(voice.stream, std::clamp(level, 0.0F, 1.0F));
}
void VehicleAudio::update(const VehicleSystem &system, const world::VerdaRegion &region, float dt,
                          bool paused, float volume, const Vector3 *listener) {
    initialize();
    if (!initialized_)
        return;
    volume = std::clamp(volume, 0.0F, 1.0F);
    if (volume != volume_) {
        volume_ = volume;
        for (auto sound : sounds_)
            if (sound.stream.buffer)
                SetSoundVolume(sound, .5F * volume_);
    }
    if (paused) {
        stop();
        return;
    }
    const auto *v = system.driver();
    const auto *a = v ? system.asset(region, v->id) : nullptr;
    const auto *d = a ? system.definition(*a) : nullptr;
    const bool running = a && d && a->vehicle.engine > 0 && !a->vehicle.destroyed;
    if (system.events().started)
        play(0);
    if (system.events().stopped)
        play(1);
    if (system.events().door) {
        play(3);
        door_timer_ = .35F;
    }
    if (door_timer_ > 0) {
        door_timer_ -= dt;
        if (door_timer_ <= 0)
            play(4);
    }
    // The player's engine: the gearbox follows the road speed and the throttle; the synth the revs.
    auto &mine = voices_[0];
    if (running) {
        tune(mine, *a);
        const auto &input = system.driver_input();
        const float top = std::max(1.0F, d->top_speed * a->vehicle.top);
        const float speed = v->speed / top;
        // Gas is the pedal pushing the way it is going (or pulling away); the other way is the brake.
        const float gas = std::abs(v->speed) < .5F || (input.throttle > 0) == (v->speed > 0) ? std::abs(input.throttle) : 0.0F;
        const float rpm = mine.box.update(dt, std::abs(speed), gas, mine.synth.profile());
        EngineSynth::Params p;
        p.running = true;
        p.rpm = rpm;
        p.load = input.boost ? 1.0F : gas;
        p.speed = v->speed;
        p.limiter = gas > .5F && rpm >= mine.synth.profile().redline * .985F;
        // The pedal the other way is the brake (a rig's engine brake bites harder with it).
        p.brake = gas <= 0 && std::abs(v->speed) > .5F && input.throttle != 0 ? std::abs(input.throttle) : input.brake ? 1.0F : 0.0F;
        // Tyres: sliding sideways, the handbrake at speed, wheelspin pulling away in something big.
        float squeal = std::clamp((std::abs(v->slip) - 1.2F) / 3.0F, 0.0F, 1.0F);
        if (input.brake && std::abs(v->speed) > 4) squeal = std::max(squeal, .7F);
        if (gas > .95F && std::abs(speed) < .06F && std::abs(v->speed) > .3F && mine.synth.profile().cylinders >= 8) squeal = std::max(squeal, .45F);
        p.squeal = squeal;
        p.volume = 1;
        feed(mine, p, .65F * volume_);
        if (system.events().horn)
            play(2);
        if (system.events().parking && !parking_)
            play(5);
        parking_ = system.events().parking;
    } else {
        silence(mine);
        mine.vehicle.clear();
        parking_ = false;
    }
    // The nearest other vehicle on the move, within earshot.
    const Vector3 ears = listener ? *listener : a ? a->position : Vector3{};
    const VehicleRuntime *near = nullptr;
    const world::WorldAsset *near_asset = nullptr;
    float best = hearing;
    if (listener || a)
        for (const auto &other : system.vehicles()) {
            if (v && other.id == v->id) continue;
            if (std::abs(other.speed) < .5F && !system.autopiloted(other.id)) continue;
            const auto *oa = system.asset(region, other.id);
            if (!oa || !oa->vehicle.enabled || oa->vehicle.destroyed || oa->vehicle.engine <= 0) continue;
            const float distance = Vector3Distance(oa->position, ears);
            if (distance < best) { best = distance; near = &other; near_asset = oa; }
        }
    auto &passing = voices_[1];
    const auto *nd = near_asset ? system.definition(*near_asset) : nullptr;
    if (near && nd) {
        tune(passing, *near_asset);
        const auto *input = system.autopilot(near->id);
        const float gas = input ? std::clamp(input->throttle, 0.0F, 1.0F) : .3F;
        const float rpm = passing.box.update(dt, std::abs(near->speed) / std::max(1.0F, nd->top_speed), gas, passing.synth.profile());
        EngineSynth::Params p;
        p.running = true;
        p.rpm = rpm;
        p.load = gas;
        p.speed = near->speed;
        p.brake = input && input->throttle < 0 ? std::min(1.0F, -input->throttle) : 0.0F;
        p.squeal = std::clamp((std::abs(near->slip) - 1.5F) / 3.0F, 0.0F, 1.0F);
        const float fade = 1 - best / hearing;
        feed(passing, p, .55F * fade * fade * volume_);
    } else {
        silence(passing);
        passing.vehicle.clear();
    }
}
} // namespace outland::game::vehicles
