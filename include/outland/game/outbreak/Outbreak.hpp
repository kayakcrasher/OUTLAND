#pragma once
#include <raylib.h>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace outland::world { class VerdaRegion; }

// The outbreak (Zombie Survival): Verda the morning it fell, and the player's story in it.
//
// Uncle Ron shakes the player awake in his house in Verda City: the city is gone, hurry. Dario -
// the only one of La Verda Mano to make it, his hideout overrun in minutes - waits at the barn on
// Ranĉo Verdaj Kampoj. The player drives Ron there in his pickup. Dario is outside the barn; the
// Garza family come out with their guns and tell them all to turn round or be shot, and Dario
// shoots them first. The ranchers get back up, infected, with the farmhands; the player clears
// the farm, and the barn is theirs: the hideout, and the ranchers' hunting rifle.
//
// After that the island is the player's to cross. Verda City is ground zero (the most infected,
// nobody's land). Holed up elsewhere: gangs still running whole towns, the Ĉefo with the last of
// the Policio behind the prison walls, Esperantist families in the forest, and the surviving
// truckers of the Brotherhood keeping a camp in Verda City - nobody knows who pays their bills.
// The black tower downtown with no windows and no doors belongs to the company behind it all.
// The map is red except where the player is (and the places they have made safe or found).
// Fuel is what the filling stations still hold: once a station is dry, it is done.
//
// This is the story's rules, the holdouts, the fuel and the save file; the game places the cast,
// runs the scenes and draws them.
namespace outland::game::outbreak {

enum class Stage : std::uint8_t {
    WakeUp,     // Ron shakes the player awake: listen to him
    ToTruck,    // get in his pickup (he gets in beside you)
    Drive,      // drive him to the barn at Ranĉo Verdaj Kampoj
    Standoff,   // the Garzas with their guns; Dario fires; they get back up
    Clear,      // kill the infected on the farm
    Free,       // the barn is the hideout; the island is the player's to cross
};
const char* stage_name(Stage stage);

struct Line { const char* speaker; const char* text; };

// Who is holed up where. Gangs still run their towns; the Policio and the families have walls
// or forest; the Brotherhood keeps a camp in Verda City.
enum class SiteKind : std::uint8_t { Town, Prison, Depot };
struct HoldoutSpec {
    std::string_view id;
    std::string_view name;        // on the map
    std::string_view faction;     // who they are
    std::string_view leader;      // who to talk to
    std::string_view settlement;  // the town it is in (or nearest)
    SiteKind site;
    int gang;                     // game::mafia::gangs index (their colours), -1 none
    Color color;
    float radius;                 // their ground: no infected inside
    int guards;
    std::array<const char*, 3> lines;   // what the leader says, in turn
};
inline constexpr std::array<HoldoutSpec, 7> holdouts{{
    {"lupoj", "Espera - the Wolves' town", "Montaj Lupoj", "Tomaso Lupo", "village_espera", SiteKind::Town, 1, {168, 176, 190, 255}, 120, 4,
     {"The Wolves still run Espera. We shot our own neighbours when they turned. We'll shoot you too if you bring it here.",
      "There's fuel in the hills if you know where to look. We don't share it.",
      "Verda City? Nobody goes in. Nobody that comes back right."}},
    {"ankro", "Porto Luma docks - the Anchor", "La Ora Ankro", "Kapitano Ines Moreno", "port_luma", SiteKind::Town, 2, {240, 196, 64, 255}, 140, 4,
     {"The Golden Anchor holds the docks. The sea's the only road that isn't full of them.",
      "Ships stopped coming the day it started. Like somebody told them to.",
      "You want passage off Verda? Everybody does. Nobody's got it."}},
    {"ranoj", "Suda Haveno - the Frogs", "Marĉaj Ranoj", "Jakobo Ranulo", "south_haven", SiteKind::Town, 3, {64, 178, 190, 255}, 120, 4,
     {"The Frogs hold Suda Haveno. The marsh slows the dead down. Slows everybody down.",
      "We trade. Shells for food, food for fuel. Bring something.",
      "Saw trucks on the ring road at night. Brotherhood rigs. Fresh paint. Who pays for paint now?"}},
    {"hundoj", "Roka - the Iron Dogs", "Feraj Hundoj", "Maks Ferro", "west_roka", SiteKind::Town, 4, {200, 86, 52, 255}, 120, 4,
     {"Roka's the Iron Dogs' now. The factory walls keep them out. Mostly.",
      "The Green Hand's gone, I heard. All but Dario. Figures he'd be the one.",
      "Something in Verda City woke this up. You don't get this many, this fast, by accident."}},
    {"stelo", "La Verda Stelo - families in the forest", "La Verda Stelo", "Klara Zamen", "ferarbaro_sovagejo", SiteKind::Town, -1, {120, 220, 140, 255}, 110, 3,
     {"Bonvenon, if you're alive. We're families - Esperantists, mostly. We keep to the trees.",
      "The children are safe here. Don't bring noise. Noise brings them.",
      "My husband worked security downtown. The black tower. He never said for who."}},
    {"policio", "Ŝtata Malliberejo - the last of the Policio", "Policio de Verda", "Ĉefo Marko Forta", "", SiteKind::Prison, -1, {70, 110, 220, 255}, 90, 4,
     {"The last of the Policio. We held the prison because it was built to keep people in - it keeps them out just as well.",
      "Verda City is gone. Ground zero. The first calls came from downtown, round the square.",
      "That black tower. We had no warrant for it. Never could get one. Now I know why."}},
    {"frateco", "The Brotherhood's camp", "The Trucking Brotherhood", "Ivo Kovač", "capital_verda", SiteKind::Depot, -1, {226, 150, 40, 255}, 70, 3,
     {"The Brotherhood keeps rolling. Somebody's got to.",
      "Who pays us? Somebody does. Diesel turns up. Money turns up. Don't ask, brother.",
      "We run the ring at night. If you hear air horns, it's us. If you hear anything else, run."}},
}};

// A holdout found in the world: its spec and where it is.
struct Holdout {
    int spec{-1};
    Vector3 centre{};
    float radius{0};
};
std::vector<Holdout> find_holdouts(const world::VerdaRegion& region);

// The black tower downtown (city kit glass_dark_tower: no windows, no doors) - the company's.
struct Tower { bool found{false}; Vector3 centre{}; Vector3 size{}; };
Tower find_tower(const world::VerdaRegion& region);
inline constexpr const char* company = "Helikso Biosistemoj";   // not shown until the story finds it

// Fuel: a filling station holds what it holds, in tankfuls. A station's stock is fixed by its
// place (some barely a can, none more than a few tanks); every fill comes out of it; once dry,
// it is done.
float initial_fuel(Vector3 station);

// The farm the story ends up on.
inline constexpr std::string_view ranch = "ranch_kampoj";

// What the save holds (beside the Zombie Survival loot profile: ".outbreak").
struct OutbreakState {
    Stage stage{Stage::WakeUp};
    bool has_position{false};
    Vector3 position{};               // feet, when saved in free play
    float yaw{0};
    std::string truck;                // Ron's pickup (vehicle id)
    std::vector<std::string> found;   // holdouts the player has come across (spec ids)
    std::vector<std::pair<int, float>> fuel;   // stations drawn from: (index, tankfuls left)
    int talks{0};                     // Dario's and Ron's words in free play, in turn
    bool tower_seen{false};
    std::vector<std::string> rigs;    // the Brotherhood's rigs on the road (vehicle ids)
    double played{0};
};
std::string serialise(const OutbreakState& state);
bool parse(const std::string& text, OutbreakState& state);

// A station's tankfuls left (its starting stock if nobody has drawn from it); take up to `want`
// tankfuls from it (returns what came out).
float fuel_left(const OutbreakState& state, int station, Vector3 at);
float draw_fuel(OutbreakState& state, int station, Vector3 at, float want);

// Is a point on the map known (not red): close to the player, the hideout or a found holdout.
struct Clearing { Vector3 centre{}; float radius{0}; };
std::vector<Clearing> clearings(const OutbreakState& state, Vector3 player, Vector3 hideout, const std::vector<Holdout>& found);
inline constexpr float sight_radius = 260;   // the clear circle round the player on the map

// The scenes.
enum class Cue : std::uint8_t { None, Fire, Rise, Claimed };
class Story {
public:
    static constexpr const char* ron = "Uncle Ron";
    static constexpr const char* dario = "Uncle Dario";
    static constexpr const char* rancher = "Rafael Garza";
    static constexpr int infected_on_farm = 9;         // the four Garzas and five farmhands
    static constexpr float line_seconds = 3.6F;        // a scene line on screen (USE skips)

    void start(Stage stage);
    [[nodiscard]] Stage stage() const { return stage_; }

    // USE: the next line while somebody is talking; in free play a word from whoever is near.
    bool talk();
    // The player is at the wheel with Ron aboard; the pickup is at the farm; the farm is clear.
    void boarded();
    void arrived();
    void cleared();
    void update(float dt);
    // Once each: Dario fires (the Garzas fall); they get back up (spawn the infected); the
    // barn is the hideout (the rifle, a save).
    Cue take_cue() { const Cue c = cue_; cue_ = Cue::None; return c; }
    bool take_changed() { const bool c = changed_; changed_ = false; return c; }

    [[nodiscard]] bool talking() const { return line_ >= 0; }
    [[nodiscard]] const Line* line() const;
    [[nodiscard]] std::string objective(int infected_left) const;
    // The screen at the start: black, coming up as Ron wakes the player (0..1 dark).
    [[nodiscard]] float fade() const;

    // A word in free play (Dario or Ron, in turn); a holdout leader's.
    static const Line& free_line(int turn);
    static const std::vector<Line>& wake_lines();
    static const std::vector<Line>& arrive_lines();
    static const std::vector<Line>& after_lines();
    static const std::vector<Line>& claim_lines();

private:
    void set(Stage stage) { stage_ = stage; changed_ = true; }
    void play(const std::vector<Line>& lines, bool timed);
    Stage stage_{Stage::WakeUp};
    const std::vector<Line>* lines_{nullptr};
    int line_{-1};
    bool timed_{false};
    float clock_{0}, wake_{0}, after_{0};
    int scene_{0};          // Standoff: 0 the talk, 1 after the shots, 2 rising
    Cue cue_{Cue::None};
    bool changed_{false};
};

} // namespace outland::game::outbreak
