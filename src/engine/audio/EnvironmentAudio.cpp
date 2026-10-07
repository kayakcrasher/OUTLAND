#include "outland/engine/audio/EnvironmentAudio.hpp"
#include "outland/world/assets/GroundSurface.hpp"

#include <algorithm>
#include <cmath>

namespace outland::engine::audio {

EnvironmentAudio::EnvironmentAudio(const std::string& directory) {
    if (!IsAudioDeviceReady()) {
        InitAudioDevice();
        owns_device_ = IsAudioDeviceReady();
    }
    ready_ = IsAudioDeviceReady();
    if (!ready_) return;

    const std::string wind_path = directory + "/verda_wind.wav";
    if (FileExists(wind_path.c_str())) {
        wind_ = LoadMusicStream(wind_path.c_str());
        if (IsMusicValid(wind_)) {
            wind_.looping = true;
            PlayMusicStream(wind_);
        }
    }
    constexpr std::array<const char*,3> surfaces{"grass", "dirt", "gravel"};
    for (std::size_t surface = 0; surface < surfaces.size(); ++surface) {
        for (std::size_t variant = 0; variant < 3; ++variant) {
            const std::string path = directory + "/step_" + surfaces[surface] +
                                     "_" + std::to_string(variant) + ".wav";
            if (FileExists(path.c_str())) steps_[surface][variant] = LoadSound(path.c_str());
        }
    }
    for (std::size_t weapon = 0; weapon < gunshots_.size(); ++weapon) {
        const std::string path = directory + (weapon == 0 ? "/gunshot_pistol.wav" : "/gunshot_rifle.wav");
        if (FileExists(path.c_str())) for (auto& sound : gunshots_[weapon]) sound = LoadSound(path.c_str());
    }
    const auto load = [&](const char* name) {
        const std::string path = directory + "/" + name;
        return FileExists(path.c_str()) ? LoadSound(path.c_str()) : Sound{};
    };
    reload_ = load("reload.wav"); hit_ = load("target_hit.wav"); dry_ = load("dry_fire.wav");
    set_volume(volume_);
}

EnvironmentAudio::~EnvironmentAudio() {
    if (!ready_) return;
    for (auto& surface : steps_) {
        for (Sound sound : surface) if (IsSoundValid(sound)) UnloadSound(sound);
    }
    if (IsMusicValid(wind_)) UnloadMusicStream(wind_);
    for (auto& weapon : gunshots_) for (Sound sound : weapon) if (IsSoundValid(sound)) UnloadSound(sound);
    for (Sound sound : {reload_, hit_, dry_}) if (IsSoundValid(sound)) UnloadSound(sound);
    if (owns_device_) CloseAudioDevice();
}

void EnvironmentAudio::set_volume(float volume) {
    volume_ = std::clamp(volume, 0.0F, 1.0F);
    const float effective = muted_ ? 0.0F : volume_;
    if (!ready_) return;
    if (IsMusicValid(wind_)) SetMusicVolume(wind_, effective * 0.30F);
    for (auto& surface : steps_) {
        for (Sound sound : surface) if (IsSoundValid(sound)) SetSoundVolume(sound, effective * 0.65F);
    }
    for (auto& weapon : gunshots_) for (Sound sound : weapon)
        if (IsSoundValid(sound)) SetSoundVolume(sound, effective * 0.80F);
    for (Sound sound : {reload_, hit_, dry_}) if (IsSoundValid(sound)) SetSoundVolume(sound, effective * 0.55F);
}

void EnvironmentAudio::play_combat(game::combat::WeaponId weapon, const game::combat::WeaponEvents& events) {
    if (!ready_ || muted_) return;
    for (int i = 0; i < events.shots; ++i) {
        const Sound sound = gunshots_[static_cast<std::size_t>(weapon)][gunshot_index_++ % 3];
        if (IsSoundValid(sound)) PlaySound(sound);
    }
    if (events.reload_started && IsSoundValid(reload_)) PlaySound(reload_);
    if (events.target_hits > 0 && IsSoundValid(hit_)) PlaySound(hit_);
    if (events.dry_fire && IsSoundValid(dry_)) PlaySound(dry_);
}

void EnvironmentAudio::toggle_mute() {
    muted_ = !muted_;
    set_volume(volume_);
}

void EnvironmentAudio::update(Vector3 position, bool grounded, bool active,
                              const world::VerdaRegion& region) {
    if (!ready_) return;
    if (IsMusicValid(wind_)) {
        if (active) {
            ResumeMusicStream(wind_);
            UpdateMusicStream(wind_);
        } else PauseMusicStream(wind_);
    }
    const float dx = position.x - previous_.x;
    const float dz = position.z - previous_.z;
    const float distance = std::sqrt(dx*dx + dz*dz);
    // Resolved movement controls cadence: pushing a wall stays silent.
    // Reset on menu, jumping and teleporting; no catch-up bursts.
    if (!active || !have_position_ || !grounded || !was_grounded_ || distance > 2.0F) {
        stride_distance_ = 0.0F;
    } else {
        stride_distance_ += distance;
        if (stride_distance_ >= 1.65F) {
            stride_distance_ = std::fmod(stride_distance_, 1.65F);
            const auto surface = static_cast<std::size_t>(world::assets::ground_surface(position, region));
            const std::size_t variant = step_number_++ % 3;
            Sound sound = steps_[surface][variant];
            if (!muted_ && IsSoundValid(sound)) {
                SetSoundPitch(sound, 0.97F + static_cast<float>(variant) * 0.03F);
                PlaySound(sound);
            }
        }
    }
    previous_ = position;
    have_position_ = active;
    was_grounded_ = grounded;
}

} // namespace outland::engine::audio
