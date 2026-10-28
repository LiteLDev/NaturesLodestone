#pragma once

// Unit U4 `actions`: internals shared by Actions.cpp and its selftests. Other units use the
// contract (src/feature/compass/Actions.h).

#include "feature/compass/Actions.h"

class BlockSource;

namespace nl::feature::actions {

/// The item's `max_durability` (assets/pack/bp/items/naturescompass.json): the durability bar
/// shows `damage / compassDurability` of it.
inline constexpr int kBarMaxDamage = 10000;

/// NC's teleport height in the column x/z of `region`, probing from `start` (docs/SPEC.md 3.5).
/// The column must be loaded.
[[nodiscard]] int safeTeleportY(BlockSource& region, int start, int x, int z);

/// The player's teleport waits for its target column to load.
[[nodiscard]] bool teleportPending(Player const& player);

/// A use event of a held compass (air or block): the first event of a click calls `onUse` on the
/// next tick; events following within a few ticks (held button, block + air event of one click)
/// are ignored. Returns true when the event starts a new click.
bool handleUseEvent(Player& player, bool sneaking);

} // namespace nl::feature::actions
