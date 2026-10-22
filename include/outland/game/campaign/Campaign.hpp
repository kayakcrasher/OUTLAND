#pragma once
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

// The campaign (Explore): the player's story on Verda, and each save slot's place in it.
//
// The player comes home to Verda after years abroad in Eastern Europe. Born and raised in Verda
// City, he has family in La Verda Mano (the Green Hand). His Uncle Ron - a working-class
// Esperantist, no friend of the gangs, sad that his own brother Dario leads the Green Hand - picks
// him up at the cargo terminal in Porto Luma, tells him about the island on the
// quay, drives him home to Verda City and gives him a home in his house in Okcidenta Verda. From
// there the game is the player's.
//
// The dialogue is in English; the island itself (its signs and places) is in Esperanto.
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

// The road the player takes, as Uncle Ron sees it.
enum class Path : std::uint8_t { None, Trucker, Cop, Gang };

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
    // Uncle Ron and the player's choices.
    bool heard_trucker{false}, heard_cop{false}, heard_gang{false};   // he has had his say about each
    bool picked_up{false};            // he came for the player at the prison gate (once only)
    bool ultimatum{false};            // "leave the gang, or leave my house"
    float ultimatum_left{0};          // seconds of play before he means it
    bool kicked_out{false};           // he meant it: the house is no longer the player's
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
SlotInfo describe(int slot, const std::string& campaign_text, bool any_save);

struct Line { const char* speaker; std::string text; };

class Story {
public:
    // How long the ultimatum gives the player to leave the gang (seconds of play).
    static constexpr float ultimatum_seconds = 1200;
    static constexpr const char* uncle = "Uncle Ron";
    static constexpr const char* brother = "Dario";   // Uncle Dario, head of La Verda Mano
    static constexpr float fade_in = 1.2F, hold = .6F, fade_out = 1.2F;

    void start(Stage stage);
    [[nodiscard]] Stage stage() const { return stage_; }
    [[nodiscard]] bool intro() const { return stage_ != Stage::Free; }

    // USE beside Uncle Ron: starts his talk, or the next line; in free play, a word from him.
    // Returns true when it did something.
    bool talk();
    // The player walked away from Ron mid-word: what he was saying is left unsaid (Free only).
    void hush();
    // USE at his truck once he is done talking on the quay: off home.
    bool board();
    void update(float dt);

    // The line on screen ("" when none), whose it is, and where in the talk it is.
    [[nodiscard]] bool talking() const { return line_ >= 0 || special_ >= 0; }
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

    static const std::vector<Line>& port_lines();
    static const std::vector<Line>& home_lines();
    static const std::vector<Line>& idle_lines();
    // A word in free play, as the player's road stands (and whether he showed them the door).
    static const std::vector<Line>& idle_lines(Path path, bool kicked_out);
    // His say on a road the player has just taken (once each).
    static const std::vector<Line>& reaction_lines(Path path);
    // At the prison gate (the one time he comes): the ultimatum if the player runs with a gang.
    static const std::vector<Line>& pickup_lines(bool gang);
    // The ultimatum ran out; and the player who left the gang after all.
    static const std::vector<Line>& kicked_out_lines();
    static const std::vector<Line>& welcome_back_lines();

    // A talk out of turn (in free play): the lines play one by one with USE. Returns false while
    // the story's own talk is on.
    bool say(const std::vector<Line>& lines, bool then_drive_home = false);
    [[nodiscard]] bool saying() const { return special_ >= 0; }
    // Uncle Ron drives the player home (a fade, as on the first day): from the prison gate.
    void drive_home();
    [[nodiscard]] bool driving_home() const { return homeward_; }
    // The road the player is on and whether he showed them the door: his words in free play.
    void set_mood(Path path, bool kicked_out) { path_ = path; kicked_out_ = kicked_out; }

private:
    void set(Stage stage) { stage_ = stage; changed_ = true; }
    Stage stage_{Stage::Arrival};
    int line_{-1};
    std::vector<Line> lines_;          // the talk out of turn
    int special_{-1};
    bool then_home_{false}, homeward_{false};
    Path path_{Path::None};
    bool kicked_out_{false};
    int idle_{0};
    float ride_{0};
    bool teleport_{false}, home_given_{false}, changed_{false};
};

} // namespace outland::game::campaign
