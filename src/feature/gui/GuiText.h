#pragma once

// The Select Biome screen's game side: the rows of a player's screen and every text it shows
// (translated, before any form-specific colouring). Shared by both form front-ends.

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/SelectBiome.h"

class Player;

namespace nl::feature::gui {

/// Characters the tags line is cut to (NC cuts it to the row width).
inline constexpr std::size_t kTagsLineChars = 46;

/// The rows for `player`: every searchable biome with translated texts (`locale`), its XP cost
/// and whether the player can afford it.
[[nodiscard]] std::vector<core::BiomeRow> buildRows(Player const& player, std::string const& locale);

/// What the held compass says (Found + biome), for the pre-selection and the button rules.
[[nodiscard]] core::CompassView compassView(Player const& player);

/// Translation of a `string.naturescompass.*` key in `locale` (English fallback).
[[nodiscard]] std::string text(std::string_view locale, std::string_view key, std::string_view fallback);

/// NC's four row lines: name, `Dimension: ...` or `<Mode>: <value>`, `Tags: ...` (cut),
/// `Source: ...`.
[[nodiscard]] std::array<std::string, 4>
rowLines(core::BiomeRow const& row, core::BiomeSortMode mode, std::string_view locale);

/// "Sort By: <mode>".
[[nodiscard]] std::string sortButtonText(core::BiomeSortMode mode, std::string_view locale);

/// Which form a screen uses.
enum class Frontend {
    Auto,    ///< the data-driven `ll::ui` form, the classic form if it can't be shown
    Ddui,    ///< only the data-driven form
    Classic, ///< only the classic `ll::form` form (with the RP skin)
};

[[nodiscard]] Frontend   frontend();
void                     setFrontend(Frontend value);
[[nodiscard]] std::string_view frontendName(Frontend value);

/// Selftest hooks: drive an open screen through the same handlers the form buttons use.
namespace testing {

struct ScreenInfo {
    bool                       dataDriven = false;
    std::optional<std::string> selected;
    std::size_t                rows   = 0;
    std::size_t                listed = 0;
    core::BiomeSortMode        sort   = core::BiomeSortMode::Name;
    std::string                sortLabel; ///< data-driven: the Sort By button's current text
    std::vector<std::string>   rowLabels; ///< data-driven: the texts of the visible row buttons
};

[[nodiscard]] std::optional<ScreenInfo> screen(Player const& player);
/// The filter field changes (data-driven: as if the client wrote it).
bool setFilter(Player& player, std::string const& filter);
/// "Sort By".
bool cycleSort(Player& player);
/// A click on the listed row at `position`.
bool clickRow(Player& player, std::size_t position);
/// Closes the screen.
void close(Player& player);

} // namespace testing

} // namespace nl::feature::gui
