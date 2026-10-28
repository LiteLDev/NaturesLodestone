#include "core/BiomeText.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <set>
#include <system_error>

namespace nl::core {

namespace {

bool isUpper(char c) { return c >= 'A' && c <= 'Z'; }
bool isLower(char c) { return c >= 'a' && c <= 'z'; }

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return s;
}

bool has(std::vector<std::string> const& tags, std::string_view tag) {
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

} // namespace

std::string_view idNamespace(std::string_view id) {
    auto colon = id.find(':');
    return colon == std::string_view::npos ? std::string_view{"minecraft"} : id.substr(0, colon);
}

std::string_view idPath(std::string_view id) {
    auto colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

std::string fullBiomeId(std::string_view id) {
    if (id.find(':') != std::string_view::npos) return std::string(id);
    return "minecraft:" + std::string(id);
}

std::string fixBiomeName(std::string_view name) {
    std::string out;
    out.reserve(name.size() + 4);
    for (size_t i = 0; i < name.size(); ++i) {
        if (i > 0 && isUpper(name[i]) && isLower(name[i - 1])) out += ' ';
        out += name[i];
    }
    return out;
}

std::string capitalizePath(std::string_view path) {
    std::string out;
    out.reserve(path.size());
    bool start = true;
    for (char c : path) {
        if (c == '_' || c == ' ') {
            out   += ' ';
            start  = true;
            continue;
        }
        out   += (start && isLower(c)) ? static_cast<char>(c - 'a' + 'A') : c;
        start  = false;
    }
    return out;
}

std::string javaFloatToString(float value) {
    if (std::isnan(value)) return "NaN";
    if (std::isinf(value)) return value > 0 ? "Infinity" : "-Infinity";
    if (value == 0.f) return std::signbit(value) ? "-0.0" : "0.0";

    // Shortest round-trip digits in scientific form, e.g. "8.5e-01".
    std::array<char, 64> buf{};
    auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value, std::chars_format::scientific);
    if (ec != std::errc{}) return "NaN";
    std::string_view sci(buf.data(), static_cast<size_t>(end - buf.data()));

    std::string out;
    if (sci.front() == '-') {
        out += '-';
        sci.remove_prefix(1);
    }
    auto        e = sci.find('e');
    std::string digits;
    for (char c : sci.substr(0, e)) {
        if (c != '.') digits += c;
    }
    int exp = 0;
    {
        auto expText = sci.substr(e + 1);
        if (!expText.empty() && expText.front() == '+') expText.remove_prefix(1);
        std::from_chars(expText.data(), expText.data() + expText.size(), exp);
    }
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();

    float magnitude = std::fabs(value);
    if (magnitude >= 1e-3f && magnitude < 1e7f) {
        if (exp >= 0) {
            auto intLen = static_cast<size_t>(exp) + 1;
            if (digits.size() <= intLen) {
                out += digits + std::string(intLen - digits.size(), '0') + ".0";
            } else {
                out += digits.substr(0, intLen) + "." + digits.substr(intLen);
            }
        } else {
            out += "0." + std::string(static_cast<size_t>(-exp - 1), '0') + digits;
        }
    } else {
        out += digits.substr(0, 1) + "." + (digits.size() > 1 ? digits.substr(1) : std::string("0")) + "E"
             + std::to_string(exp);
    }
    return out;
}

std::vector<std::string> ncTagsFromBedrock(std::vector<std::string> const& t) {
    std::vector<std::string> out;
    bool const               ocean = has(t, "ocean");
    if (has(t, "mesa")) out.emplace_back("badlands");
    if (has(t, "beach") && !has(t, "stone")) out.emplace_back("beach");
    if (ocean && has(t, "deep")) out.emplace_back("deep_ocean");
    if (has(t, "the_end")) out.emplace_back("end");
    if ((has(t, "forest") || has(t, "flower_forest") || has(t, "grove") || has(t, "pale_garden")) && !has(t, "taiga")
        && !has(t, "extreme_hills"))
        out.emplace_back("forest");
    if (has(t, "extreme_hills")) out.emplace_back("hill");
    if (has(t, "jungle")) out.emplace_back("jungle");
    if (has(t, "mountains") && !has(t, "grove")) out.emplace_back("mountain");
    if (has(t, "nether")) out.emplace_back("nether");
    if (ocean) out.emplace_back("ocean");
    if (has(t, "river")) out.emplace_back("river");
    if (has(t, "savanna")) out.emplace_back("savanna");
    if (has(t, "taiga")) out.emplace_back("taiga");
    return out;
}

std::vector<std::string> dimensionsFromBedrockTags(std::vector<std::string> const& t) {
    std::vector<std::string> out;
    if (has(t, "overworld") || has(t, "overworld_generation")) out.emplace_back("minecraft:overworld");
    if (has(t, "nether")) out.emplace_back("minecraft:nether");
    if (has(t, "the_end")) out.emplace_back("minecraft:the_end");
    return out;
}

bool isLegacyBedrockBiome(std::string_view path) {
    static constexpr std::string_view kLegacy[] = {
        "bamboo_jungle_hills",
        "birch_forest_hills",
        "birch_forest_hills_mutated",
        "cold_taiga_hills",
        "cold_taiga_mutated",
        "deep_warm_ocean",
        "desert_hills",
        "desert_mutated",
        "extreme_hills_edge",
        "extreme_hills_plus_trees_mutated",
        "forest_hills",
        "ice_mountains",
        "jungle_edge_mutated",
        "jungle_hills",
        "jungle_mutated",
        "legacy_frozen_ocean",
        "mega_taiga_hills",
        "mesa_plateau",
        "mesa_plateau_mutated",
        "mesa_plateau_stone_mutated",
        "mushroom_island_shore",
        "redwood_taiga_hills_mutated",
        "roofed_forest_mutated",
        "savanna_plateau_mutated",
        "swampland_mutated",
        "taiga_hills",
        "taiga_mutated",
    };
    return std::find(std::begin(kLegacy), std::end(kLegacy), path) != std::end(kLegacy);
}

std::string tagsLine(
    std::vector<std::string> const& tags,
    std::string_view                ns,
    TranslationLookup const&        lookup,
    std::string_view                noneText
) {
    std::set<std::string> names;
    std::string const     nsText(ns);
    for (auto const& tag : tags) {
        if (auto t = lookup("biome." + nsText + "." + tag)) return *t;
        if (auto t = lookup("category." + nsText + "." + tag)) return *t;
        names.insert(capitalizePath(tag));
    }
    if (names.empty()) return std::string(noneText);
    std::string out;
    for (auto const& n : names) {
        if (!out.empty()) out += ", ";
        out += n;
    }
    return out;
}

std::string dimensionFallbackName(std::string_view dimensionId) {
    auto path = idPath(dimensionId);
    if (idNamespace(dimensionId) == "minecraft" && path == "nether") return "The Nether";
    return capitalizePath(path);
}

std::string sourceName(std::string_view ns) {
    if (ns == "minecraft") return "Minecraft";
    return std::string(ns);
}

std::optional<std::pair<std::string, int>> parseXpEntry(std::string_view entry) {
    auto comma = entry.find(',');
    if (comma == std::string_view::npos || entry.find(',', comma + 1) != std::string_view::npos) return std::nullopt;
    auto glob = trim(entry.substr(0, comma));
    auto num  = trim(entry.substr(comma + 1));
    if (glob.empty() || num.empty()) return std::nullopt;
    // Java Integer.parseInt: optional sign, decimal digits, int range.
    if (num.front() == '+') {
        num.remove_prefix(1);
        if (num.empty() || num.front() == '-') return std::nullopt;
    }
    int levels   = 0;
    auto [p, ec] = std::from_chars(num.data(), num.data() + num.size(), levels);
    if (ec != std::errc{} || p != num.data() + num.size()) return std::nullopt;
    return std::pair{std::string(glob), std::clamp(levels, 0, 3)};
}

} // namespace nl::core
