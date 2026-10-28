#pragma once

// Pure helpers of the compass actions (docs/SPEC.md sections 3.5 and 3.7). No game headers.

#include <algorithm>
#include <cmath>

namespace nl::core {

/// NC's fallback teleport height when no probe is valid.
inline constexpr int kFallbackTeleportY = 256;

/// NC's teleport height search over one column.
///
/// `isFree(y)` tells whether the block at height `y` is "free" (air, fire, liquid or replaceable);
/// heights outside the build range [`minY`, `maxY`) are treated as free (they read as air).
/// A height is valid when its feet (y) and head (y + 1) blocks are free and the block below is
/// not. Starting at `start`, the up probe (start + k) and the down probe (start - k) move one block
/// per step while either is inside the build range and neither is valid. The up probe wins over
/// the down probe. When neither is valid, returns `kFallbackTeleportY` clamped to `maxY`.
template <class IsFree>
[[nodiscard]] int findTeleportY(int start, int minY, int maxY, IsFree&& isFree) {
    auto inside = [&](int y) { return y >= minY && y < maxY; };
    auto free   = [&](int y) { return !inside(y) || static_cast<bool>(isFree(y)); };
    auto valid  = [&](int y) { return free(y) && free(y + 1) && !free(y - 1); };
    int  up     = start;
    int  down   = start;
    while ((inside(up) || inside(down)) && !valid(up) && !valid(down)) {
        ++up;
        --down;
    }
    if (valid(up)) return up;
    if (valid(down)) return down;
    return std::min(kFallbackTeleportY, maxY);
}

/// The Bedrock damage value that shows the compass durability bar with NC's fraction:
/// `damage / durability` of `maxDamage` (the item's max_durability), at least 1 while damaged so
/// the bar shows. It stays below `maxDamage` (a broken compass gets `maxDamage - 1`: an empty bar,
/// while a full damage value would let a stray vanilla durability hit destroy the item).
/// 0 (no bar) when durability is disabled or there is no damage.
[[nodiscard]] inline int durabilityBarDamage(int damage, int durability, int maxDamage) {
    if (durability <= 0 || damage <= 0 || maxDamage <= 1) return 0;
    if (damage >= durability) return maxDamage - 1;
    auto scaled = static_cast<int>(std::lround(static_cast<double>(maxDamage) * damage / durability));
    return std::clamp(scaled, 1, maxDamage - 1);
}

} // namespace nl::core
