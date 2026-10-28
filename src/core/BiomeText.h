#pragma once

// Pure text rules of the biome catalog (docs/SPEC.md sections 3.6 and 4.1): Nature's Compass
// display helpers and the mapping of Bedrock biome data onto NC's Java concepts.

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nl::core {

/// "minecraft:plains" -> "minecraft"; an id without ':' is in the "minecraft" namespace.
[[nodiscard]] std::string_view idNamespace(std::string_view id);
/// "minecraft:plains" -> "plains".
[[nodiscard]] std::string_view idPath(std::string_view id);
/// "plains" -> "minecraft:plains"; ids with a namespace are returned unchanged.
[[nodiscard]] std::string fullBiomeId(std::string_view id);

/// NC fixBiomeNames: a space before every uppercase letter that follows a lowercase one
/// ("ForestHills" -> "Forest Hills"). ASCII letters only.
[[nodiscard]] std::string fixBiomeName(std::string_view name);

/// NC's fallback for untranslated paths: '_' -> ' ' and every word capitalised
/// ("snowy_plains" -> "Snowy Plains", "the_nether" -> "The Nether").
[[nodiscard]] std::string capitalizePath(std::string_view path);

/// Java `Float.toString` (JDK 19+: shortest representation that reads back to the same float,
/// at least one fractional digit; scientific "1.0E7" form outside [1e-3, 1e7)).
[[nodiscard]] std::string javaFloatToString(float value);

/// The NC tag names (Java `is_*` tags minus `is_overworld`, `is_` stripped) of a biome, derived
/// from its Bedrock tags so that the Tags line reads like NC's on Java. Fixed order (alphabetical).
///
///   badlands   <- mesa                       mountain <- mountains, except grove
///   beach      <- beach, except stone        nether   <- nether
///   deep_ocean <- ocean + deep               ocean    <- ocean
///   end        <- the_end                    river    <- river
///   forest     <- forest / flower_forest / grove / pale_garden, except taiga and extreme_hills
///   hill       <- extreme_hills              savanna  <- savanna
///   jungle     <- jungle                     taiga    <- taiga
[[nodiscard]] std::vector<std::string> ncTagsFromBedrock(std::vector<std::string> const& bedrockTags);

/// The dimensions (ids "minecraft:overworld", "minecraft:nether", "minecraft:the_end") a biome
/// belongs to according to its Bedrock tags (`overworld` / `overworld_generation`, `nether`,
/// `the_end`), in that order.
[[nodiscard]] std::vector<std::string> dimensionsFromBedrockTags(std::vector<std::string> const& bedrockTags);

/// True for vanilla Bedrock biomes that are still registered (old worlds) but that no current
/// world generator produces (pre-1.18 "hills" / "mutated" variants, legacy frozen ocean, ...).
/// They are hidden from the compass, like NC's `c:hidden_from_locator_selection`. `path` is the
/// id path in the minecraft namespace.
[[nodiscard]] bool isLegacyBedrockBiome(std::string_view path);

/// Returns the translation of `key`, or nullopt when there is none.
using TranslationLookup = std::function<std::optional<std::string>(std::string const& key)>;

/// NC's Tags line: for each tag (in order) the translation of `biome.<ns>.<tag>` is returned at
/// once if there is one, else that of `category.<ns>.<tag>`; else the capitalised tag goes into a
/// sorted set. The set joined with ", ", or `noneText` when empty.
[[nodiscard]] std::string tagsLine(
    std::vector<std::string> const& tags,
    std::string_view                ns,
    TranslationLookup const&        lookup,
    std::string_view                noneText
);

/// The fallback name of a dimension id: "minecraft:nether" -> "The Nether", otherwise the path
/// capitalised ("minecraft:overworld" -> "Overworld", "minecraft:the_end" -> "The End").
[[nodiscard]] std::string dimensionFallbackName(std::string_view dimensionId);

/// NC's Source line: "Minecraft" for the minecraft namespace, else the namespace itself.
[[nodiscard]] std::string sourceName(std::string_view ns);

/// Parses a perBiomeXpLevels entry "glob,levels" (whitespace around both parts ignored; levels
/// clamped to 0..3). nullopt for malformed entries (no comma, more than one comma, empty glob,
/// levels not an integer).
[[nodiscard]] std::optional<std::pair<std::string, int>> parseXpEntry(std::string_view entry);

} // namespace nl::core
