// The campaign: Uncle Ron's welcome (the quay, the drive, the house), the save slot file and how a
// slot reads on the campaign screen.
#include "outland/game/campaign/Campaign.hpp"
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

    // In Esperanto (SETTINGS > Language): every line has its translation, the goals too.
    check(Story::port_lines(true).size() == Story::port_lines().size() && Story::home_lines(true).size() == Story::home_lines().size() &&
          Story::idle_lines(true).size() == Story::idle_lines().size(), "a translation for every line");
    {
        Story eo;
        eo.set_esperanto(true);
        eo.start(Stage::Arrival);
        check(eo.objective() == "Trovu Onklon Ron sur la kajo", "the goal in Esperanto");
        eo.talk();
        eo.talk();
        check(eo.line() && eo.line()->text.find("Ĉu vi ankoraŭ memoras Esperanton? Ahaha!") != std::string::npos &&
              std::string(eo.line()->speaker) == "Onklo Ron", "Onklo Ron speaks Esperanto");
    }

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

    if (failures) { std::cerr << failures << " failures\n"; return 1; }
    std::cout << "campaign ok\n";
    return 0;
}
