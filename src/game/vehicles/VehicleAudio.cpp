#include "outland/game/vehicles/VehicleAudio.hpp"
#include <algorithm>
#include <cmath>
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
    const auto loop = path("Car_Engine_Loop.ogg");
    if (FileExists(loop.c_str())) {
        engine_ = LoadMusicStream(loop.c_str());
        engine_.looping = true;
        SetMusicVolume(engine_, .35F);
    }
    const char *files[]{"Car_Engine_Start_Up.ogg", "Car_Engine_Turning_Off.ogg",
                        "Car_Acceleration.ogg",    "Car_Horn.ogg",
                        "Car_Door_Open.ogg",       "Car_Door_Close.ogg",
                        "Car_Parking_Brake.ogg"};
    for (std::size_t i = 0; i < sounds_.size(); ++i) {
        const auto source = path(files[i]);
        if (FileExists(source.c_str())) {
            sounds_[i] = LoadSound(source.c_str());
            if (sounds_[i].stream.buffer)
                SetSoundVolume(sounds_[i], .5F);
        }
    }
}
void VehicleAudio::play(std::size_t i) {
    if (sounds_[i].stream.buffer && !IsSoundPlaying(sounds_[i]))
        PlaySound(sounds_[i]);
}
void VehicleAudio::stop() {
    if (engine_.stream.buffer)
        StopMusicStream(engine_);
    looping_ = false;
    for (auto sound : sounds_)
        if (sound.stream.buffer)
            StopSound(sound);
    door_timer_ = 0;
}
VehicleAudio::~VehicleAudio() {
    stop();
    if (engine_.stream.buffer)
        UnloadMusicStream(engine_);
    for (auto sound : sounds_)
        if (sound.stream.buffer)
            UnloadSound(sound);
}
void VehicleAudio::update(const VehicleSystem &system, const world::VerdaRegion &region, float dt,
                          bool paused, float volume) {
    initialize();
    if (!initialized_)
        return;
    volume = std::clamp(volume, 0.0F, 1.0F);
    if (volume != volume_) {
        volume_ = volume;
        if (engine_.stream.buffer)
            SetMusicVolume(engine_, .35F * volume_);
        for (auto sound : sounds_)
            if (sound.stream.buffer)
                SetSoundVolume(sound, .5F * volume_);
    }
    const auto *v = system.driver();
    const auto *a = v ? system.asset(region, v->id) : nullptr;
    const auto *d = a ? system.definition(*a) : nullptr;
    const bool running = a && d && a->vehicle.engine > 0 && !a->vehicle.destroyed && !paused;
    if (paused) {
        stop();
        return;
    }
    if (system.events().started)
        play(0);
    if (system.events().stopped)
        play(1);
    if (system.events().door) {
        play(4);
        door_timer_ = .35F;
    }
    if (door_timer_ > 0) {
        door_timer_ -= dt;
        if (door_timer_ <= 0)
            play(5);
    }
    if (running && engine_.stream.buffer) {
        if (!looping_) {
            PlayMusicStream(engine_);
            looping_ = true;
        }
        SetMusicPitch(engine_, .8F + std::min(1.2F, std::abs(v->speed) / d->top_speed));
        UpdateMusicStream(engine_);
    } else if (looping_) {
        StopMusicStream(engine_);
        looping_ = false;
    }
    acceleration_cooldown_ = std::max(0.0F, acceleration_cooldown_ - dt);
    if (running) {
        if (std::abs(v->speed) > last_speed_ + .05F && acceleration_cooldown_ <= 0) {
            play(2);
            acceleration_cooldown_ = 2;
        }
        if (system.events().horn)
            play(3);
        if (system.events().parking && !parking_)
            play(6);
        last_speed_ = std::abs(v->speed);
        parking_ = system.events().parking;
    } else {
        last_speed_ = 0;
        parking_ = false;
    }
}
} // namespace outland::game::vehicles
