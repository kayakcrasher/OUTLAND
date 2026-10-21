#include "outland/game/radio/RadioAudio.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace outland::game::radio {
namespace {
std::string asset(const char* relative) {
    const std::string packaged = std::string(GetApplicationDirectory()) + relative;
    return FileExists(packaged.c_str()) ? packaged : relative;
}

// A third of a second of band-limited hiss with a whistle through it: the dial turning.
Sound make_static() {
    constexpr int rate = 22050, count = rate / 3;
    std::vector<std::int16_t> pcm(count);
    std::uint32_t seed = 2024;
    float smooth = 0;
    for (int i = 0; i < count; ++i) {
        seed = seed * 1664525U + 1013904223U;
        const float white = static_cast<float>(seed >> 8) / 8388608.0F - 1.0F;
        smooth = smooth * .55F + white * .45F;
        const float t = static_cast<float>(i) / rate;
        const float whistle = .15F * std::sin(6.2832F * (900 + 2400 * t) * t);
        const float fade = std::min({1.0F, t / .01F, (static_cast<float>(count - i) / rate) / .08F});
        pcm[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(std::clamp((smooth * .6F + whistle) * fade, -1.0F, 1.0F) * 20000);
    }
    Wave wave{static_cast<unsigned int>(count), rate, 16, 1, pcm.data()};
    return LoadSoundFromWave(wave);
}
} // namespace

RadioAudio::~RadioAudio() {
    unload();
    if (static_.stream.buffer) UnloadSound(static_);
}

void RadioAudio::unload() {
    if (music_.stream.buffer) {
        StopMusicStream(music_);
        UnloadMusicStream(music_);
    }
    music_ = {};
    file_.clear();
}

void RadioAudio::stop() { unload(); }

void RadioAudio::update(CarRadio& radio, bool paused, float volume) {
    if (!IsAudioDeviceReady()) return;
    if (!initialized_) {
        initialized_ = true;
        static_ = make_static();
    }
    volume = std::clamp(volume, 0.0F, 1.0F);
    if (radio.take_tuned() && radio.at_wheel() && !paused && static_.stream.buffer) {
        SetSoundVolume(static_, .35F * volume);
        PlaySound(static_);
    }
    if (!radio.playing()) {
        // Out of the car (or switched off): nothing streams.
        unload();
        return;
    }
    if (paused) {
        if (music_.stream.buffer && IsMusicStreamPlaying(music_)) PauseMusicStream(music_);
        return;
    }
    const auto& now = radio.now();
    if (now.file == nullptr || now.file[0] == '\0') { unload(); return; }
    bool seek = false;
    if (file_ != now.file) {
        unload();
        const auto path = asset(now.file);
        if (!FileExists(path.c_str())) return;
        music_ = LoadMusicStream(path.c_str());
        if (!music_.stream.buffer) { music_ = {}; return; }
        music_.looping = false;
        file_ = now.file;
        volume_ = -1;
        PlayMusicStream(music_);
        seek = now.offset > .25F;
    } else if (!IsMusicStreamPlaying(music_)) {
        ResumeMusicStream(music_);
        if (!IsMusicStreamPlaying(music_)) PlayMusicStream(music_);
        seek = true;
    }
    // Keep with the station's clock: tuned in late, back from a pause, or drifted.
    if (seek || std::abs(GetMusicTimePlayed(music_) - now.offset) > 1.5F)
        SeekMusicStream(music_, std::clamp(now.offset, 0.0F, std::max(0.0F, GetMusicTimeLength(music_) - .1F)));
    if (volume != volume_) {
        volume_ = volume;
        SetMusicVolume(music_, volume);
    }
    UpdateMusicStream(music_);
}

} // namespace outland::game::radio
