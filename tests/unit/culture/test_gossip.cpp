// Word gets around: headlines become talk, the player's doings are told first, old news goes;
// and Sundays and feast days at the churches.
#include "outland/game/culture/Faith.hpp"
#include "outland/game/culture/Gossip.hpp"
#include <iostream>
#include <string>

using namespace outland::game::culture;
namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
} // namespace

int main() {
    // Headlines into talk.
    check(Gossip::as_talk("HIT AND RUN: a car hit someone and drove on") == "a car hit someone and drove on", "the tag goes");
    check(Gossip::as_talk("Feraj Hundoj took Roka from Marĉaj Ranoj") == "Feraj Hundoj took Roka from Marĉaj Ranoj", "names keep their capitals");
    check(Gossip::as_talk("THE CHIEF IS DEAD - LA VERDA MANO TAKE VERDA") == "the chief is dead, la verda mano take verda", "shouting in sentence case");
    check(Gossip::car("Racing red", "Verda Tempesto") == "a racing red Verda Tempesto" && Gossip::car("", "Verda Sedano") == "a Verda Sedano" &&
          Gossip::car("", "Old hatchback") == "an Old hatchback", "the player's car, described");
    check(Gossip::as_talk("The Policio broke up the Frogs") == "the Policio broke up the Frogs", "a sentence's first word goes small");

    Gossip gossip;
    check(gossip.tell(0, "west_roka", 1, "Nice weather.") == "Nice weather.", "nothing new: small talk");
    gossip.hear("FINAL: Roka 5, Espera 2", 10);
    gossip.hear("FINAL: Roka 5, Espera 2", 30);   // the same story twice is heard once
    check(gossip.rumours().size() == 1, "one story, once");
    gossip.hear("Feraj Hundoj took Suda Haveno from Marĉaj Ranoj", 40, "south_haven");
    gossip.hear("somebody in a racing red Verda Tempesto ran a man down in Roka and never stopped", 50, "west_roka", true);
    const auto first = gossip.tell(60, "south_haven", 2, "");
    check(has(first, "racing red Verda Tempesto"), "what the player did is told first: " + first);
    const auto second = gossip.tell(61, "south_haven", 2, "");
    check(has(second, "Suda Haveno"), "then this town's news: " + second);
    for (int i = 0; i < 10; ++i) gossip.tell(62 + i, "south_haven", static_cast<std::uint32_t>(i), "");
    check(gossip.tell(80, "south_haven", 3, "Quiet week.") == "Quiet week.", "a story told three times is worn out");
    gossip.hear("a new story", 100);
    check(gossip.tell(100 + Gossip::keep_minutes + 1, "", 0, "Old news.") == "Old news.", "old news is forgotten");

    // Sundays and feast days.
    Church chapel;
    chapel.denomination = Denomination::Baptist;
    chapel.green_hand = true;
    Church church;
    church.denomination = Denomination::Orthodox;
    const int plain = 1;   // not a feast day
    check(bells(true, plain, 9 * 60 + 20) && bells(true, plain, 11 * 60 + 35) && !bells(true, plain, 10 * 60) && !bells(false, plain, 9 * 60 + 20),
          "Sunday bells before and after the service");
    check(bells(false, 0, 12 * 60 + 5) && !bells(false, plain, 12 * 60 + 5), "noon bells on a feast");
    const int pasko = 2 * feast_days;
    check(bells(false, pasko, 10) && gathering(church, false, pasko, 30).candles, "Pasko: bells and candles at midnight");
    const auto sunday = gathering(chapel, true, plain, 10 * 60);
    check(sunday.people > 0 && sunday.green_hand > 0, "Sunday: the faithful, and La Verda Mano outside their chapel");
    check(gathering(church, true, plain, 10 * 60).green_hand == 0 && gathering(church, false, plain, 10 * 60).people == 0, "no Green Hand at the Orthodox church; nobody on a weekday");
    check(gathering(church, false, feast_days, 14 * 60).people > gathering(chapel, false, feast_days, 14 * 60).people, "Teofanio is the Orthodox church's day");
    check(day_of_peace(0) && !day_of_peace(plain) && has(feast_morning(0), "Kristnasko") && feast_morning(plain).empty(), "feast days are days of peace");

    if (failures == 0) std::cout << "gossip ok\n";
    return failures == 0 ? 0 : 1;
}
