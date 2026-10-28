// Selftests of the biomes unit (U2): catalog, hidden biomes, blacklist, XP costs, names, display
// helpers. Unit U2 owns selftest dx 200..299 (these tests place no blocks).

#include <algorithm>
#include <string>

#include "feature/biomes/BiomesInternal.h"
#include "feature/compass/Biomes.h"
#include "feature/compass/CompassConfig.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "selftest/SelfTest.h"

namespace {

using nl::selftest::Context;
namespace biomes  = nl::feature::biomes;
namespace compass = nl::feature::compass;

/// Changes the compass config for one test and restores it (and the catalog) afterwards.
class ConfigGuard {
public:
    ConfigGuard() : mSaved(nl::config::get<compass::CompassConfig>()) {}
    ~ConfigGuard() {
        nl::config::mut<compass::CompassConfig>() = mSaved;
        biomes::invalidateCatalog();
    }
    ConfigGuard(ConfigGuard const&)                       = delete;
    ConfigGuard&            operator=(ConfigGuard const&) = delete;
    compass::CompassConfig& cfg() { return nl::config::mut<compass::CompassConfig>(); }

private:
    compass::CompassConfig mSaved;
};

bool hasDim(biomes::BiomeInfo const* b, std::string const& dim) {
    return b && std::find(b->dimensions.begin(), b->dimensions.end(), dim) != b->dimensions.end();
}

void expectEntry(Context& ctx, char const* id, char const* dim) {
    auto const* b = biomes::findBiome(id);
    ctx.check(b != nullptr, std::string(id) + " is searchable");
    ctx.check(hasDim(b, dim), std::string(id) + " is in " + dim);
    if (b) ctx.check(b->dimensions.size() == 1, std::string(id) + " is in exactly one dimension");
}

constexpr std::string_view kMissing = "\x01missing";

} // namespace

NL_REGISTER_SELFTEST("biomes.catalog", [](Context& ctx) {
    biomes::invalidateCatalog();
    auto const& all = biomes::searchableBiomes();
    ctx.info("catalog: " + std::to_string(all.size()) + " biomes");
    ctx.require(all.size() >= 50, "catalog lists the vanilla biomes");
    expectEntry(ctx, "minecraft:plains", "minecraft:overworld");
    expectEntry(ctx, "minecraft:desert", "minecraft:overworld");
    expectEntry(ctx, "minecraft:deep_dark", "minecraft:overworld");
    expectEntry(ctx, "minecraft:hell", "minecraft:nether");
    expectEntry(ctx, "minecraft:crimson_forest", "minecraft:nether");
    expectEntry(ctx, "minecraft:the_end", "minecraft:the_end");
    for (auto const& b : all) {
        ctx.check(b.id.find(':') != std::string::npos, b.id + " has a namespace");
        // Add-on biomes without dimension tags may have none (contract); vanilla ones always have one.
        if (b.source == "minecraft") ctx.check(!b.dimensions.empty(), b.id + " has a dimension");
        ctx.check(b.xpLevels >= 0 && b.xpLevels <= 3, b.id + " XP cost in 0..3");
        ctx.check(
            std::count_if(all.begin(), all.end(), [&](auto const& o) { return o.id == b.id; }) == 1,
            b.id + " is listed once"
        );
    }
    auto const* plains = biomes::findBiome("minecraft:plains");
    ctx.require(plains != nullptr, "plains");
    ctx.check(plains->source == "minecraft", "plains source namespace");
    ctx.check(plains->temperature == 0.8f && plains->downfall == 0.4f, "plains temperature 0.8, downfall 0.4");
    ctx.check(biomes::findBiome("minecraft:no_such_biome") == nullptr, "unknown ids are not found");
});

NL_REGISTER_SELFTEST("biomes.hidden", [](Context& ctx) {
    biomes::invalidateCatalog();
    for (auto const* id :
         {"minecraft:desert_hills",
          "minecraft:legacy_frozen_ocean",
          "minecraft:mesa_plateau",
          "minecraft:mushroom_island_shore",
          "minecraft:jungle_mutated"}) {
        ctx.check(biomes::findBiome(id) == nullptr, std::string(id) + " (legacy, never generated) is hidden");
    }
    ctx.check(biomes::findBiome("minecraft:") == nullptr, "the unnamed placeholder biome is hidden");
    for (auto const* id :
         {"minecraft:mesa_plateau_stone",
          "minecraft:birch_forest_mutated",
          "minecraft:redwood_taiga_mutated",
          "minecraft:savanna_mutated",
          "minecraft:mesa_bryce"}) {
        ctx.check(biomes::findBiome(id) != nullptr, std::string(id) + " (generated) is listed");
    }
});

NL_REGISTER_SELFTEST("biomes.blacklist", [](Context& ctx) {
    ConfigGuard guard;
    guard.cfg().biomeBlacklist = {"minecraft:*ocean*", "minecraft:desert", "minecraft:the_en?"};
    biomes::invalidateCatalog();
    ctx.check(biomes::isBlacklisted("minecraft:deep_cold_ocean"), "*ocean* matches deep_cold_ocean");
    ctx.check(biomes::isBlacklisted("minecraft:ocean"), "*ocean* matches ocean");
    ctx.check(biomes::isBlacklisted("minecraft:the_end"), "? matches one character");
    ctx.check(!biomes::isBlacklisted("minecraft:desert_mutated"), "globs are anchored");
    ctx.check(!biomes::isBlacklisted("minecraft:plains"), "plains is not blacklisted");
    ctx.check(biomes::findBiome("minecraft:desert") == nullptr, "blacklisted desert is not searchable");
    ctx.check(biomes::findBiome("minecraft:warm_ocean") == nullptr, "blacklisted warm_ocean is not searchable");
    ctx.check(biomes::findBiome("minecraft:the_end") == nullptr, "blacklisted the_end is not searchable");
    ctx.check(biomes::findBiome("minecraft:plains") != nullptr, "plains stays searchable");
});

NL_REGISTER_SELFTEST("biomes.xp_levels", [](Context& ctx) {
    ConfigGuard guard;
    guard.cfg().defaultXpLevels  = 1;
    guard.cfg().perBiomeXpLevels = {
        "minecraft:desert,2",
        "garbage",
        "minecraft:*ocean*,7",
        "minecraft:plains,x",
        "minecraft:deep_*,0",
        "minecraft:plains, 3 "
    };
    biomes::invalidateCatalog();
    ctx.check(biomes::xpLevelsFor("minecraft:desert") == 2, "desert costs 2");
    ctx.check(biomes::xpLevelsFor("minecraft:deep_ocean") == 3, "first match wins, capped at 3");
    ctx.check(biomes::xpLevelsFor("minecraft:deep_dark") == 0, "deep_* costs 0");
    ctx.check(biomes::xpLevelsFor("minecraft:plains") == 3, "malformed entries are skipped");
    ctx.check(biomes::xpLevelsFor("minecraft:forest") == 1, "defaultXpLevels otherwise");
    auto const* desert = biomes::findBiome("minecraft:desert");
    ctx.check(desert && desert->xpLevels == 2, "catalog entry carries the XP cost");
    auto const* forest = biomes::findBiome("minecraft:forest");
    ctx.check(forest && forest->xpLevels == 1, "catalog entry carries the default XP cost");
    guard.cfg().defaultXpLevels = 9;
    ctx.check(biomes::xpLevelsFor("minecraft:forest") == 3, "defaultXpLevels is clamped to 3");
});

NL_REGISTER_SELFTEST("biomes.names", [](Context& ctx) {
    biomes::invalidateCatalog();
    ctx.check(biomes::biomeDisplayName("minecraft:plains", "en_US") == "Plains", "plains en_US");
    ctx.check(biomes::biomeDisplayName("minecraft:plains", "zh_CN") == "平原", "plains zh_CN");
    ctx.check(biomes::biomeDisplayName("minecraft:hell", "en_US") == "Nether Wastes", "hell en_US");
    ctx.check(biomes::biomeDisplayName("minecraft:hell", "zh_CN") == "下界荒地", "hell zh_CN");
    ctx.check(biomes::biomeDisplayName("minecraft:roofed_forest", "en_US") == "Dark Forest", "roofed_forest en_US");
    ctx.check(biomes::biomeDisplayName("minecraft:the_end", "zh_CN") == "末地", "the_end zh_CN");
    ctx.check(biomes::biomeDisplayName("plains", "en_US") == "Plains", "ids without a namespace");
    ctx.check(biomes::biomeDisplayName("foo:crystal_wastes", "en_US") == "Crystal Wastes", "unknown ids title-cased");
    ctx.check(biomes::biomeDisplayName("foo:CrystalWastes", "en_US", true) == "Crystal Wastes", "fixBiomeNames");
    ctx.check(biomes::dimensionDisplayName("minecraft:overworld", "en_US") == "Overworld", "Overworld");
    ctx.check(biomes::dimensionDisplayName("minecraft:nether", "en_US") == "The Nether", "The Nether");
    ctx.check(biomes::dimensionDisplayName("minecraft:the_end", "en_US") == "The End", "The End");
    ctx.check(biomes::dimensionDisplayName("minecraft:nether", "zh_CN") == "下界", "下界");
    ctx.check(biomes::dimensionDisplayName("foo:deep_space", "en_US") == "Deep Space", "unknown dimension");
    // Every searchable (vanilla) biome has its own en_US and zh_CN name.
    int missing = 0;
    for (auto const& b : biomes::searchableBiomes()) {
        if (!b.id.starts_with("minecraft:")) continue;
        auto key = "biome.minecraft." + b.id.substr(10);
        auto en  = nl::translate(key, kMissing, "en_US");
        auto zh  = nl::translate(key, kMissing, "zh_CN");
        if (en == kMissing || zh == kMissing || en == zh) {
            ++missing;
            ctx.check(false, b.id + " has en_US and zh_CN names");
        }
    }
    ctx.info("biomes without names: " + std::to_string(missing));
});

NL_REGISTER_SELFTEST("biomes.display", [](Context& ctx) {
    biomes::invalidateCatalog();
    auto row = [&](char const* id) -> biomes::BiomeInfo const* {
        auto const* b = biomes::findBiome(id);
        ctx.require(b != nullptr, std::string(id) + " is searchable");
        return b;
    };
    ctx.check(biomes::tagsText(*row("minecraft:plains"), "en_US") == "None", "plains: no tags");
    ctx.check(biomes::tagsText(*row("minecraft:plains"), "zh_CN") == "无", "plains: no tags (zh_CN)");
    ctx.check(biomes::tagsText(*row("minecraft:forest"), "en_US") == "Forest", "forest: Forest");
    ctx.check(biomes::tagsText(*row("minecraft:forest"), "zh_CN") == "森林", "forest: 森林");
    ctx.check(biomes::tagsText(*row("minecraft:cold_taiga"), "en_US") == "Taiga", "snowy taiga: Taiga");
    ctx.check(biomes::tagsText(*row("minecraft:deep_ocean"), "en_US") == "Deep Ocean", "deep ocean: Deep Ocean");
    ctx.check(biomes::tagsText(*row("minecraft:jagged_peaks"), "en_US") == "Mountain", "jagged peaks: Mountain");
    ctx.check(biomes::tagsText(*row("minecraft:extreme_hills"), "en_US") == "Hill", "windswept hills: Hill");
    ctx.check(biomes::tagsText(*row("minecraft:mesa"), "en_US") == "Badlands", "badlands: Badlands");
    ctx.check(biomes::tagsText(*row("minecraft:hell"), "en_US") == "Nether", "nether wastes: Nether");
    ctx.check(biomes::tagsText(*row("minecraft:jungle"), "en_US") == "Jungle", "jungle: Jungle");
    ctx.check(biomes::tagsText(*row("minecraft:bamboo_jungle"), "en_US") == "Jungle", "bamboo jungle: Jungle");
    ctx.check(biomes::tagsText(*row("minecraft:cherry_grove"), "en_US") == "Mountain", "cherry grove: Mountain");
    ctx.check(biomes::tagsText(*row("minecraft:grove"), "en_US") == "Forest", "grove: Forest");
    ctx.check(
        biomes::tagsText(*row("minecraft:extreme_hills_plus_trees"), "en_US") == "Hill",
        "windswept forest: Hill"
    );
    ctx.check(biomes::tagsText(*row("minecraft:stone_beach"), "en_US") == "None", "stony shore: None");
    ctx.check(biomes::tagsText(*row("minecraft:cold_beach"), "en_US") == "Beach", "snowy beach: Beach");
    ctx.check(biomes::temperatureText(*row("minecraft:plains")) == "0.8", "plains temperature text");
    ctx.check(biomes::rainfallText(*row("minecraft:desert")) == "0.0", "desert rainfall text");
    ctx.check(biomes::temperatureText(*row("minecraft:cold_taiga")) == "-0.5", "snowy taiga temperature text");
    ctx.check(biomes::tagsText(*row("minecraft:the_end"), "en_US") == "End", "the end: End");
    ctx.check(biomes::dimensionsText(*row("minecraft:plains"), "en_US") == "Overworld", "plains: Overworld");
    ctx.check(biomes::dimensionsText(*row("minecraft:warped_forest"), "en_US") == "The Nether", "warped: The Nether");
    ctx.check(biomes::dimensionsText(*row("minecraft:the_end"), "zh_CN") == "末地", "the end: 末地");
    ctx.check(biomes::sourceText(*row("minecraft:plains"), "en_US") == "Minecraft", "source: Minecraft");
});
