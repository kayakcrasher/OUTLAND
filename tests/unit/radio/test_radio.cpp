// Verda's radio: three live stations with three songs each and an ident between them, the files
// on disk, what is on air at any time, and the car radio's dial and "now playing" note.
#include "outland/game/radio/Radio.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
using namespace outland::game::radio;
namespace {
int failures = 0;
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main() {
    // The catalogue: Verda City's pop polka, Sovaĝejo's country, Espera's rock - three songs each.
    check(station_count == 3, "three stations");
    check(std::string(stations[0].town) == "Verda City" && std::string(stations[0].genre) == "Pop polka", "Verda City plays pop polka");
    check(std::string(stations[1].town) == "Sovaĝejo" && std::string(stations[1].genre).find("bluegrass") != std::string::npos, "Sovaĝejo plays country and bluegrass");
    check(std::string(stations[2].town) == "Espera" && std::string(stations[2].genre) == "Rock", "Espera plays rock");
    std::set<std::string> titles;
    for (const auto& st : stations) {
        check(std::filesystem::exists(std::string(OUTLAND_SOURCE_DIR) + "/" + st.ident), std::string("ident on disk: ") + st.ident);
        check(st.ident_seconds > 2 && st.ident_seconds < 10, "a short ident");
        for (const auto& song : st.songs) {
            check(std::filesystem::exists(std::string(OUTLAND_SOURCE_DIR) + "/" + song.file), std::string("song on disk: ") + song.file);
            check(song.seconds > 90 && song.seconds < 240, std::string("a full song: ") + song.title);
            titles.insert(song.title);
        }
    }
    check(titles.size() == 14, "fourteen different songs");
    check(stations[0].songs.size() == 4 && stations[1].songs.size() == 5 && stations[2].songs.size() == 5, "the playlists: 4 pop polka, 5 country, 5 rock");

    // On air: ident, song 1, ident, song 2, ident, song 3, round again - by the clock alone.
    const auto& pop = stations[0];
    const float loop = loop_seconds(0);
    float round = 0;
    for (const auto& song : pop.songs) round += voice_slot + song.seconds;
    check(std::abs(loop - round) < .01F, "a round is the songs and idents");
    auto on = on_air(0, 1.0);
    check(on.song == -1 && std::abs(on.offset - 1) < 1e-3F && std::string(on.file) == pop.ident, "the ident first");
    on = on_air(0, voice_slot + 10);
    check(on.song == 0 && std::abs(on.offset - 10) < 1e-3F && std::string(title(on)) == pop.songs[0].title, "then the first song, ten seconds in");
    on = on_air(0, 2 * voice_slot + pop.songs[0].seconds + 3);
    check(on.song == 1 && std::abs(on.offset - 3) < 1e-3F, "then the ident and the second song");
    on = on_air(0, loop + voice_slot + 10);
    check(on.song == 0 && std::abs(on.offset - 10) < 1e-2F, "and round again");
    check(on_air(1, 5000).station == 1 && on_air(-1, 5).station == -1 && on_air(3, 5).station == -1, "only real stations");
    check(on_air(2, -30).song >= -1 && on_air(2, -30).offset >= 0, "a clock before zero is still a time of day");

    // The car radio: silent on foot; at the wheel it plays the station; the button turns the dial.
    CarRadio radio;
    radio.set_station(0);
    radio.update(false, 100, .1F);
    check(!radio.playing() && radio.note() == 0, "nothing on foot");
    radio.update(true, 100, .1F);
    check(radio.playing() && radio.now().station == 0 && radio.note() > 0, "in the car: Verda Popolko, with a note");
    check(radio.note_station().find("98.1") != std::string::npos && radio.note_station().find("Verda Popolko") != std::string::npos, "the note names the station");
    for (int k = 0; k < 80; ++k) radio.update(true, 100 + k * .1, .1F);
    check(radio.note() == 0, "the note goes after a few seconds");
    radio.press();
    radio.update(true, 110, .1F);
    check(radio.station() == 1 && radio.now().station == 1 && radio.take_tuned() && !radio.take_tuned() && radio.note() > 0, "RADIO: the next station, with static and a note");
    radio.press();
    radio.press();
    radio.update(true, 110, .1F);
    check(radio.station() == -1 && !radio.playing() && radio.note_station() == "RADIO OFF", "after the last station: off");
    radio.press();
    check(radio.station() == 0, "then the first again");
    // A new song coming on shows the note again.
    CarRadio song;
    song.set_station(2);
    const auto& rock = stations[2];
    song.update(true, voice_slot + rock.songs[0].seconds - 8, .1F);
    for (int k = 0; k < 60; ++k) song.update(true, voice_slot + rock.songs[0].seconds - 8 + k * .1, .1F);
    check(song.note() == 0 && song.now().song == 0, "the first song playing quietly");
    for (int k = 0; k < 100; ++k) song.update(true, voice_slot + rock.songs[0].seconds - 2 + k * .1, .1F);
    check(song.now().song == 1 && song.note() > 0 && song.note_title() == rock.songs[1].title, "the next song comes on and says so");
    song.set_station(9);
    check(song.station() == 2, "a station from the settings is kept in range");

    // The breaks are spoken: an ident for the time of day, on disk, and the same length as the slot.
    check(occasion(7) == 0 && occasion(13) == 1 && occasion(19) == 2 && occasion(23) == 3 && occasion(2) == 3, "morning, day, evening, night");
    for (int s = 0; s < station_count; ++s)
        for (int o = 0; o < occasions; ++o) {
            const std::string clip = voice_idents[static_cast<std::size_t>(s)][static_cast<std::size_t>(o)];
            check(std::filesystem::exists(std::string(OUTLAND_SOURCE_DIR) + "/" + clip), "spoken ident on disk: " + clip);
        }
    for (std::size_t i = 0; i < news_clips.size(); ++i) {
        const bool same = i / 5 == i % 5;
        check(same == (news_clips[i][0] == 0) && same == (news_clips_drawl[i][0] == 0), "a news clip for every pairing");
        if (!same)
            check(std::filesystem::exists(std::string(OUTLAND_SOURCE_DIR) + "/" + news_clips_drawl[i]), std::string("news on disk: ") + news_clips_drawl[i]);
    }
    check(drawl[1] && !drawl[0] && !drawl[2] && std::string(stations[1].name) == "Radio Sovaĝejo", "Radio Sovaĝejo drawls");
    bool news = true;
    check(std::string(break_clip(0, 1, 8, -1, -1, &news)) == voice_idents[0][0] && !news, "no result yet: the morning ident");
    check(std::string(break_clip(0, 2, 20, 4, 1, &news)) == voice_idents[0][2] && !news, "even breaks: the ident");
    check(std::string(break_clip(0, 1, 20, 4, 1, &news)) == news_clips[21] && news, "odd breaks: Roka venkis, Espera perdis");
    check(std::string(break_clip(1, 3, 20, 4, 1)) == news_clips_drawl[21], "on Radio Sovaĝejo, drawled");
    CarRadio night;
    night.set_station(0);
    night.set_context(23, 2, 0);
    const float first = voice_slot + stations[0].songs[0].seconds;   // the second break (slot 1) starts here
    night.update(true, first + 1, .1F);
    check(night.now().song == -1 && night.now().news && std::string(night.now().file) == news_clips[10] && night.note_title() == "Sportaj novaĵoj",
          "the news on air in the second break");
    night.set_context(5, -1, -1);
    night.update(true, first + 2, .1F);
    check(std::string(night.now().file) == news_clips[10], "a break doesn't change its clip half way through");
    night.update(true, 1, .1F);
    check(std::string(night.now().file) == voice_idents[0][0], "the next break: the morning ident");

    if (failures == 0) std::cout << "radio tests passed\n";
    return failures == 0 ? 0 : 1;
}
