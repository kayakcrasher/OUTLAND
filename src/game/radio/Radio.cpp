#include "outland/game/radio/Radio.hpp"
#include <algorithm>
#include <cmath>

namespace outland::game::radio {

float loop_seconds(int station) {
    if (station < 0 || station >= station_count) return 0;
    const auto& st = stations[static_cast<std::size_t>(station)];
    float total = 0;
    for (const auto& song : st.songs) total += st.ident_seconds + song.seconds;
    return total;
}

OnAir on_air(int station, double clock) {
    OnAir on;
    const float loop = loop_seconds(station);
    if (!(loop > 0) || !std::isfinite(clock)) return on;
    const auto& st = stations[static_cast<std::size_t>(station)];
    double t = std::fmod(clock, static_cast<double>(loop));
    if (t < 0) t += loop;
    on.station = station;
    for (int k = 0; k < static_cast<int>(st.songs.size()); ++k) {
        if (t < st.ident_seconds) {
            on.song = -1;
            on.offset = static_cast<float>(t);
            on.length = st.ident_seconds;
            on.file = st.ident;
            return on;
        }
        t -= st.ident_seconds;
        const auto& song = st.songs[static_cast<std::size_t>(k)];
        if (t < song.seconds || k + 1 == static_cast<int>(st.songs.size())) {
            on.song = k;
            on.offset = static_cast<float>(std::min(t, static_cast<double>(song.seconds)));
            on.length = song.seconds;
            on.file = song.file;
            return on;
        }
        t -= song.seconds;
    }
    return on;
}

const char* title(const OnAir& on) {
    if (on.station < 0 || on.station >= station_count) return "";
    const auto& st = stations[static_cast<std::size_t>(on.station)];
    return on.song >= 0 ? st.songs[static_cast<std::size_t>(on.song)].title : st.name;
}

void CarRadio::set_station(int station) {
    station_ = std::clamp(station, -1, station_count - 1);
}

void CarRadio::press() {
    station_ = station_ + 1 >= station_count ? -1 : station_ + 1;
    tuned_ = true;
    show();
}

void CarRadio::update(bool at_wheel, double clock, float dt) {
    if (at_wheel && !at_wheel_) show();
    at_wheel_ = at_wheel;
    if (!at_wheel) {
        note_ = 0;
        now_ = {};
        return;
    }
    const OnAir before = now_;
    now_ = station_ >= 0 ? on_air(station_, clock) : OnAir{};
    // A new song (not an ident) coming on says so.
    if (now_.song >= 0 && before.station == now_.station && before.song != now_.song) show();
    if (dt > 0) note_ = std::max(0.0F, note_ - dt);
}

float CarRadio::note() const {
    if (!at_wheel_ || note_ <= 0) return 0;
    const float shown = note_seconds - note_;
    return std::clamp(std::min(shown / .3F, note_ / .6F), 0.0F, 1.0F);
}

std::string CarRadio::note_station() const {
    if (station_ < 0) return "RADIO OFF";
    const auto& st = stations[static_cast<std::size_t>(station_)];
    return std::string(st.frequency) + "  " + st.name;
}

std::string CarRadio::note_title() const {
    if (station_ < 0) return "O / RADIO: on";
    return now_.song >= 0 ? title(now_) : std::string("from ") + stations[static_cast<std::size_t>(station_)].town;
}

} // namespace outland::game::radio
