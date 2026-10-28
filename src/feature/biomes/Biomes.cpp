// The biome catalog (contract src/feature/compass/Biomes.h, docs/SPEC.md sections 3.6 and 4.1).
//
// Every registered biome of the level's BiomeRegistry, in registry (numeric id) order, minus the
// biomes hidden from the compass (the registry's unnamed placeholder biome and vanilla legacy
// biomes no current generator produces, `core::isLegacyBedrockBiome`) and the config blacklist.
// Dimensions and NC's `is_*` tags are derived from the Bedrock biome tags (`core::BiomeText`).

#include "feature/compass/Biomes.h"
#include "feature/biomes/BiomesInternal.h"

#include <algorithm>
#include <optional>

#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/string/HashedString.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/biome/Biome.h"
#include "mc/world/level/biome/registry/BiomeRegistry.h"

#include "core/BiomeText.h"
#include "core/Glob.h"
#include "feature/compass/CompassConfig.h"
#include "game/WorldDb.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace nl::feature::biomes {

namespace {

std::vector<BiomeInfo> gCatalog;
bool                   gBuilt = false;

constexpr std::string_view kMissing = "\x01nl-missing";

/// The translation of `key` in `locale` (or its fallbacks), nullopt if no lang file has it.
std::optional<std::string> lookup(std::string const& key, std::string_view locale) {
    auto t = translate(key, kMissing, locale);
    if (t == kMissing) return std::nullopt;
    return std::string(t);
}

void build() {
    gCatalog.clear();
    auto level = ll::service::getLevel();
    if (!level) return; // no world yet: try again on the next call
    gBuilt = true;

    auto&                     registry = level->getBiomeRegistry();
    std::vector<Biome const*> all;
    for (auto const& [hash, biome] : *registry.mBiomesByName) {
        if (biome) all.push_back(biome.get());
    }
    std::sort(all.begin(), all.end(), [](Biome const* a, Biome const* b) { return a->mId->mValue < b->mId->mValue; });

    int hidden = 0, blacklisted = 0;
    for (auto const* biome : all) {
        auto const& name = biome->mHash->getString();
        if (name.empty()) {
            ++hidden; // the registry's unnamed placeholder biome
            continue;
        }
        auto id = core::fullBiomeId(name);
        auto ns = core::idNamespace(id);
        if (ns == "minecraft" && core::isLegacyBedrockBiome(core::idPath(id))) {
            ++hidden;
            continue;
        }
        if (isBlacklisted(id)) {
            ++blacklisted;
            continue;
        }
        auto                     tagList = registry.biomeGetTags(*biome);
        std::vector<std::string> bedrockTags(tagList.begin(), tagList.end());
        BiomeInfo                info;
        info.id          = id;
        info.dimensions  = core::dimensionsFromBedrockTags(bedrockTags);
        info.tags        = core::ncTagsFromBedrock(bedrockTags);
        info.source      = std::string(ns);
        info.temperature = biome->mTemperature;
        info.downfall    = biome->mDownfall;
        info.xpLevels    = xpLevelsFor(id);
        gCatalog.push_back(std::move(info));
    }
    log().info("Biome catalog: {} searchable biomes ({} hidden, {} blacklisted)", gCatalog.size(), hidden, blacklisted);
}

} // namespace

/// Rebuilds the catalog lazily for every world and after a config reload.
class BiomesModule {
public:
    void install() {
        if (mHooked) return;
        mHooked = true;
        game::db::onWorldReady([] { gBuilt = false; });
        game::db::onWorldClosed([] {
            gBuilt = false;
            gCatalog.clear();
        });
        config::onReload([] { gBuilt = false; });
    }
    void uninstall() {
        gBuilt = false;
        gCatalog.clear();
    }

private:
    bool mHooked = false;
};

void invalidateCatalog() { gBuilt = false; }

std::vector<BiomeInfo> const& searchableBiomes() {
    if (!gBuilt) build();
    return gCatalog;
}

BiomeInfo const* findBiome(std::string_view id) {
    for (auto const& b : searchableBiomes()) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

bool isBlacklisted(std::string_view id) {
    for (auto const& pattern : compass::rawConfig().biomeBlacklist) {
        if (core::globMatch(pattern, id)) return true;
    }
    return false;
}

int xpLevelsFor(std::string_view id) {
    for (auto const& entry : compass::rawConfig().perBiomeXpLevels) {
        auto parsed = core::parseXpEntry(entry);
        if (parsed && core::globMatch(parsed->first, id)) return parsed->second;
    }
    return compass::defaultXpLevels();
}

std::string biomeDisplayName(std::string_view id, std::string_view locale, bool fix) {
    auto        full = core::fullBiomeId(id);
    std::string key  = "biome." + std::string(core::idNamespace(full)) + "." + std::string(core::idPath(full));
    std::string name(translate(key, core::capitalizePath(core::idPath(full)), locale));
    return fix ? core::fixBiomeName(name) : name;
}

std::string dimensionDisplayName(std::string_view dimensionId, std::string_view locale) {
    std::string key =
        "dimension." + std::string(core::idNamespace(dimensionId)) + "." + std::string(core::idPath(dimensionId));
    return std::string(translate(key, core::dimensionFallbackName(dimensionId), locale));
}

std::string dimensionsText(BiomeInfo const& biome, std::string_view locale) {
    std::string out;
    for (auto const& d : biome.dimensions) {
        if (!out.empty()) out += ", ";
        out += dimensionDisplayName(d, locale);
    }
    return out;
}

std::string tagsText(BiomeInfo const& biome, std::string_view locale) {
    std::string loc(locale);
    return core::tagsLine(
        biome.tags,
        "minecraft", // the tags are vanilla (`minecraft:is_*`) tags
        [&](std::string const& key) { return lookup(key, loc); },
        translate("string.naturescompass.none", "None", locale)
    );
}

std::string sourceText(BiomeInfo const& biome, std::string_view) { return core::sourceName(biome.source); }

std::string temperatureText(BiomeInfo const& biome) { return core::javaFloatToString(biome.temperature); }

std::string rainfallText(BiomeInfo const& biome) { return core::javaFloatToString(biome.downfall); }

} // namespace nl::feature::biomes

NL_REGISTER_MODULE(nl::feature::biomes::BiomesModule);
