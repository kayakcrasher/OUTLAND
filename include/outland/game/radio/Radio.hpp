#pragma once
#include "outland/game/radio/RadioCatalog.hpp"
#include "outland/game/radio/RadioVoices.hpp"
#include <string>

// Verda's radio: three stations, each from its own building in its own town -
//   98.1  Verda Popolko   (Verda City)  pop polka
//   102.5 Radio Sovaĝejo  (Sovaĝejo)    country and bluegrass
//   105.9 Espera Roko     (Espera)      rock
// - four or five instrumental songs each (tools/generate_radio_music.py), with the station's ident
// between them. The stations are live: each plays ident, song, ident, song... round and round,
// and what is on depends only on the clock, so tuning in drops you into whatever is playing.
// The idents are spoken (tools/generate_radio_voice.py): "Vi aŭskultas ..." over the jingle, a
// different one for the morning, the day, the evening and the night - and every other break, once
// La Verda Basballigo has a result, the sports news ("Roka venkis, Espera perdis"). Radio
// Sovaĝejo's announcer drawls.
// The player hears the radio at the wheel; O (or the RADIO button) changes the station, and after
// the last one switches it off.
namespace outland::game::radio {

inline constexpr int station_count = static_cast<int>(stations.size());

// What a station has on air: its ident (song -1) or a song, and how far into it.
struct OnAir {
    int station{-1};
    int song{-1};
    int slot{0};                // which break in the round (an ident before song `slot`)
    bool news{false};           // the break is the sports news
    float offset{0};
    float length{0};
    const char* file{""};
};

// One round of a station: its songs, each after the ident (voice_slot long), in seconds.
float loop_seconds(int station);
OnAir on_air(int station, double clock);
// "Dancu sur la Placo", or the station's name for an ident.
const char* title(const OnAir& on);
// The part of the day an ident is for: 0 morning (5-11), 1 day, 2 evening (17-22), 3 night.
int occasion(int hour);
// What a break says: the spoken ident for the hour, or (every other break, given a result: the
// winner and loser, game::sports::teams indices; -1 none) the sports news.
const char* break_clip(int station, int slot, int hour, int winner, int loser, bool* news = nullptr);

// The radio in the player's car.
class CarRadio {
public:
    static constexpr float note_seconds = 5.0F;

    // The station (-1 off), e.g. from the settings.
    void set_station(int station);
    [[nodiscard]] int station() const { return station_; }
    // The RADIO button: the next station; after the last, off; then the first again.
    void press();
    // Every frame: whether the player is at the wheel, the radio clock, the frame's time.
    void update(bool at_wheel, double clock, float dt);
    // What the breaks talk about: the island's hour, and the league's latest result (-1 none).
    void set_context(int hour, int winner, int loser) { hour_ = hour; winner_ = winner; loser_ = loser; }

    [[nodiscard]] bool at_wheel() const { return at_wheel_; }
    [[nodiscard]] bool playing() const { return at_wheel_ && station_ >= 0; }
    [[nodiscard]] const OnAir& now() const { return now_; }
    // The "now playing" note shown after getting in, tuning or when a song starts: how visible
    // (0..1) and its lines ("98.1  Verda Popolko", "Dancu sur la Placo", "RADIO OFF").
    [[nodiscard]] float note() const;
    [[nodiscard]] std::string note_station() const;
    [[nodiscard]] std::string note_title() const;
    // Once each: the dial was turned (a burst of static).
    bool take_tuned() { const bool t = tuned_; tuned_ = false; return t; }

private:
    void show() { note_ = note_seconds; }
    int station_{0};
    int hour_{12}, winner_{-1}, loser_{-1};
    const char* clip_{nullptr};   // this break's clip, chosen as it starts
    bool clip_news_{false};
    bool at_wheel_{false}, tuned_{false};
    OnAir now_{};
    float note_{0};
};

} // namespace outland::game::radio
