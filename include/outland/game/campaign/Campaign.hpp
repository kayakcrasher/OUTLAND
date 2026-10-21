#pragma once
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

// The campaign (Explore): the player's story on Verda, and each save slot's place in it.
//
// The player comes home to Verda after years abroad in Eastern Europe. Born and raised in Verda
// City, he has family in La Verda Mano (the Green Hand). His Uncle Ron - a working-class
// Esperantist, no friend of the gangs, sad that his own brother Dario is a big man in the Green
// Hand - picks him up at the cargo terminal in Porto Luma, tells him about the island on the
// quay, drives him home to Verda City and gives him a home in his house in Okcidenta Verda. From
// there the game is the player's.
//
// The dialogue is in English, or in Esperanto when that is the language chosen in SETTINGS
// (set_esperanto); the island's signs and places are in Esperanto either way.
// This is the story's rules and the slot file; the game places Uncle Ron, his truck and the
// player, shows the lines, fades the screen and saves.
namespace outland::game::campaign {

enum class Stage : std::uint8_t {
    Arrival,    // on the quay at Porto Luma, just off the ship: find Uncle Ron
    PortTalk,   // Uncle Ron on the island
    ToTruck,    // get in his truck
    Ride,       // the drive home (a fade)
    HomeTalk,   // at his house in Verda City: the family, the gangs, the house
    Free,       // the game is the player's
};

// What a slot holds besides the inventory, career and the rest (beside its profile: ".campaign").
struct CampaignState {
    Stage stage{Stage::Arrival};
    bool has_position{false};
    Vector3 position{};               // where the player was when it was saved (feet)
    float yaw{0};                     // degrees
    double played{0};                 // seconds in this campaign
    std::int64_t saved_at{0};         // unix time
    std::string place;                // the nearest town when saved
    std::string ron_truck;            // Uncle Ron's pickup (vehicle id)
};
std::string serialise(const CampaignState& state);
bool parse(const std::string& text, CampaignState& state);

// A slot on the campaign screen.
struct SlotInfo {
    bool used{false};
    std::string title;                // "SLOT 1 · Verda City"
    std::string detail;               // "2 h 05 min played · saved 21 Oct 19:40" / "New game"
};
// From the slot's campaign file (empty if none) and whether it has any save at all (an older
// save without a campaign file is a game well under way).
SlotInfo describe(int slot, const std::string& campaign_text, bool any_save, bool esperanto = false);

struct Line { const char* speaker; std::string text; };

class Story {
public:
    static constexpr const char* uncle = "Uncle Ron";
    static constexpr const char* brother = "Dario";
    static constexpr float fade_in = 1.2F, hold = .6F, fade_out = 1.2F;

    void start(Stage stage);
    // The language he speaks (and the goals and captions are shown in).
    void set_esperanto(bool esperanto) { esperanto_ = esperanto; }
    [[nodiscard]] bool esperanto() const { return esperanto_; }
    [[nodiscard]] Stage stage() const { return stage_; }
    [[nodiscard]] bool intro() const { return stage_ != Stage::Free; }

    // USE beside Uncle Ron: starts his talk, or the next line; in free play, a word from him.
    // Returns true when it did something.
    bool talk();
    // USE at his truck once he is done talking on the quay: off home.
    bool board();
    void update(float dt);

    // The line on screen ("" when none), whose it is, and where in the talk it is.
    [[nodiscard]] bool talking() const { return line_ >= 0; }
    [[nodiscard]] const Line* line() const;
    [[nodiscard]] int line_number() const { return line_ + 1; }
    [[nodiscard]] int lines() const;
    // The goal shown on screen ("" when none).
    [[nodiscard]] std::string objective() const;
    // The drive home: how dark the screen is (0..1), and what it says.
    [[nodiscard]] float fade() const;
    [[nodiscard]] const char* caption() const;

    // Once each: the screen is black (move the player, Uncle Ron and the truck home); the house
    // was handed over; the stage moved on (save the game).
    bool take_teleport() { const bool t = teleport_; teleport_ = false; return t; }
    bool take_home_given() { const bool h = home_given_; home_given_ = false; return h; }
    bool take_changed() { const bool c = changed_; changed_ = false; return c; }

    static const std::vector<Line>& port_lines(bool esperanto = false);
    static const std::vector<Line>& home_lines(bool esperanto = false);
    static const std::vector<Line>& idle_lines(bool esperanto = false);

private:
    void set(Stage stage) { stage_ = stage; changed_ = true; }
    Stage stage_{Stage::Arrival};
    int line_{-1};
    int idle_{0};
    float ride_{0};
    bool teleport_{false}, home_given_{false}, changed_{false}, esperanto_{false};
};

} // namespace outland::game::campaign
