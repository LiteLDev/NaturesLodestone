#pragma once

// Contract: biome searches (owner: unit U3 `search`, implementation in src/feature/search/).
// docs/SPEC.md section 3.3.
//
// One cooperative worker per compass (keyed by `CompassData::uid`), run on the server thread
// within NC's per-tick budget, oldest first. A worker finds its compass again every time it
// writes (`compass::findCompass(player, uid)`); a compass that is gone (dropped, stored) is
// skipped and the search runs on (its end logs a warning), a player who is offline stops it
// without a write (docs/SPEC.md 3.3). These functions do no permission, XP or durability checks: callers
// (src/feature/actions) do them first.

#include <cstdint>
#include <string>

#include "feature/compass/CompassData.h"

class BlockPos;
class Player;

namespace nl::feature::search {

/// Starts a search for `biomeId` from `start` (block position; its y is the first y level) in
/// the player's current dimension, for the compass at `slot`. Stops any search of that compass,
/// gives the compass a uid if needed, writes the "search start" data (state Searching) and
/// returns true. False (nothing changed) if `slot` holds no compass.
bool startSearch(Player& player, compass::CompassSlot slot, std::string const& biomeId, BlockPos const& start);

/// "Search for Next": a new search for the compass's Found biome from the player's position,
/// skipping matches connected to any of its `prevPos`. False (nothing changed) if `slot` holds no
/// compass or the compass is not Found.
bool startSearchForNext(Player& player, compass::CompassSlot slot);

/// Stops the search of the compass with `uid` without writing anything (sneak-use, new search).
void stopSearch(std::int64_t uid);

/// A search of the compass with `uid` is running.
[[nodiscard]] bool isSearching(std::int64_t uid);

/// Stops every search (world closed, mod disabled).
void stopAll();

} // namespace nl::feature::search
