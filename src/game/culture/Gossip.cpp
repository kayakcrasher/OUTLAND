#include "outland/game/culture/Gossip.hpp"
#include <algorithm>
#include <array>
#include <cctype>

namespace outland::game::culture {

namespace {
constexpr std::array<const char*, 6> openers{
    "Did you hear? ", "They're saying ", "Word is ", "My cousin swears ", "Everybody's talking about it: ", "Between us? "};
constexpr std::array<const char*, 4> about_you{
    "People are talking. ", "You didn't hear it from me, but ", "Keep your head down - ", "Funny thing: "};
constexpr std::array<const char*, 3> local{"Here in town ", "Right here ", "Just down the road "};

std::string capital(std::string s) {
    if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}
} // namespace

std::string Gossip::as_talk(std::string_view headline) {
    std::string text(headline);
    // Drop an all-caps tag ("HIT AND RUN: ", "FINAL: ") if there is one.
    if (const auto colon = text.find(": "); colon != std::string::npos && colon < 24) {
        bool shouting = true;
        for (std::size_t i = 0; i < colon; ++i) {
            const auto c = static_cast<unsigned char>(text[i]);
            if (std::islower(c)) { shouting = false; break; }
        }
        if (shouting) text = text.substr(colon + 2);
    }
    // A whole headline in capitals reads as shouting: say it in sentence case.
    bool lower = false;
    for (const char c : text) lower |= std::islower(static_cast<unsigned char>(c)) != 0;
    if (!lower) {
        for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    // " - " is a pause in a headline; in talk, a comma.
    for (std::size_t at = text.find(" - "); at != std::string::npos; at = text.find(" - ", at)) text.replace(at, 3, ", ");
    // A sentence's first word goes small in the middle of talk ("the Policio", "a car"); names keep theirs.
    for (const char* word : {"The ", "A ", "An ", "You ", "Your ", "Somebody ", "Someone "})
        if (text.rfind(word, 0) == 0) { text[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[0]))); break; }
    while (!text.empty() && (text.back() == '.' || text.back() == ' ')) text.pop_back();
    return text;
}

std::string Gossip::car(std::string_view paint, std::string_view model) {
    std::string out = "a ";
    if (!paint.empty()) {
        std::string p(paint);
        for (auto& c : p) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        out += p + " ";
    }
    out += model.empty() ? std::string("car") : std::string(model);
    if (out.size() > 2 && std::string_view("aeiouAEIOU").find(out[2]) != std::string_view::npos) out.insert(1, "n");
    return out;
}

void Gossip::forget(double now) {
    std::erase_if(rumours_, [&](const Rumour& r) { return now - r.at > keep_minutes; });
    while (rumours_.size() > keep) rumours_.erase(rumours_.begin());
}

void Gossip::hear(std::string_view text, double now, std::string town, bool you) {
    std::string said = as_talk(text);
    if (said.empty()) return;
    for (auto& r : rumours_)
        if (r.text == said && now - r.at < 60) { r.about_you |= you; return; }
    rumours_.push_back({std::move(said), now, std::move(town), you, 0});
    forget(now);
}

std::string Gossip::tell(double now, std::string_view town, std::uint32_t seed, std::string_view small_talk) {
    forget(now);
    // The best story: about the player first, then this town's, then the freshest; the least told.
    int best = -1;
    double score = -1e18;
    for (std::size_t i = 0; i < rumours_.size(); ++i) {
        const auto& r = rumours_[i];
        const double s = (r.about_you ? 4000 : 0) + (!town.empty() && r.town == town ? 2500 : 0) - (now - r.at) - r.told * 2500.0;
        if (s > score) { score = s; best = static_cast<int>(i); }
    }
    if (best < 0 || rumours_[static_cast<std::size_t>(best)].told >= 3) return std::string(small_talk);
    auto& r = rumours_[static_cast<std::size_t>(best)];
    ++r.told;
    std::string opener;
    if (r.about_you) opener = about_you[seed % about_you.size()];
    else if (!town.empty() && r.town == town) opener = local[seed % local.size()];
    else opener = openers[seed % openers.size()];
    // After a full stop or a question the story starts a sentence of its own.
    const bool fresh = opener.size() >= 2 && (opener[opener.size() - 2] == '.' || opener[opener.size() - 2] == '?');
    return opener + (fresh ? capital(r.text) : r.text) + ".";
}

} // namespace outland::game::culture
