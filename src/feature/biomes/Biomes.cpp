// Default (minimal) implementation of the biome catalog contract (src/feature/compass/Biomes.h).
// Unit U2 `biomes` replaces this file: real tags, hidden-from-locator filtering, translated
// biome and dimension names.

#include "feature/compass/Biomes.h"

#include <algorithm>
#include <cctype>
#include <charconv>

#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/string/HashedString.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/biome/Biome.h"
#include "mc/world/level/biome/registry/BiomeRegistry.h"
#include "mc/world/level/dimension/DimensionType.h"

#include "core/Glob.h"
#include "feature/compass/CompassConfig.h"
#include "game/WorldDb.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "mod/Module.h"

namespace nl::feature::biomes {

namespace {

std::vector<BiomeInfo> gCatalog;
bool                   gBuilt = false;

constexpr std::pair<int, std::string_view> kDimensions[] = {
    {0, "minecraft:overworld"},
    {1, "minecraft:nether"   },
    {2, "minecraft:the_end"  },
};

std::string fullId(std::string const& name) { return name.find(':') == std::string::npos ? "minecraft:" + name : name; }

std::string_view pathOf(std::string_view id) {
    auto colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

std::string_view namespaceOf(std::string_view id) {
    auto colon = id.find(':');
    return colon == std::string_view::npos ? std::string_view{"minecraft"} : id.substr(0, colon);
}

/// "snowy_plains" -> "Snowy Plains".
std::string capitalizeWords(std::string_view path) {
    std::string out;
    bool        start = true;
    for (char c : path) {
        if (c == '_') {
            out   += ' ';
            start  = true;
        } else {
            out   += start ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
            start  = false;
        }
    }
    return out;
}

void build() {
    gCatalog.clear();
    gBuilt     = true;
    auto level = ll::service::getLevel();
    if (!level) return;
    auto& registry = level->getBiomeRegistry();
    for (auto const& [dimId, dimName] : kDimensions) {
        for (auto const* biome : registry.getBiomesInDimension(DimensionType(dimId))) {
            if (!biome) continue;
            auto id = fullId(biome->mHash->getString());
            if (isBlacklisted(id)) continue;
            auto it = std::find_if(gCatalog.begin(), gCatalog.end(), [&](auto const& b) { return b.id == id; });
            if (it == gCatalog.end()) {
                BiomeInfo info;
                info.id          = id;
                info.source      = std::string(namespaceOf(id));
                info.temperature = biome->mTemperature;
                info.downfall    = biome->mDownfall;
                info.xpLevels    = xpLevelsFor(id);
                gCatalog.push_back(std::move(info));
                it = std::prev(gCatalog.end());
            }
            if (std::find(it->dimensions.begin(), it->dimensions.end(), dimName) == it->dimensions.end()) {
                it->dimensions.emplace_back(dimName);
            }
        }
    }
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
        auto comma = entry.find(',');
        if (comma == std::string::npos) continue;
        std::string_view glob(entry.data(), comma);
        std::string_view num = std::string_view(entry).substr(comma + 1);
        while (!glob.empty() && glob.back() == ' ') glob.remove_suffix(1);
        while (!glob.empty() && glob.front() == ' ') glob.remove_prefix(1);
        while (!num.empty() && num.front() == ' ') num.remove_prefix(1);
        while (!num.empty() && num.back() == ' ') num.remove_suffix(1);
        int levels   = 0;
        auto [p, ec] = std::from_chars(num.data(), num.data() + num.size(), levels);
        if (ec != std::errc{} || p != num.data() + num.size()) continue;
        if (core::globMatch(glob, id)) return std::clamp(levels, 0, 3);
    }
    return compass::defaultXpLevels();
}

std::string biomeDisplayName(std::string_view id, std::string_view locale, bool fix) {
    std::string key  = "biome." + std::string(namespaceOf(id)) + "." + std::string(pathOf(id));
    std::string name = std::string(translate(key, capitalizeWords(pathOf(id)), locale));
    if (!fix) return name;
    std::string out;
    for (size_t i = 0; i < name.size(); ++i) {
        if (i > 0 && std::isupper(static_cast<unsigned char>(name[i]))
            && std::islower(static_cast<unsigned char>(name[i - 1])))
            out += ' ';
        out += name[i];
    }
    return out;
}

std::string dimensionDisplayName(std::string_view dimensionId, std::string_view locale) {
    std::string key = "dimension." + std::string(namespaceOf(dimensionId)) + "." + std::string(pathOf(dimensionId));
    std::string fallback = dimensionId == "minecraft:nether"  ? "The Nether"
                         : dimensionId == "minecraft:the_end" ? "The End"
                                                              : capitalizeWords(pathOf(dimensionId));
    return std::string(translate(key, fallback, locale));
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
    std::vector<std::string> names;
    for (auto const& t : biome.tags) names.push_back(capitalizeWords(t));
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    if (names.empty()) return std::string(translate("string.naturescompass.none", "None", locale));
    std::string out;
    for (auto const& n : names) {
        if (!out.empty()) out += ", ";
        out += n;
    }
    return out;
}

std::string sourceText(BiomeInfo const& biome, std::string_view) {
    if (biome.source == "minecraft") return "Minecraft";
    return biome.source;
}

} // namespace nl::feature::biomes

NL_REGISTER_MODULE(nl::feature::biomes::BiomesModule);
