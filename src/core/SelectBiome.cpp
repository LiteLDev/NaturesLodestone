#include "core/SelectBiome.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <system_error>

namespace nl::core {

namespace {

struct ModeInfo {
    std::string_view key;
    std::string_view english;
};

constexpr std::array<ModeInfo, kBiomeSortModeCount> kModes{
    {
     {"string.naturescompass.name", "Name"},
     {"string.naturescompass.source", "Source"},
     {"string.naturescompass.tags", "Tags"},
     {"string.naturescompass.dimension", "Dimension"},
     {"string.naturescompass.levels", "Levels"},
     {"string.naturescompass.rainfall", "Rainfall"},
     {"string.naturescompass.temperature", "Temperature"},
     }
};

ModeInfo const& info(BiomeSortMode mode) {
    auto i = static_cast<int>(mode);
    return kModes[static_cast<std::size_t>(i >= 0 && i < kBiomeSortModeCount ? i : 0)];
}

char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

bool containsIgnoreCase(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    if (needle.size() > haystack.size()) return false;
    for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        std::size_t j = 0;
        while (j < needle.size() && lower(haystack[i + j]) == lower(needle[j])) ++j;
        if (j == needle.size()) return true;
    }
    return false;
}

/// Byte length of the UTF-8 sequence starting with `lead` (1 for stray continuation bytes).
std::size_t utf8Length(unsigned char lead) {
    if (lead >= 0xF0) return 4;
    if (lead >= 0xE0) return 3;
    if (lead >= 0xC0) return 2;
    return 1;
}

} // namespace

BiomeSortMode nextSortMode(BiomeSortMode mode) {
    return static_cast<BiomeSortMode>((static_cast<int>(mode) + 1) % kBiomeSortModeCount);
}

std::string_view sortModeKey(BiomeSortMode mode) { return info(mode).key; }

std::string_view sortModeEnglish(BiomeSortMode mode) { return info(mode).english; }

bool showsDimensionLine(BiomeSortMode mode) {
    switch (mode) {
    case BiomeSortMode::Name:
    case BiomeSortMode::Source:
    case BiomeSortMode::Tags:
    case BiomeSortMode::Dimension:
        return true;
    default:
        return false;
    }
}

std::string javaFloatToString(float value) {
    if (std::isnan(value)) return "NaN";
    if (std::isinf(value)) return value > 0 ? "Infinity" : "-Infinity";
    if (value == 0.f) return std::signbit(value) ? "-0.0" : "0.0";

    // Shortest round-trip digits, as "d.ddde±xx".
    std::array<char, 64> buf{};
    auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value, std::chars_format::scientific);
    if (ec != std::errc{}) return "0.0";
    std::string_view sci(buf.data(), static_cast<std::size_t>(end - buf.data()));

    bool negative = false;
    if (!sci.empty() && sci.front() == '-') {
        negative = true;
        sci.remove_prefix(1);
    }
    auto        ePos = sci.find('e');
    std::string digits;
    for (char c : sci.substr(0, ePos)) {
        if (c != '.') digits += c;
    }
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    int exponent = std::atoi(std::string(sci.substr(ePos + 1)).c_str());

    std::string out = negative ? "-" : "";
    float       mag = std::fabs(value);
    if (mag >= 1e-3f && mag < 1e7f) {
        if (exponent >= 0) {
            auto intLen = static_cast<std::size_t>(exponent) + 1;
            if (digits.size() <= intLen) {
                out += digits + std::string(intLen - digits.size(), '0') + ".0";
            } else {
                out += digits.substr(0, intLen) + "." + digits.substr(intLen);
            }
        } else {
            out += "0." + std::string(static_cast<std::size_t>(-exponent - 1), '0') + digits;
        }
    } else {
        out += digits.substr(0, 1) + "." + (digits.size() > 1 ? digits.substr(1) : std::string("0"));
        out += "E" + std::to_string(exponent);
    }
    return out;
}

std::string ellipsize(std::string_view text, std::size_t maxChars) {
    if (maxChars < 4) return "...";
    std::size_t count = 0;
    std::size_t cut   = 0; // byte offset after maxChars - 3 code points
    for (std::size_t i = 0; i < text.size(); i += utf8Length(static_cast<unsigned char>(text[i]))) {
        if (count == maxChars - 3) cut = i;
        ++count;
    }
    if (count <= maxChars) return std::string(text);
    return std::string(text.substr(0, cut)) + "...";
}

std::string sortValue(BiomeRow const& row, BiomeSortMode mode) {
    switch (mode) {
    case BiomeSortMode::Name:
        return row.rawName;
    case BiomeSortMode::Source:
        return row.source;
    case BiomeSortMode::Tags:
        return row.tags;
    case BiomeSortMode::Dimension:
        return row.dimensions;
    case BiomeSortMode::Levels:
        return std::to_string(row.xpLevels);
    case BiomeSortMode::Rainfall:
        return row.rainfallText.empty() ? javaFloatToString(row.rainfall) : row.rainfallText;
    case BiomeSortMode::Temperature:
        return row.temperatureText.empty() ? javaFloatToString(row.temperature) : row.temperatureText;
    }
    return row.rawName;
}

bool matchesFilter(BiomeRow const& row, std::string_view filter) {
    if (filter.empty()) return true;
    if (filter.front() == '$') return containsIgnoreCase(row.tags, filter.substr(1));
    if (filter.front() == '@') return containsIgnoreCase(row.source, filter.substr(1));
    return containsIgnoreCase(row.displayName, filter);
}

std::vector<std::size_t> sortedRows(std::vector<BiomeRow> const& rows, BiomeSortMode mode) {
    std::vector<std::size_t> order(rows.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return rows[a].rawName < rows[b].rawName;
    });
    switch (mode) {
    case BiomeSortMode::Name:
        break;
    case BiomeSortMode::Rainfall:
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return rows[a].rainfall < rows[b].rainfall;
        });
        break;
    case BiomeSortMode::Temperature:
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return rows[a].temperature < rows[b].temperature;
        });
        break;
    default: {
        std::vector<std::string> keys;
        keys.reserve(rows.size());
        for (auto const& row : rows) keys.push_back(sortValue(row, mode));
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return keys[a] < keys[b]; });
        break;
    }
    }
    return order;
}

ButtonStates buttonStates(
    std::optional<std::string> const& selected,
    CompassView const&                compass,
    int                               maxNextSearches,
    bool                              canTeleport
) {
    ButtonStates s;
    bool selectedIsFound = selected && compass.found && *selected == compass.foundBiome;
    s.searchActive       = selected.has_value();
    s.nextVisible        = maxNextSearches > 0;
    s.nextActive         = s.nextVisible && selectedIsFound;
    s.teleportVisible    = canTeleport;
    s.teleportActive     = canTeleport && selectedIsFound;
    return s;
}

SelectBiomeModel::SelectBiomeModel(std::vector<BiomeRow> rows, CompassView compass)
: mRows(std::move(rows)),
  mCompass(std::move(compass)) {
    rebuild();
    if (mCompass.found) select(mCompass.foundBiome);
}

BiomeRow const* SelectBiomeModel::listedRow(std::size_t position) const {
    return position < mListed.size() ? &mRows[mListed[position]] : nullptr;
}

bool SelectBiomeModel::setFilter(std::string filter) {
    if (filter == mFilter) return false;
    mFilter = std::move(filter);
    rebuild();
    return true;
}

void SelectBiomeModel::cycleSort() {
    mSort = nextSortMode(mSort);
    rebuild();
}

SelectBiomeModel::Click SelectBiomeModel::click(std::size_t position) {
    auto const* row = listedRow(position);
    if (!row || !row->affordable) return Click::Ignored;
    if (isSelected(*row)) return Click::Search;
    mSelected = row->id;
    return Click::Selected;
}

void SelectBiomeModel::select(std::string const& biomeId) {
    mSelected.reset();
    for (auto index : mListed) {
        if (mRows[index].id == biomeId) {
            mSelected = biomeId;
            return;
        }
    }
}

void SelectBiomeModel::rebuild() {
    mSelected.reset();
    mListed.clear();
    for (auto index : sortedRows(mRows, mSort)) {
        if (matchesFilter(mRows[index], mFilter)) mListed.push_back(index);
    }
}

} // namespace nl::core
