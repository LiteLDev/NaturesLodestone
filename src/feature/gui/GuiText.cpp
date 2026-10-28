#include "feature/gui/GuiText.h"

#include "mc/world/actor/player/Player.h"

#include "feature/compass/Actions.h"
#include "feature/compass/Biomes.h"
#include "feature/compass/CompassConfig.h"
#include "feature/compass/CompassData.h"
#include "mod/I18n.h"

namespace nl::feature::gui {

namespace {
Frontend gFrontend = Frontend::Auto;
} // namespace

std::vector<core::BiomeRow> buildRows(Player const& player, std::string const& locale) {
    bool                        fix = compass::fixBiomeNames();
    std::vector<core::BiomeRow> rows;
    for (auto const& b : biomes::searchableBiomes()) {
        core::BiomeRow r;
        r.id          = b.id;
        r.rawName     = biomes::biomeDisplayName(b.id, locale, false);
        r.displayName = fix ? biomes::biomeDisplayName(b.id, locale, true) : r.rawName;
        r.dimensions  = biomes::dimensionsText(b, locale);
        r.tags        = biomes::tagsText(b, locale);
        r.source      = biomes::sourceText(b, locale);
        r.temperature = b.temperature;
        r.rainfall    = b.downfall;
        r.xpLevels    = b.xpLevels;
        r.affordable  = actions::canAfford(player, b.xpLevels);
        rows.push_back(std::move(r));
    }
    return rows;
}

core::CompassView compassView(Player const& player) {
    core::CompassView view;
    auto              slot = compass::heldCompass(player);
    if (!slot) return view;
    auto data = compass::dataAt(player, *slot);
    if (data && data->state == compass::CompassState::Found) {
        view.found      = true;
        view.foundBiome = data->biomeId;
    }
    return view;
}

std::string text(std::string_view locale, std::string_view key, std::string_view fallback) {
    return std::string(translate(key, fallback, locale));
}

std::array<std::string, 4> rowLines(core::BiomeRow const& row, core::BiomeSortMode mode, std::string_view locale) {
    std::array<std::string, 4> lines;
    lines[0] = row.displayName;
    if (core::showsDimensionLine(mode)) {
        lines[1] = text(locale, "string.naturescompass.dimension", "Dimension") + ": " + row.dimensions;
    } else {
        lines[1] = text(locale, core::sortModeKey(mode), core::sortModeEnglish(mode)) + ": " + core::sortValue(row, mode);
    }
    lines[2] = core::ellipsize(text(locale, "string.naturescompass.tags", "Tags") + ": " + row.tags, kTagsLineChars);
    lines[3] = text(locale, "string.naturescompass.source", "Source") + ": " + row.source;
    return lines;
}

std::string sortButtonText(core::BiomeSortMode mode, std::string_view locale) {
    return text(locale, "string.naturescompass.sortBy", "Sort By") + ": "
         + text(locale, core::sortModeKey(mode), core::sortModeEnglish(mode));
}

Frontend frontend() { return gFrontend; }

void setFrontend(Frontend value) { gFrontend = value; }

std::string_view frontendName(Frontend value) {
    switch (value) {
    case Frontend::Ddui:
        return "ddui";
    case Frontend::Classic:
        return "classic";
    default:
        return "auto";
    }
}

} // namespace nl::feature::gui
