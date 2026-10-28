// Selftests of the Select Biome screen (U5 `gui`). A SimulatedPlayer can't click forms, so the
// tests drive an open screen through the same handlers the buttons use (GuiText.h `testing`).
// Unit U5 owns selftest dx 500..599 (these tests place no blocks).

#include "ll/api/coro/CoroTask.h"

#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"

#include "core/SelectBiome.h"
#include "feature/compass/Actions.h"
#include "feature/compass/Biomes.h"
#include "feature/compass/CompassConfig.h"
#include "feature/compass/CompassData.h"
#include "feature/compass/Gui.h"
#include "feature/gui/GuiText.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "selftest/SelfTest.h"

namespace {

using nl::selftest::Context;
namespace compass = nl::feature::compass;
namespace gui     = nl::feature::gui;
namespace biomes  = nl::feature::biomes;
using nl::core::BiomeSortMode;

/// Puts a compass into the selected slot, Found `biome` when given.
bool holdCompass(Context& ctx, Player& player, std::string const& biome = {}) {
    int  selected = player.getSelectedItemSlot();
    auto stack    = compass::makeCompass();
    if (!biome.empty()) {
        compass::CompassData d;
        d.succeed(biome, 100, 200, {{100, 64, 200}}, 5, true);
        compass::writeData(stack, d);
    }
    return ctx.give(player, stack, selected) == selected;
}

/// A searchable biome to work with (plains when the world has it).
std::string someBiome() {
    auto const& all = biomes::searchableBiomes();
    if (biomes::findBiome("minecraft:plains")) return "minecraft:plains";
    return all.empty() ? std::string{} : all.front().id;
}

std::size_t codePoints(std::string const& s) {
    std::size_t n = 0;
    for (unsigned char c : s) n += (c & 0xC0) != 0x80;
    return n;
}

/// Restores the front-end after a test.
struct FrontendGuard {
    gui::Frontend saved = gui::frontend();
    ~FrontendGuard() { gui::setFrontend(saved); }
};

} // namespace

NL_REGISTER_SELFTEST("gui.rows", [](Context& ctx) {
    auto& player = ctx.player();
    auto  rows   = gui::buildRows(player, "en_US");
    auto const& all = biomes::searchableBiomes();
    ctx.require(!all.empty(), "the catalog has biomes");
    ctx.check(rows.size() == all.size(), "one row per searchable biome");
    for (std::size_t i = 0; i < rows.size() && i < all.size(); ++i) {
        auto const& r = rows[i];
        ctx.check(r.id == all[i].id, "rows keep the catalog order: " + r.id);
        ctx.check(!r.rawName.empty() && !r.displayName.empty(), "row has a name: " + r.id);
        ctx.check(r.xpLevels == all[i].xpLevels, "row has the catalog's XP cost: " + r.id);
        ctx.check(r.affordable == nl::feature::actions::canAfford(player, r.xpLevels), "affordability: " + r.id);
        ctx.check(r.displayName == biomes::biomeDisplayName(r.id, "en_US", compass::fixBiomeNames()), "display name");
        ctx.check(r.rawName == biomes::biomeDisplayName(r.id, "en_US", false), "raw name sorts");
        ctx.check(r.tags == biomes::tagsText(all[i], "en_US"), "tags text");
        ctx.check(r.source == biomes::sourceText(all[i], "en_US"), "source text");
    }
});

NL_REGISTER_SELFTEST("gui.row_lines", [](Context& ctx) {
    auto& player = ctx.player();
    auto  rows   = gui::buildRows(player, "en_US");
    ctx.require(!rows.empty(), "rows");
    auto const& r = rows.front();
    auto        name = gui::rowLines(r, BiomeSortMode::Name, "en_US");
    ctx.check(name[0] == r.displayName, "line 1 is the display name");
    ctx.check(name[1] == "Dimension: " + r.dimensions, "Name sort: line 2 is Dimension: " + name[1]);
    ctx.check(name[2].rfind("Tags: ", 0) == 0, "line 3 starts with Tags: " + name[2]);
    ctx.check(codePoints(name[2]) <= gui::kTagsLineChars, "line 3 is cut to the row");
    ctx.check(name[3] == "Source: " + r.source, "line 4 is Source: " + name[3]);
    for (auto mode : {BiomeSortMode::Source, BiomeSortMode::Tags, BiomeSortMode::Dimension}) {
        ctx.check(gui::rowLines(r, mode, "en_US")[1] == name[1], "Source/Tags/Dimension sorts show Dimension");
    }
    ctx.check(
        gui::rowLines(r, BiomeSortMode::Levels, "en_US")[1] == "Levels: " + std::to_string(r.xpLevels),
        "Levels sort: line 2"
    );
    ctx.check(
        gui::rowLines(r, BiomeSortMode::Rainfall, "en_US")[1] == "Rainfall: " + nl::core::javaFloatToString(r.rainfall),
        "Rainfall sort: line 2"
    );
    ctx.check(
        gui::rowLines(r, BiomeSortMode::Temperature, "en_US")[1]
            == "Temperature: " + nl::core::javaFloatToString(r.temperature),
        "Temperature sort: line 2"
    );
    ctx.check(gui::sortButtonText(BiomeSortMode::Name, "en_US") == "Sort By: Name", "Sort By: Name");
    ctx.check(gui::sortButtonText(BiomeSortMode::Rainfall, "en_US") == "Sort By: Rainfall", "Sort By: Rainfall");
    ctx.check(gui::sortButtonText(BiomeSortMode::Name, "zh_CN") == "排序依据: 名称", "zh_CN Sort By");
});

NL_REGISTER_SELFTEST("gui.compass_view", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& player = ctx.player();
    co_await ctx.ticks(1);
    ctx.check(!gui::compassView(player).found, "no compass: nothing found");
    ctx.require(holdCompass(ctx, player), "compass held");
    ctx.check(!gui::compassView(player).found, "a new compass is not Found");
    ctx.require(holdCompass(ctx, player, "minecraft:desert"), "Found compass held");
    auto view = gui::compassView(player);
    ctx.check(view.found && view.foundBiome == "minecraft:desert", "Found compass: its biome");
});

NL_REGISTER_SELFTEST("gui.no_compass", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& player = ctx.player();
    co_await ctx.ticks(1);
    gui::openSelectBiome(player);
    ctx.check(!gui::testing::screen(player), "no screen without a compass");
});

namespace {

ll::coro::CoroTask<> screenFlow(Context& ctx, gui::Frontend frontend) {
    FrontendGuard guard;
    gui::setFrontend(frontend);
    auto& player = ctx.player();
    co_await ctx.ticks(1);
    auto biome = someBiome();
    ctx.require(!biome.empty(), "a searchable biome");
    ctx.require(holdCompass(ctx, player, biome), "Found compass held");

    gui::openSelectBiome(player);
    auto info = gui::testing::screen(player);
    ctx.require(info.has_value(), "the screen is open");
    if (frontend == gui::Frontend::Classic) ctx.check(!info->dataDriven, "classic form");
    ctx.check(info->rows == biomes::searchableBiomes().size(), "every biome is a row");
    ctx.check(info->listed == info->rows, "nothing filtered at first");
    ctx.check(info->sort == BiomeSortMode::Name, "sorted by name at first");
    ctx.check(info->selected && *info->selected == biome, "the Found biome is pre-selected");
    if (info->dataDriven) {
        ctx.check(info->sortLabel == gui::sortButtonText(BiomeSortMode::Name, nl::localeOf(player)), "Sort By label");
        ctx.check(info->rowLabels.size() == info->rows, "every row button is visible");
    }

    // Filter: the list is rebuilt and the selection dropped.
    auto const* row  = biomes::findBiome(biome);
    auto        name = biomes::biomeDisplayName(biome, nl::localeOf(player), compass::fixBiomeNames());
    ctx.require(gui::testing::setFilter(player, name), "filter typed");
    info = gui::testing::screen(player);
    ctx.require(info.has_value(), "still open after filtering");
    ctx.check(info->listed >= 1 && (info->listed < info->rows || info->rows == 1), "the filter narrows the list");
    ctx.check(!info->selected, "filtering drops the selection");
    if (info->dataDriven) ctx.check(info->rowLabels.size() == info->listed, "only listed rows are visible");
    gui::testing::setFilter(player, "@" + biomes::sourceText(*row, nl::localeOf(player)));
    info = gui::testing::screen(player);
    ctx.check(info && info->listed >= 1, "@source filter lists the biome's source");
    gui::testing::setFilter(player, "zzzz-no-such-biome");
    info = gui::testing::screen(player);
    ctx.check(info && info->listed == 0, "a filter matching nothing lists nothing");
    if (info && info->dataDriven) ctx.check(info->rowLabels.empty(), "no row visible");

    // Sort By cycles.
    gui::testing::setFilter(player, name);
    ctx.require(gui::testing::cycleSort(player), "Sort By pressed");
    info = gui::testing::screen(player);
    ctx.check(info && info->sort == BiomeSortMode::Source, "Name -> Source");
    if (info && info->dataDriven) {
        ctx.check(info->sortLabel == gui::sortButtonText(BiomeSortMode::Source, nl::localeOf(player)), "label follows");
    }
    for (int i = 0; i < nl::core::kBiomeSortModeCount - 1; ++i) gui::testing::cycleSort(player);
    info = gui::testing::screen(player);
    ctx.check(info && info->sort == BiomeSortMode::Name, "seven presses: back to Name");

    // Click selects (an affordable row); a second click searches and closes the screen.
    std::size_t position = 0;
    bool        found    = false;
    {
        auto rows = gui::buildRows(player, nl::localeOf(player));
        nl::core::SelectBiomeModel model(rows, {});
        model.setFilter(name);
        for (; position < model.listed().size(); ++position) {
            if (model.listedRow(position)->id == biome) {
                found = true;
                break;
            }
        }
    }
    ctx.require(found, "the biome is listed under its own name");
    ctx.require(gui::testing::clickRow(player, position), "row clicked");
    info = gui::testing::screen(player);
    ctx.check(info && info->selected && *info->selected == biome, "click selects");
    if (info && info->dataDriven) {
        bool marked = false;
        for (auto const& l : info->rowLabels) marked |= l.rfind("> ", 0) == 0;
        ctx.check(marked, "the selected row is marked");
    }
    ctx.require(gui::testing::clickRow(player, position), "row clicked again");
    ctx.check(!gui::testing::screen(player), "a second click searches and closes the screen");
    auto slot = compass::heldCompass(player);
    ctx.require(slot.has_value(), "compass still held");
    auto data = compass::dataAt(player, *slot).value_or(compass::CompassData{});
    ctx.check(data.state == compass::CompassState::Searching && data.biomeId == biome, "the search started");
    nl::feature::actions::onUse(player, true); // reset (stops the search)
    co_await ctx.ticks(1);
}

} // namespace

NL_REGISTER_SELFTEST("gui.flow", [](Context& ctx) -> ll::coro::CoroTask<> {
    co_await screenFlow(ctx, gui::Frontend::Auto);
});

NL_REGISTER_SELFTEST("gui.flow_classic", [](Context& ctx) -> ll::coro::CoroTask<> {
    co_await screenFlow(ctx, gui::Frontend::Classic);
});

NL_REGISTER_SELFTEST("gui.cancel", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& player = ctx.player();
    co_await ctx.ticks(1);
    ctx.require(holdCompass(ctx, player), "compass held");
    gui::openSelectBiome(player);
    auto info = gui::testing::screen(player);
    ctx.require(info.has_value(), "open");
    ctx.check(!info->selected, "nothing selected for an Inactive compass");
    gui::testing::close(player);
    ctx.check(!gui::testing::screen(player), "Cancel closes");
    auto slot = compass::heldCompass(player);
    ctx.require(slot.has_value(), "compass held");
    auto data = compass::dataAt(player, *slot).value_or(compass::CompassData{});
    ctx.check(data.state == compass::CompassState::Inactive, "Cancel starts nothing");
    co_await ctx.ticks(2); // the session is dropped on a later tick
    ctx.check(!gui::testing::screen(player), "still closed");
});
