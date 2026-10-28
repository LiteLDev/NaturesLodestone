#pragma once

// The game-independent model of the Select Biome screen (docs/SPEC.md section 4.2): the rows,
// the filter, the sort modes, the selection and the button rules. The form front-ends in
// src/feature/gui/ only translate strings and draw what this model says.

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/BiomeText.h" // javaFloatToString

namespace nl::core {

/// NC's sort modes, in cycling order.
enum class BiomeSortMode : int {
    Name        = 0,
    Source      = 1,
    Tags        = 2,
    Dimension   = 3,
    Levels      = 4,
    Rainfall    = 5,
    Temperature = 6,
};

inline constexpr int kBiomeSortModeCount = 7;

/// The mode after `mode` (Temperature wraps to Name).
[[nodiscard]] BiomeSortMode nextSortMode(BiomeSortMode mode);

/// The `string.naturescompass.*` key of the mode's name ("string.naturescompass.name").
[[nodiscard]] std::string_view sortModeKey(BiomeSortMode mode);

/// The English name of the mode ("Name").
[[nodiscard]] std::string_view sortModeEnglish(BiomeSortMode mode);

/// Row line 2 shows `Dimension: <dims>` for Name / Source / Tags / Dimension, else
/// `<Mode>: <value>`.
[[nodiscard]] bool showsDimensionLine(BiomeSortMode mode);

/// Cuts `text` to at most `maxChars` code points, ending in "..." when cut (NC cuts the tags
/// line to the row width). `maxChars` < 4 returns "...".
[[nodiscard]] std::string ellipsize(std::string_view text, std::size_t maxChars);

/// One searchable biome as the screen shows it. Texts are already translated.
struct BiomeRow {
    std::string id;          ///< "minecraft:plains"
    std::string rawName;     ///< translated name, used for sorting ("ForestHills")
    std::string displayName; ///< shown and filtered (fixBiomeNames applied when enabled)
    std::string dimensions;  ///< "Overworld, The Nether" (may be empty)
    std::string tags;        ///< "Forest" / "None"
    std::string source;      ///< "Minecraft"
    float       temperature = 0.f;
    float       rainfall    = 0.f;
    std::string temperatureText; ///< shown for Temperature; empty = `javaFloatToString(temperature)`
    std::string rainfallText;    ///< shown for Rainfall; empty = `javaFloatToString(rainfall)`
    int         xpLevels    = 0;
    bool        affordable  = true; ///< creative, or the player's level >= xpLevels
};

/// The value of `row` for `mode` as NC compares and shows it (Levels as a string, Rainfall and
/// Temperature through `javaFloatToString`, Name = the raw name).
[[nodiscard]] std::string sortValue(BiomeRow const& row, BiomeSortMode mode);

/// NC's filter: case-insensitive "contains"; `$term` matches the tags text, `@term` the source,
/// anything else the display name. An empty filter matches everything.
[[nodiscard]] bool matchesFilter(BiomeRow const& row, std::string_view filter);

/// Indices of `rows` sorted the NC way: by raw name, then stable by `mode`, ascending.
[[nodiscard]] std::vector<std::size_t> sortedRows(std::vector<BiomeRow> const& rows, BiomeSortMode mode);

/// What the compass the screen was opened for says.
struct CompassView {
    bool        found = false; ///< state Found
    std::string foundBiome;    ///< its biome id (Found only)
};

/// Visibility and activity of the buttons.
struct ButtonStates {
    bool searchActive    = false; ///< an entry is selected
    bool nextVisible     = false; ///< maxNextSearches > 0
    bool nextActive      = false; ///< the selected entry is the Found biome
    bool teleportVisible = false; ///< canTeleport
    bool teleportActive  = false; ///< same rule as Next
};

[[nodiscard]] ButtonStates
buttonStates(std::optional<std::string> const& selected, CompassView const& compass, int maxNextSearches, bool canTeleport);

/// The state of one open Select Biome screen.
class SelectBiomeModel {
public:
    enum class Click {
        Ignored,  ///< out of range or not affordable
        Selected, ///< the row is now selected
        Search,   ///< a second click on the selected row (NC double click): search for it
    };

    /// Builds the list (sorted by Name, no filter) and pre-selects the Found biome (if affordable).
    SelectBiomeModel(std::vector<BiomeRow> rows, CompassView compass);

    [[nodiscard]] std::vector<BiomeRow> const& rows() const { return mRows; }
    [[nodiscard]] CompassView const&           compass() const { return mCompass; }
    [[nodiscard]] BiomeSortMode                sortMode() const { return mSort; }
    [[nodiscard]] std::string const&           filter() const { return mFilter; }

    /// Indices into `rows()` of the listed entries, in display order.
    [[nodiscard]] std::vector<std::size_t> const& listed() const { return mListed; }

    /// The row listed at `position` (nullptr when out of range).
    [[nodiscard]] BiomeRow const* listedRow(std::size_t position) const;

    /// The selected biome id.
    [[nodiscard]] std::optional<std::string> const& selected() const { return mSelected; }
    [[nodiscard]] bool isSelected(BiomeRow const& row) const { return mSelected && *mSelected == row.id; }

    /// Changes the filter. A different filter rebuilds the list: the selection is dropped and the
    /// list starts at the top again. Returns true when the list was rebuilt.
    bool setFilter(std::string filter);

    /// "Sort By": the next mode; rebuilds the list (selection dropped).
    void cycleSort();

    /// A click on the row listed at `position`.
    Click click(std::size_t position);

    /// Selects `biomeId` if it is listed (and drops the selection otherwise).
    void select(std::string const& biomeId);

    [[nodiscard]] ButtonStates buttons(int maxNextSearches, bool canTeleport) const {
        return buttonStates(mSelected, mCompass, maxNextSearches, canTeleport);
    }

private:
    void rebuild();

    std::vector<BiomeRow>      mRows;
    CompassView                mCompass;
    BiomeSortMode              mSort = BiomeSortMode::Name;
    std::string                mFilter;
    std::vector<std::size_t>   mListed;
    std::optional<std::string> mSelected;
};

} // namespace nl::core
