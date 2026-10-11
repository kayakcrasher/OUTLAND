#include "outland/engine/audio/GunAudio.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::engine::audio {

ShotMix mix_shot(Vector3 source, Vector3 listener, Vector3 forward, float hearing_radius, float loudness) {
    ShotMix mix;
    const Vector3 to = Vector3Subtract(source, listener);
    const float distance = Vector3Length(to);
    if (!std::isfinite(distance) || hearing_radius <= 0 || distance > hearing_radius * 1.25F) return mix;
    mix.audible = true;
    // Inverse-distance falloff, steep enough that a fight 200 m off is a distant pop.
    mix.volume = std::clamp(loudness / (1 + distance / 9), 0.0F, 1.0F);
    mix.distant = distance > 35;
    if (mix.distant) mix.volume = std::min(.75F, mix.volume * 2.2F); // the distant mix is already quiet
    mix.volume *= std::clamp(1.25F - distance / hearing_radius, 0.0F, 1.0F);
    mix.delay = distance / GunAudio::speed_of_sound;
    // Pan by which side of the head it is on (right is forward x up), softened up close.
    Vector3 flat_forward{forward.x, 0, forward.z};
    if (Vector3Length(flat_forward) > 1e-4F && distance > 1e-3F) {
        flat_forward = Vector3Normalize(flat_forward);
        const Vector3 right{-flat_forward.z, 0, flat_forward.x};
        const Vector3 dir = Vector3Normalize({to.x, 0, to.z});
        mix.pan = std::clamp(Vector3DotProduct(dir, right) * .8F * std::min(1.0F, distance / 4), -.8F, .8F);
    }
    return mix;
}

namespace {
const char* gun_name(game::combat::WeaponId id) {
    switch (id) {
        case game::combat::WeaponId::Pistol: return "pistol";
        case game::combat::WeaponId::Rifle: return "rifle";
        case game::combat::WeaponId::Smg: return "smg";
        case game::combat::WeaponId::Shotgun: return "shotgun";
        case game::combat::WeaponId::Sniper: return "sniper";
    }
    return "rifle";
}
}

GunAudio::GunAudio(const std::string& directory) {
    ready_ = IsAudioDeviceReady();
    if (!ready_) return;
    for (std::size_t i = 0; i < game::combat::weapon_count; ++i) {
        const std::string name = gun_name(static_cast<game::combat::WeaponId>(i));
        for (int v = 0; v < 3; ++v) load(close_[i], directory + "/" + name + "_" + std::to_string(v) + ".wav");
        load(distant_[i], directory + "/" + name + "_distant.wav");
    }
    load(explosion_, directory + "/explosion.wav");
    load(explosion_distant_, directory + "/explosion_distant.wav");
    load(pump_, directory + "/shotgun_pump.wav");
    load(bolt_, directory + "/sniper_bolt.wav");
    for (int v = 0; v < 3; ++v) load(casing_, directory + "/casing_" + std::to_string(v) + ".wav");
    load(kill_, directory + "/kill.wav");
}

GunAudio::~GunAudio() {
    if (!ready_) return;
    for (auto& bank : close_) unload(bank);
    for (auto& bank : distant_) unload(bank);
    for (Bank* bank : {&explosion_, &explosion_distant_, &pump_, &bolt_, &casing_, &kill_}) unload(*bank);
}

void GunAudio::load(Bank& bank, const std::string& path) {
    if (!FileExists(path.c_str())) return;
    const Sound sound = LoadSound(path.c_str());
    if (!IsSoundValid(sound)) return;
    bank.base.push_back(sound);
    std::array<Sound, voice_count> aliases{};
    for (auto& alias : aliases) alias = LoadSoundAlias(sound);
    bank.voices.push_back(aliases);
}

void GunAudio::unload(Bank& bank) {
    for (auto& aliases : bank.voices) for (Sound alias : aliases) if (IsSoundValid(alias)) UnloadSoundAlias(alias);
    for (Sound sound : bank.base) UnloadSound(sound);
    bank.voices.clear();
    bank.base.clear();
}

float GunAudio::random() {
    random_ ^= random_ << 13; random_ ^= random_ >> 17; random_ ^= random_ << 5;
    return static_cast<float>(random_ & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

void GunAudio::set_volume(float volume, bool muted) {
    volume_ = std::clamp(volume, 0.0F, 1.0F);
    muted_ = muted;
}

void GunAudio::play(Bank& bank, float volume, float pan, float pitch) {
    if (!ready_ || muted_ || bank.voices.empty() || volume <= .002F) return;
    // A different recording each time, and the voice that has waited longest.
    const unsigned pick = bank.next++;
    auto& aliases = bank.voices[pick % bank.voices.size()];
    Sound voice = aliases[(pick / bank.voices.size()) % voice_count];
    SetSoundVolume(voice, std::clamp(volume * volume_ * 1.35F, 0.0F, 1.0F));
    SetSoundPan(voice, pan);
    SetSoundPitch(voice, pitch);
    PlaySound(voice);
}

void GunAudio::player_shot(game::combat::WeaponId weapon) {
    const auto index = static_cast<std::size_t>(weapon);
    play(close_[index], 1.0F, 0, .97F + random() * .06F);
    // The gun's own mechanics and the spent case, after the report.
    if (weapon == game::combat::WeaponId::Shotgun) pending_.push_back({&pump_, .32F, .55F, 0, 1});
    else if (weapon == game::combat::WeaponId::Sniper) pending_.push_back({&bolt_, .42F, .5F, 0, 1});
    if (weapon != game::combat::WeaponId::Shotgun)
        pending_.push_back({&casing_, .38F + random() * .12F, .22F, .25F, .95F + random() * .1F});
}

void GunAudio::world_shot(game::combat::WeaponId weapon, Vector3 source, Vector3 listener, Vector3 forward) {
    const auto& gun = game::combat::definition(weapon);
    const auto mix = mix_shot(source, listener, forward, gun.hearing * 1.6F);
    if (!mix.audible) return;
    const auto index = static_cast<std::size_t>(weapon);
    pending_.push_back({mix.distant ? &distant_[index] : &close_[index], mix.delay, mix.volume, mix.pan, .96F + random() * .08F});
}

void GunAudio::explosion(Vector3 at, Vector3 listener, Vector3 forward) {
    const auto mix = mix_shot(at, listener, forward, 900, 1.6F);
    if (!mix.audible) return;
    pending_.push_back({mix.distant ? &explosion_distant_ : &explosion_, mix.delay, std::min(1.0F, mix.volume * 1.3F), mix.pan, .95F + random() * .1F});
}

void GunAudio::kill_confirm() { play(kill_, .55F, 0, 1); }

void GunAudio::update(float dt) {
    if (!std::isfinite(dt) || dt < 0) return;
    for (auto& p : pending_) p.delay -= dt;
    for (const auto& p : pending_) if (p.delay <= 0) play(*p.bank, p.volume, p.pan, p.pitch);
    std::erase_if(pending_, [](const Pending& p) { return p.delay <= 0; });
    if (pending_.size() > 256) pending_.erase(pending_.begin(), pending_.begin() + static_cast<long>(pending_.size() - 256));
}

} // namespace outland::engine::audio
