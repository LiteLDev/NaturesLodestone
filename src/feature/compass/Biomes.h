#pragma once

// Contract: the biome catalog (owner: unit U2 `biomes`, implementation in src/feature/biomes/).
// docs/SPEC.md sections 3.6 (catalog) and 4.1 (display helpers).
//
// Server thread. The catalog is rebuilt for every world (`game::db::onWorldReady`) and after
// `/naturescompass reload` (blacklist / XP costs), so do not keep `BiomeInfo` pointers across ticks.

#include <string>
#include <string_view>
#include <vector>

namespace nl::feature::biomes {

/// One searchable biome.
struct BiomeInfo {
    std::string id; ///< "minecraft:plains"
    /// Dimensions whose generator can produce the biome, as ids ("minecraft:overworld",
    /// "minecraft:nether", "minecraft:the_end"); may be empty.
    std::vector<std::string> dimensions;
    /// Raw NC tag names (the `is_*` tags minus `is_overworld`, `is_` stripped, cut at '/'), e.g.
    /// {"forest"}. Display text: `tagsText`.
    std::vector<std::string> tags;
    std::string              source; ///< the id's namespace ("minecraft"). Display: `sourceText`.
    float                    temperature = 0.f;
    float                    downfall    = 0.f; ///< NC "Rainfall"
    int                      xpLevels    = 0;   ///< search cost (config), 0..3
};

/// Every biome the compass can search for: all registered biomes minus the config blacklist and
/// biomes hidden from locators, in registry order (the GUI sorts).
[[nodiscard]] std::vector<BiomeInfo> const& searchableBiomes();

/// The catalog entry of `id` (nullptr if unknown or not searchable).
[[nodiscard]] BiomeInfo const* findBiome(std::string_view id);

/// True if the config blacklist excludes `id`.
[[nodiscard]] bool isBlacklisted(std::string_view id);

/// XP levels a search for `id` costs (perBiomeXpLevels, else defaultXpLevels; 0..3).
[[nodiscard]] int xpLevelsFor(std::string_view id);

/// The biome's display name in `locale` ("" = default locale). With `fix` the NC
/// fixBiomeNames rule inserts missing spaces ("ForestHills" -> "Forest Hills").
[[nodiscard]] std::string biomeDisplayName(std::string_view id, std::string_view locale = {}, bool fix = false);

/// A dimension's display name ("Overworld", "The Nether", "The End") in `locale`.
[[nodiscard]] std::string dimensionDisplayName(std::string_view dimensionId, std::string_view locale = {});

/// The "Dimension:" value of a GUI row: display names joined with ", " ("" if none).
[[nodiscard]] std::string dimensionsText(BiomeInfo const& biome, std::string_view locale = {});

/// The "Tags:" value of a GUI row (NC rules; "None" when there are none).
[[nodiscard]] std::string tagsText(BiomeInfo const& biome, std::string_view locale = {});

/// The "Source:" value of a GUI row ("Minecraft" for vanilla biomes).
[[nodiscard]] std::string sourceText(BiomeInfo const& biome, std::string_view locale = {});

} // namespace nl::feature::biomes
