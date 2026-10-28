#pragma once

// The two configuration sections of Nature's Compass (docs/SPEC.md section 5).
//
//   config/naturescompass.json         CompassConfig        (NC "General", server side)
//   config/naturescompass_client.json  CompassClientConfig  (NC "Client"; read by the client build in
//                                                            local worlds, and on BDS by the server
//                                                            for the fallback HUD)
//
// Read the values through the clamping helpers below (`nl::config::get<...>()` returns the raw file
// values, which people can set out of range).

#include <string>
#include <vector>

namespace nl::feature::compass {

struct CompassConfig {
    int version = 1;
    /// Allows a player to teleport to a located biome when opped, or in cheat mode.
    bool allowTeleport = true;
    /// 0..10000. 0 hides "Search for Next" (the compass always locates the nearest biome).
    int maxNextSearches = 25;
    /// Show the precise coordinates and distance of a located biome on the HUD.
    bool displayCoordinates = true;
    /// 0..1000000. biomeSize (4) * radiusModifier = maximum search radius in blocks.
    int radiusModifier = 2500;
    /// 0..1000000. biomeSize (4) * sampleSpaceModifier = blocks between two samples.
    int sampleSpaceModifier = 16;
    /// Biome ids the compass cannot search for; `*` and `?` wildcards (anchored).
    std::vector<std::string> biomeBlacklist;
    /// 0..3. XP levels consumed by a search unless overridden by perBiomeXpLevels.
    int defaultXpLevels = 0;
    /// "biome_glob,levels" entries (levels capped at 3); the first matching entry wins.
    std::vector<std::string> perBiomeXpLevels;
    /// 0..1000000. Maximum number of samples taken by one search.
    int maxSamples = 50000;
    /// 0..10000. Successful searches before the compass breaks; 0 disables durability.
    int compassDurability = 0;
};

struct CompassClientConfig {
    int version = 1;
    /// Show the HUD lines while the chat screen is open.
    bool displayWithChatOpen = true;
    /// Insert missing spaces into biome names in the Select Biome list ("ForestHills" -> "Forest Hills").
    bool fixBiomeNames = true;
    /// 0..50. Line offset of the HUD lines.
    int overlayLineOffset = 1;
    /// "LEFT" or "RIGHT" (case-insensitive; anything else is LEFT).
    std::string overlaySide = "LEFT";
};

/// NC's hard-coded biome size: every distance modifier is multiplied by it.
inline constexpr int kBiomeSize = 4;

/// Clamped values of `config::get<CompassConfig>()` (ranges of docs/SPEC.md section 5).
[[nodiscard]] CompassConfig const& rawConfig();
[[nodiscard]] bool                 allowTeleport();
[[nodiscard]] int                  maxNextSearches();
[[nodiscard]] bool                 displayCoordinates();
[[nodiscard]] int                  radiusModifier();
[[nodiscard]] int                  sampleSpaceModifier();
[[nodiscard]] int                  defaultXpLevels();
[[nodiscard]] int                  maxSamples();
[[nodiscard]] int                  compassDurability();
/// biomeSize * radiusModifier (blocks; 0 = searches fail immediately).
[[nodiscard]] int maxSearchRadius();
/// biomeSize * sampleSpaceModifier (blocks; 0 = searches fail immediately).
[[nodiscard]] int sampleSpace();

/// Clamped values of `config::get<CompassClientConfig>()`.
[[nodiscard]] CompassClientConfig const& rawClientConfig();
[[nodiscard]] bool                       displayWithChatOpen();
[[nodiscard]] bool                       fixBiomeNames();
[[nodiscard]] int                        overlayLineOffset();
/// True when overlaySide is "RIGHT" (case-insensitive).
[[nodiscard]] bool overlayOnRight();

} // namespace nl::feature::compass
