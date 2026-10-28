// Default (minimal) implementation of the GUI contract (src/feature/compass/Gui.h): a classic
// button form. Unit U5 `gui` replaces this file with the full Select Biome screen.

#include "feature/compass/Gui.h"

#include <algorithm>

#include "ll/api/form/SimpleForm.h"

#include "mc/world/actor/player/Player.h"

#include "feature/compass/Actions.h"
#include "feature/compass/Biomes.h"
#include "feature/compass/CompassConfig.h"
#include "feature/compass/CompassData.h"
#include "mod/I18n.h"
#include "mod/Log.h"

namespace nl::feature::gui {

void openSelectBiome(Player& player) {
    auto slot = compass::heldCompass(player);
    if (!slot) return;
    auto data   = compass::dataAt(player, *slot).value_or(compass::CompassData{});
    auto locale = localeOf(player);
    bool found  = data.state == compass::CompassState::Found;

    ll::form::SimpleForm form(trFor(player, "string.naturescompass.selectBiome", "Select Biome"));
    if (found && compass::maxNextSearches() > 0) {
        form.appendButton(trFor(player, "string.naturescompass.searchForNext", "Search for Next"), [](Player& p) {
            actions::searchForNext(p);
        });
    }
    if (found && actions::canTeleport(player)) {
        form.appendButton(trFor(player, "string.naturescompass.teleport", "Teleport"), [](Player& p) {
            actions::teleport(p);
        });
    }
    struct Entry {
        std::string id;
        std::string name;
    };
    std::vector<Entry> entries;
    for (auto const& b : biomes::searchableBiomes()) {
        if (!actions::canAfford(player, b.xpLevels)) continue;
        entries.push_back({b.id, biomes::biomeDisplayName(b.id, locale, compass::fixBiomeNames())});
    }
    std::stable_sort(entries.begin(), entries.end(), [](auto const& a, auto const& b) { return a.name < b.name; });
    for (auto const& e : entries) {
        form.appendButton(e.name, [id = e.id](Player& p) { actions::search(p, id); });
    }
    form.sendTo(player);
    log().info("Select Biome opened for {} ({} biomes)", player.getRealName(), entries.size());
}

} // namespace nl::feature::gui
