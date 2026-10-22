// The campaign: Uncle Ron's welcome (the quay, the drive, the house), the save slot file and how a
// slot reads on the campaign screen.
#include "outland/game/campaign/Campaign.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace outland::game::campaign;
namespace {
int failures = 0;
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main() {
    Story story;
    story.start(Stage::Arrival);
    check(story.intro() && !story.talking() && story.objective() == "Find Uncle Ron on the quay", "off the ship: find Uncle Ron");
    check(!story.board(), "no ride before he has talked");
    check(story.talk() && story.stage() == Stage::PortTalk && story.line_number() == 1, "he starts talking");
    check(story.take_changed(), "the stage moved on (a save)");
    bool esperanto = false, overview = false;
    for (int i = 0; i < story.lines(); ++i) {
        const auto* line = story.line();
        check(line && std::string(line->speaker) == Story::uncle, "Uncle Ron speaks");
        if (line) {
            esperanto |= line->text.find("You still remember Esperanto? Ahaha!") != std::string::npos;
            overview |= line->text.find("Espera") != std::string::npos && line->text.find("Roka") != std::string::npos;
        }
        story.talk();
    }
    check(esperanto, "\"You still remember Esperanto? Ahaha!\"");
    check(overview, "a word on the island's towns");
    check(story.stage() == Stage::ToTruck && !story.talking() && story.objective() == "Get in Uncle Ron's truck", "then: get in his truck");
    check(story.board() && story.stage() == Stage::Ride, "off home");
    // The drive: the screen goes black, the player is moved, it comes back at the house.
    bool moved = false;
    float darkest = 0;
    for (int i = 0; i < 100 && story.stage() == Stage::Ride; ++i) {
        story.update(.05F);
        moved |= story.take_teleport();
        darkest = std::max(darkest, story.fade());
        check(!moved || story.fade() > .2F || story.stage() != Stage::Ride || i > 30, "moved while dark");
    }
    check(moved && darkest >= .99F, "black screen, and the player moved home");
    check(story.stage() == Stage::HomeTalk && story.talking() && story.fade() == 0, "at the house, he talks again");
    bool brother = false, gang = false, home = false;
    while (story.stage() == Stage::HomeTalk) {
        const auto* line = story.line();
        if (line) {
            brother |= line->text.find("brother Dario") != std::string::npos;
            gang |= line->text.find("Green Hand") != std::string::npos;
            home |= line->text.find("home") != std::string::npos;
        }
        story.talk();
    }
    check(brother && gang && home, "his brother in the Green Hand, and a home");
    check(story.take_home_given() && story.stage() == Stage::Free && !story.intro(), "the house is the player's; the game begins");
    check(story.objective().empty(), "no goal set: free play");
    // Free play: a word from him, and off again.
    check(story.talk() && story.talking(), "a word from Uncle Ron");
    check(story.talk() && !story.talking(), "and done");
    // Every line is in English (ASCII letters bar the odd Esperanto word).
    for (const auto* set : {&Story::port_lines(), &Story::home_lines()})
        for (const auto& line : *set) check(line.text.size() > 20, "a real line");

    // A talk cut short by quitting starts again from its beginning.
    Story again;
    again.start(Stage::PortTalk);
    check(again.stage() == Stage::Arrival, "quit mid-talk on the quay: back to the quay");
    again.start(Stage::Ride);
    check(again.stage() == Stage::HomeTalk && again.talking(), "quit mid-drive: at the house");

    // The slot file.
    CampaignState state;
    state.stage = Stage::Free;
    state.has_position = true;
    state.position = {-244.5F, .2F, -71.25F};
    state.yaw = 135;
    state.played = 7500;
    state.saved_at = 1793000000;
    state.place = "Verda";
    state.ron_truck = "creator_vehicle_7";
    CampaignState back;
    check(parse(serialise(state), back), "the slot file reads back");
    check(back.stage == Stage::Free && back.has_position && std::abs(back.position.x + 244.5F) < .01F && std::abs(back.yaw - 135) < .01F &&
          back.played == 7500 && back.saved_at == 1793000000 && back.place == "Verda" && back.ron_truck == "creator_vehicle_7", "every field");
    check(!parse("rubbish", back), "rubbish is not a campaign");
    // On the campaign screen.
    const auto used = describe(2, serialise(state), true);
    check(used.used && used.title == "SLOT 2  ·  Verda" && used.detail.find("2 h 05 min played") != std::string::npos, "a used slot: " + used.detail);
    const auto fresh = describe(3, "", false);
    check(!fresh.used && fresh.detail.find("New game") != std::string::npos, "an empty slot: a new game");
    const auto old = describe(1, "", true);
    check(old.used && old.detail == "Saved game", "an older save without a campaign file");

    // Uncle Ron and the player's road: pleased with a trucker, overjoyed by a cop, ashamed of a
    // gangster - his words in free play change with it.
    {
        Story ron;
        ron.start(Stage::Free);
        ron.set_mood(Path::Cop, false);
        ron.talk();
        check(ron.line() && std::string(ron.line()->text).find("Officer") != std::string::npos, "a cop: Ron is over the moon");
        ron.talk();
        ron.set_mood(Path::Gang, false);
        ron.talk();
        check(ron.line() && std::string(ron.line()->text).find("Ahaha") == std::string::npos, "a gangster: no laughing");
        ron.talk();
        check(!Story::reaction_lines(Path::Trucker).empty() && !Story::reaction_lines(Path::Cop).empty() && !Story::reaction_lines(Path::Gang).empty(),
              "he has his say on each road");
        check(Story::reaction_lines(Path::None).empty(), "and nothing on none");
        // The talk out of turn: the reaction, line by line.
        check(ron.say(Story::reaction_lines(Path::Cop)) && ron.talking(), "his say");
        int lines = 0;
        while (ron.talking() && lines < 20) { ++lines; ron.talk(); }
        check(lines == static_cast<int>(Story::reaction_lines(Path::Cop).size()), "every line, then done");
        // The prison gate: the ultimatum, then the drive home (a fade, the player moved home).
        check(ron.say(Story::pickup_lines(true), true), "at the gate");
        const bool ultimatum = std::any_of(Story::pickup_lines(true).begin(), Story::pickup_lines(true).end(),
                                           [](const Line& l) { return l.text.find("leave my house") != std::string::npos; });
        check(ultimatum, "the ultimatum: leave the gang or leave my house");
        while (ron.talking()) ron.talk();
        check(ron.driving_home(), "then he drives the player home");
        bool moved = false;
        for (int k = 0; k < 40; ++k) { ron.update(.1F); moved |= ron.take_teleport(); }
        check(moved && !ron.driving_home() && ron.fade() == 0 && ron.stage() == Stage::Free, "home, and back to free play");
        ron.set_mood(Path::Gang, true);
        ron.talk();
        check(ron.line() && std::string(ron.line()->text).find("isn't your home") != std::string::npos, "shown the door: he says so");
        // All of it is saved with the slot.
        CampaignState state;
        state.stage = Stage::Free;
        state.heard_cop = state.heard_gang = true;
        state.picked_up = state.ultimatum = true;
        state.ultimatum_left = 640;
        CampaignState back;
        check(parse(serialise(state), back) && back.heard_cop && back.heard_gang && !back.heard_trucker && back.picked_up && back.ultimatum &&
              !back.kicked_out && std::abs(back.ultimatum_left - 640) < 1, "Ron's memory is saved");
    }

    if (failures) { std::cerr << failures << " failures\n"; return 1; }
    std::cout << "campaign ok\n";
    return 0;
}
