#pragma once

// Internals of the search unit shared with its selftests (not a contract: other units use
// src/feature/compass/Search.h).

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "feature/compass/CompassData.h"

class BlockPos;
class Player;

namespace nl::feature::search {

/// The biome id ("minecraft:plains") the generator of dimension `dimId` (0 overworld, 1 nether,
/// 2 end) produces at the block position, looked up at quart resolution without loading chunks.
/// "" when the dimension or its generator is not available.
[[nodiscard]] std::string sampleBiomeId(int dimId, int x, int y, int z);

/// NC's y levels of a search from `startY` in dimension `dimId` (empty if the dimension is missing).
[[nodiscard]] std::vector<int> yLevelsFor(int dimId, int startY);

/// Starts a search like `startSearch` but in dimension `dimId` and with the given previous
/// positions (non-empty = Search for Next). Returns false if `slot` holds no compass.
bool startWorker(
    Player&                              player,
    compass::CompassSlot                 slot,
    std::string const&                   biomeId,
    BlockPos const&                      start,
    int                                  dimId,
    std::vector<compass::PrevPos> const& prevPos
);

/// Running workers, in run order (compass uids).
[[nodiscard]] std::vector<std::int64_t> runningUids();

} // namespace nl::feature::search
