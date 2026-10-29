// Uncle Ron and Uncle Dario on the phone: who rings about what, when, once each, and where it sends you.
#include "outland/game/campaign/Phone.hpp"
#include <iostream>
#include <string>

using namespace outland::game::campaign;
namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
} // namespace

int main() {
    PhoneContext c;
    c.race_meet = Vector3{1500, 0, 0};
    c.orthodox_church = Vector3{346, 0, 18};
    c.chapel = Vector3{-373, 0, -21};
    c.ron_home = Vector3{-300, 0, 40};
    c.hideout = Vector3{-60, 0, 80};
    Phone phone;
    check(!phone.choose(c), "nothing to say yet");
    c.owns_car = true;
    c.hour = 20;
    auto call = phone.choose(c);
    check(call && call->topic == "ron_race" && call->caller == "Uncle Ron" && call->target && call->target->x == 1500, "Ron tells of the races, with the way to the meet");
    c.hour = 12;
    check(!phone.choose(c), "the races are a night thing");

    // The phone rings after a few minutes of play, then the lines go by (USE or in time).
    c.hour = 20;
    for (float t = 0; t < Phone::first_call - 1; t += .5F) phone.update(.5F, c);
    check(!phone.on_call(), "nobody rings at first");
    phone.update(2, c);
    check(phone.ringing() && phone.take_ring() && !phone.take_ring() && phone.line().empty(), "it rings");
    phone.update(Phone::ring_seconds + .1F, c);
    check(!phone.ringing() && phone.line().find("Uncle Ron (phone)") == 0, "answered: " + phone.line());
    check(phone.next() && phone.on_call() && phone.next() && !phone.on_call(), "USE goes through the lines and hangs up");
    const auto ended = phone.take_ended();
    check(ended && ended->topic == "ron_race" && ended->target && phone.said("ron_race"), "the call ends with the waypoint, said once");
    check(!phone.choose(c), "not again");
    // The next call waits an hour of real play, however much there is to say.
    c.sunday = true;
    c.hour = 8;
    for (float t = 0; t < 3590; t += 1) phone.update(1, c);
    check(!phone.on_call(), "no call within the hour");
    for (float t = 0; t < 200 && !phone.on_call(); t += 1) phone.update(1, c);
    check(phone.on_call() && phone.call()->topic == "dario_chapel", "the next call an hour on");
    while (phone.on_call()) phone.next();
    phone.take_ended();
    phone.set_done({"ron_race"});
    c.sunday = false;

    // Sundays, feasts, the rumour, the badge, the family.
    c.sunday = true;
    c.hour = 8;
    call = phone.choose(c);
    check(call && call->topic == "dario_chapel" && call->target && call->target->x < 0, "Dario: chapel at half nine");
    c.feast_tomorrow = 2;
    call = phone.choose(c);
    check(call && call->caller == "Uncle Ron" && call->lines.front().find("Pasko") != std::string::npos, "a feast tomorrow comes first");
    c.rumour = "Somebody in a racing red Verda Tempesto ran a man down in Roka.";
    call = phone.choose(c);
    check(call && call->topic == "ron_rumour" && !call->target && call->lines.front().find("Tempesto") != std::string::npos, "what people are saying comes first of all");
    c.rumour.clear();
    c.feast_tomorrow = -1;
    c.cop = true;
    call = phone.choose(c);
    check(call && call->topic == "dario_badge", "Dario on the badge");
    c.cop = false;
    c.race_wins = 1;
    c.sunday = false;
    phone.set_done({"ron_sunday", "dario_chapel"});
    call = phone.choose(c);
    check(call && call->topic == "dario_race" && call->target && call->target->x == -60, "a race won: Dario wants a driver");
    c.green_hand = true;
    call = phone.choose(c);
    check(call && call->topic == "dario_family", "in the family");
    // Nothing rings while the player is busy.
    Phone busy;
    c.busy = true;
    for (int i = 0; i < 2000; ++i) busy.update(1, c);
    check(!busy.on_call(), "it waits while the player is busy");

    if (failures == 0) std::cout << "phone ok\n";
    return failures == 0 ? 0 : 1;
}
