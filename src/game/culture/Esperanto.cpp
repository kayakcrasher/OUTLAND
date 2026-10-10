#include "outland/game/culture/Esperanto.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace outland::game::culture {
namespace {
template<std::size_t N> const char* pick(const std::array<const char*, N>& words, std::uint32_t seed) { return words[seed % N]; }

constexpr std::array<const char*, 12> shops{
    "VENDEJO", "NUTRAĴVENDEJO", "BAKEJO", "VIANDEJO", "FERAĴVENDEJO", "LIBREJO",
    "TABAKEJO", "FRIZEJO", "VESTAĴVENDEJO", "APOTEKO", "FRUKTOJ KAJ LEGOMOJ", "HORLOĜISTO"};
constexpr std::array<const char*, 10> pubs{
    "LA VERDA STELO", "LA FORGESITA INSULO", "LA RUĜA BOVO", "LA MALNOVA RADIO", "LA LACA FIŜKAPTISTO",
    "LA DORMANTA URSO", "LA BLUA ANKRO", "LA KOSMONAŬTO", "LA LASTA PRAMO", "ĈE LA HAVENO"};
constexpr std::array<const char*, 6> saints{"PETRO", "MIKAELO", "NIKOLAO", "ANDREO", "MARIA", "KLARA"};
constexpr std::array<const char*, 12> offices{
    "POŜTOFICEJO", "BANKO DE VERDA", "STATISTIKA OFICEJO", "ŜTATA ARKIVO", "RADIO VERDA",
    "METEOROLOGIA INSTITUTO", "MINISTERIO PRI FIŜKAPTADO", "INSTITUTO 47", "MINISTERIO PRI PACO",
    "LINGVA KOMITATO", "PATENTA OFICEJO", "ŜTATA KOOPERATIVO"};
constexpr std::array<const char*, 4> garages{"AŬTOMEKANIKEJO", "GARAĜO", "RIPAREJO", "BENZINSTACIO"};
constexpr std::array<const char*, 5> works{
    "FABRIKO N-RO 7", "KONSERVFABRIKO", "SEGEJO", "CEMENTFABRIKO", "LABORKOOPERATIVO"};
constexpr std::array<const char*, 3> docks{"HAVENO", "FIŜMERKATO", "HAVENOFICEJO"};
constexpr std::array<const char*, 12> streets{
    "ZAMENHOF-STRATO", "AVENUO DE LA PACO", "STRATO DE LA LABORISTOJ", "KOSMONAŬTA AVENUO",
    "BULVARDO VERDA STELO", "STRATO DE LA FONDINTOJ", "RADIOSTACIA STRATO", "STRATO DE LA EKSPERIMENTO",
    "BULVARDO DE LA ESPERO", "HAVENA STRATO", "LINGVO-STRATO", "STRATO DE LA 1-A DE MAJO"};

}

std::string upper(std::string_view text) {
    std::string out;
    for (int cp : utf8_codepoints(text)) {
        if (cp >= 'a' && cp <= 'z') cp -= 'a' - 'A';
        else if (cp >= 0xE0 && cp <= 0xFE && cp != 0xF7) cp -= 0x20;
        // Latin Extended-A pairs (ĉ → Ĉ, ž → Ž): capital first, on the even codepoint except in
        // the two runs where it falls on the odd one.
        else if (((cp >= 0x100 && cp <= 0x137) || (cp >= 0x14A && cp <= 0x177)) && cp % 2 == 1) cp -= 1;
        else if (((cp >= 0x139 && cp <= 0x148) || (cp >= 0x179 && cp <= 0x17E)) && cp % 2 == 0) cp -= 1;
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) {out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F));}
        else {out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F));}
    }
    return out;
}

SignText place_sign(life::PlaceKind kind, std::uint32_t seed, std::string_view owner, int ordinal, bool capital) {
    using K = life::PlaceKind;
    // Places of a kind in one town take consecutive names, so a town never has two of the same.
    seed += static_cast<std::uint32_t>(std::max(0, ordinal));
    const std::string at_owner = owner.empty() ? std::string() : "ĈE " + upper(owner);
    switch (kind) {
        case K::Shop: return {pick(shops, seed), at_owner};
        case K::Pub: return {pick(pubs, seed), "DRINKEJO"};
        case K::Church: return {"PREĜEJO", std::string("DE SANKTA ") + pick(saints, seed)};
        case K::Police: return {"POLICO", "RESPUBLIKO VERDA"};
        case K::Clinic: return {"KLINIKO", "URĜA HELPO"};
        case K::Office:
            if (ordinal == 0) return capital ? SignText{"REGISTARA PALACO", "RESPUBLIKO VERDA"} : SignText{"URBODOMO", ""};
            return {pick(offices, seed), ""};
        case K::Garage: return {pick(garages, seed), "BENZINO · OLEO · RADOJ"};
        case K::Industrial: return {pick(works, seed), "ENIRO NUR POR LABORISTOJ"};
        case K::Dock: return {pick(docks, seed), "NE FUMU"};
        case K::Farm: return {"KOLĤOZO", "«VERDA ESPERO»"};
        case K::Square: return {"URBOPLACO", ""};
        case K::Home: break;
    }
    return {};
}

std::string street_name(int index) {
    const int n = static_cast<int>(streets.size());
    return streets[static_cast<std::size_t>(((index % n) + n) % n)];
}

std::string format_distance(float metres) {
    char text[32];
    if (!std::isfinite(metres) || metres < 0) metres = 0;
    if (metres < 950) std::snprintf(text, sizeof text, "%d m", static_cast<int>(std::lround(metres / 50) * 50));
    else {
        const long tenths = std::lround(metres / 100);
        std::snprintf(text, sizeof text, "%ld,%ld km", tenths / 10, tenths % 10);
    }
    return text;
}

const std::vector<int>& sign_codepoints() {
    static const std::vector<int> codepoints = [] {
        std::vector<int> c;
        for (int i = 32; i < 127; ++i) c.push_back(i);
        for (int i = 160; i < 256; ++i) c.push_back(i);   // Latin-1: « » · and friends
        for (int i = 0x100; i < 0x180; ++i) c.push_back(i); // Latin Extended-A: Ĉĉ Ĝĝ Ĥĥ Ĵĵ Ŝŝ Ŭŭ, and Č Ć Ž in names
        for (const int i : {0x2013, 0x2014, 0x2190, 0x2191, 0x2192, 0x2193, 0x2605}) c.push_back(i); // – — ← ↑ → ↓ ★
        return c;
    }();
    return codepoints;
}

std::vector<int> utf8_codepoints(std::string_view text) {
    std::vector<int> out;
    for (std::size_t i = 0; i < text.size();) {
        const auto b = static_cast<unsigned char>(text[i]);
        int length = b < 0x80 ? 1 : (b >> 5) == 6 ? 2 : (b >> 4) == 14 ? 3 : (b >> 3) == 30 ? 4 : 0;
        if (length == 0 || i + static_cast<std::size_t>(length) > text.size()) {out.push_back(0xFFFD); ++i; continue;}
        int cp = length == 1 ? b : b & (0x7F >> length);
        for (int k = 1; k < length; ++k) cp = (cp << 6) | (static_cast<unsigned char>(text[i + static_cast<std::size_t>(k)]) & 0x3F);
        out.push_back(cp);
        i += static_cast<std::size_t>(length);
    }
    return out;
}

bool printable(std::string_view text) {
    const auto& known = sign_codepoints();
    for (const int cp : utf8_codepoints(text)) if (std::find(known.begin(), known.end(), cp) == known.end()) return false;
    return true;
}

std::uint32_t text_hash(std::string_view text) {
    std::uint32_t h = 2166136261U;
    for (const char c : text) {h ^= static_cast<unsigned char>(c); h *= 16777619U;}
    h ^= h >> 15; h *= 0x2c1b3c6dU; h ^= h >> 12;
    return h;
}
}
