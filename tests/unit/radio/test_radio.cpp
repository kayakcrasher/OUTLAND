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
    check(titles.size() == 9, "nine different songs");

    // On air: ident, song 1, ident, song 2, ident, song 3, round again - by the clock alone.
    const auto& pop = stations[0];
    const float loop = loop_seconds(0);
    check(std::abs(loop - (3 * pop.ident_seconds + pop.songs[0].seconds + pop.songs[1].seconds + pop.songs[2].seconds)) < .01F, "a round is the songs and idents");
    auto on = on_air(0, 1.0);
    check(on.song == -1 && std::abs(on.offset - 1) < 1e-3F && std::string(on.file) == pop.ident, "the ident first");
    on = on_air(0, pop.ident_seconds + 10);
    check(on.song == 0 && std::abs(on.offset - 10) < 1e-3F && std::string(title(on)) == pop.songs[0].title, "then the first song, ten seconds in");
    on = on_air(0, 2 * pop.ident_seconds + pop.songs[0].seconds + 3);
    check(on.song == 1 && std::abs(on.offset - 3) < 1e-3F, "then the ident and the second song");
    on = on_air(0, loop + pop.ident_seconds + 10);
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
    song.update(true, rock.ident_seconds + rock.songs[0].seconds - 8, .1F);
    for (int k = 0; k < 60; ++k) song.update(true, rock.ident_seconds + rock.songs[0].seconds - 8 + k * .1, .1F);
    check(song.note() == 0 && song.now().song == 0, "the first song playing quietly");
    for (int k = 0; k < 100; ++k) song.update(true, rock.ident_seconds + rock.songs[0].seconds - 2 + k * .1, .1F);
    check(song.now().song == 1 && song.note() > 0 && song.note_title() == rock.songs[1].title, "the next song comes on and says so");
    song.set_station(9);
    check(song.station() == 2, "a station from the settings is kept in range");

    if (failures == 0) std::cout << "radio tests passed\n";
    return failures == 0 ? 0 : 1;
}
