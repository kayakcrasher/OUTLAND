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
        case game::combat::WeaponId::DoubleBarrel: return "double_barrel";
        case game::combat::WeaponId::AutoShotgun: return "auto_shotgun";
        case game::combat::WeaponId::Rocket: return "rocket";
        case game::combat::WeaponId::MachinePistol: return "machine_pistol";
    }
    return "rifle";
}
}

GunAudio::GunAudio(const std::string& directory, const std::string& steps) {
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
    for (std::size_t i = 0; i < game::combat::weapon_count; ++i)
        load(reload_[i], directory + "/reload_" + gun_name(static_cast<game::combat::WeaponId>(i)) + ".wav");
    load(equip_, directory + "/equip.wav");
    for (int v = 0; v < 2; ++v) {
        load(crash_light_, directory + "/crash_light_" + std::to_string(v) + ".wav");
        load(crash_heavy_, directory + "/crash_heavy_" + std::to_string(v) + ".wav");
    }
    for (int v = 0; v < 3; ++v) load(body_hit_, directory + "/body_hit_" + std::to_string(v) + ".wav");
    load(land_, directory + "/land.wav");
    load(punch_, directory + "/punch.wav");
    load(tinnitus_, directory + "/tinnitus.wav");
    load(pickup_, directory + "/pickup.wav");
    load(pin_, directory + "/grenade_pin.wav");
    load(hitmarker_, directory + "/hitmarker.wav");
    for (int v = 0; v < 3; ++v) load(hurt_, directory + "/hurt_" + std::to_string(v) + ".wav");
    load(armour_, directory + "/armour_hit.wav");
    for (int v = 0; v < 3; ++v) load(whiz_, directory + "/whiz_" + std::to_string(v) + ".wav");
    for (int v = 0; v < 2; ++v) load(ricochet_, directory + "/ricochet_" + std::to_string(v) + ".wav");
    load(zone_, directory + "/zone_warning.wav");
    load(heart_, directory + "/heartbeat.wav");
    for (auto [music, name] : {std::pair{&siren_, "siren"}, std::pair{&rotor_, "rotor_loop"}, std::pair{&wind_, "wind_loop"}})
        if (const std::string path = directory + "/" + name + ".wav"; FileExists(path.c_str())) {
            *music = LoadMusicStream(path.c_str());
            if (IsMusicValid(*music)) music->looping = true;
        }
    load(chute_, directory + "/chute_open.wav");
    if (!steps.empty())
        for (const char* surface : {"gravel", "dirt", "grass"})
            for (int v = 0; v < 3; ++v) load(steps_, steps + "/step_" + surface + "_" + std::to_string(v) + ".wav");
}

GunAudio::~GunAudio() {
    if (!ready_) return;
    for (auto& bank : close_) unload(bank);
    for (auto& bank : distant_) unload(bank);
    for (auto& bank : reload_) unload(bank);
    for (Bank* bank : {&explosion_, &explosion_distant_, &pump_, &bolt_, &casing_, &kill_, &equip_, &pickup_, &pin_,
                       &hitmarker_, &hurt_, &armour_, &whiz_, &ricochet_, &steps_, &zone_, &heart_, &crash_light_,
                       &crash_heavy_, &body_hit_, &land_, &punch_, &tinnitus_, &chute_}) unload(*bank);
    for (Music* music : {&siren_, &rotor_, &wind_}) if (IsMusicValid(*music)) UnloadMusicStream(*music);
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
    // The punch in the chest under the report (heavier for the big guns), and the report rolling
    // back off the hills a moment later.
    static constexpr float punch[game::combat::weapon_count]{.45F, .6F, .45F, .9F, 1.0F, 1.0F, .75F, 1.0F};
    play(punch_, punch[index], 0, .95F + random() * .1F);
    pending_.push_back({&distant_[index], .5F + random() * .25F, .09F + .05F * punch[index], (random() - .5F) * .6F, .97F + random() * .06F});
    // The gun's own mechanics and the spent case, after the report.
    if (weapon == game::combat::WeaponId::Shotgun) pending_.push_back({&pump_, .32F, .55F, 0, 1});
    else if (weapon == game::combat::WeaponId::Sniper) pending_.push_back({&bolt_, .42F, .5F, 0, 1});
    // Shotguns keep their shells until the reload (the pump ejects one); a rocket leaves no case.
    if (!game::combat::is_shotgun(weapon) && weapon != game::combat::WeaponId::Rocket)
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
    // Close enough to be stunned: the ears ring (once at a time).
    const float distance = Vector3Distance(at, listener);
    if (distance < 14 && ringing_ <= 0) {
        pending_.push_back({&tinnitus_, mix.delay + .05F, .55F * (1 - distance / 14), 0, 1});
        ringing_ = 2.5F;
    }
}

void GunAudio::crash(float impact) {
    if (impact < 3 || crash_wait_ > 0) return;
    const bool heavy = impact >= 12;
    play(heavy ? crash_heavy_ : crash_light_, std::clamp(.35F + impact / 25, .35F, 1.0F), 0, .92F + random() * .14F);
    crash_wait_ = heavy ? .6F : .3F;
}

void GunAudio::land(float speed) {
    if (speed < 6) return;
    play(land_, std::clamp((speed - 4) / 12, .25F, .9F), 0, .9F + random() * .15F);
}

void GunAudio::kill_confirm() { play(kill_, .55F, 0, 1); }

void GunAudio::reload(game::combat::WeaponId weapon) {
    auto& bank = reload_[static_cast<std::size_t>(weapon)];
    cancel_reload();
    if (!ready_ || muted_ || bank.voices.empty()) return;
    reloading_ = bank.voices[0][bank.next++ % voice_count];
    SetSoundVolume(reloading_, std::clamp(.8F * volume_ * 1.35F, 0.0F, 1.0F));
    SetSoundPan(reloading_, 0);
    SetSoundPitch(reloading_, 1);
    PlaySound(reloading_);
}

void GunAudio::cancel_reload() {
    if (ready_ && IsSoundValid(reloading_) && IsSoundPlaying(reloading_)) StopSound(reloading_);
    reloading_ = {};
}

void GunAudio::equip() { play(equip_, .6F, 0, .97F + random() * .06F); }
void GunAudio::pickup() { play(pickup_, .55F, 0, .95F + random() * .1F); }
void GunAudio::grenade_pin() { play(pin_, .7F, 0, 1); }
void GunAudio::hitmarker(bool headshot) {
    play(hitmarker_, headshot ? .6F : .4F, 0, headshot ? 1.45F : 1.0F);
    // The round landing in the body, under the tick.
    play(body_hit_, headshot ? .7F : .5F, 0, (headshot ? 1.1F : .95F) + random() * .1F);
}

void GunAudio::hurt(bool armoured) {
    play(hurt_, .8F, 0, .92F + random() * .14F);
    if (armoured) play(armour_, .55F, 0, .95F + random() * .1F);
}

void GunAudio::play_at(Bank& bank, Vector3 at, Vector3 listener, Vector3 forward, float radius, float loudness) {
    const auto mix = mix_shot(at, listener, forward, radius, loudness);
    if (mix.audible) play(bank, mix.volume, mix.pan, .94F + random() * .12F);
}

void GunAudio::near_miss(Vector3 at, Vector3 listener, Vector3 forward) { play_at(whiz_, at, listener, forward, 12, 2.5F); }
void GunAudio::ricochet(Vector3 at, Vector3 listener, Vector3 forward) { play_at(ricochet_, at, listener, forward, 30, 2.2F); }
void GunAudio::footstep(Vector3 at, Vector3 listener, Vector3 forward) { play_at(steps_, at, listener, forward, 26, 1.1F); }
void GunAudio::zone_warning() { play(zone_, .45F, 0, 1); }
void GunAudio::siren(float loudness, float pan) {
    if (!ready_ || !IsMusicValid(siren_)) return;
    const float volume = muted_ ? 0.0F : std::clamp(loudness, 0.0F, 1.0F) * volume_;
    if (volume <= .003F) {
        if (siren_playing_) { PauseMusicStream(siren_); siren_playing_ = false; }
        return;
    }
    if (!siren_playing_) { if (IsMusicStreamPlaying(siren_)) ResumeMusicStream(siren_); else PlayMusicStream(siren_); siren_playing_ = true; }
    SetMusicVolume(siren_, volume);
    SetMusicPan(siren_, pan);
    UpdateMusicStream(siren_);
}

void GunAudio::loop(Music& music, bool& playing, float loudness, float pitch) {
    if (!ready_ || !IsMusicValid(music)) return;
    const float volume = muted_ ? 0.0F : std::clamp(loudness, 0.0F, 1.0F) * volume_;
    if (volume <= .003F) {
        if (playing) { PauseMusicStream(music); playing = false; }
        return;
    }
    if (!playing) { if (IsMusicStreamPlaying(music)) ResumeMusicStream(music); else PlayMusicStream(music); playing = true; }
    SetMusicVolume(music, volume);
    SetMusicPitch(music, std::clamp(pitch, .5F, 2.0F));
    UpdateMusicStream(music);
}

void GunAudio::drop_loops(float rotor, float wind, float wind_pitch) {
    loop(rotor_, rotor_playing_, rotor, 1);
    loop(wind_, wind_playing_, wind, wind_pitch);
}

void GunAudio::chute_open() { play(chute_, .9F, 0, .95F + .1F * random()); }

void GunAudio::heartbeat(float strength) { play(heart_, std::clamp(strength, 0.0F, 1.0F) * .8F, 0, 1); }

void GunAudio::update(float dt) {
    if (!std::isfinite(dt) || dt < 0) return;
    crash_wait_ = std::max(0.0F, crash_wait_ - dt);
    ringing_ = std::max(0.0F, ringing_ - dt);
    for (auto& p : pending_) p.delay -= dt;
    for (const auto& p : pending_) if (p.delay <= 0) play(*p.bank, p.volume, p.pan, p.pitch);
    std::erase_if(pending_, [](const Pending& p) { return p.delay <= 0; });
    if (pending_.size() > 256) pending_.erase(pending_.begin(), pending_.begin() + static_cast<long>(pending_.size() - 256));
}

} // namespace outland::engine::audio
