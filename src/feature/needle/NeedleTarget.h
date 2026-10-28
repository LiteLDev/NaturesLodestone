#pragma once

// What the compass needle (docs/SPEC.md 3.8) needs from a compass stack: state Found -> the found
// x/z, anything else -> the world spawn. Read straight from the stack's user data without
// allocating, because the client reads it for every rendered compass every frame.

class ItemStackBase;

namespace nl::feature::needle {

struct NeedleTarget {
    bool found = false; ///< state Found: point at (x, 0, z); otherwise at the world spawn
    int  x     = 0;
    int  z     = 0;

    friend bool operator==(NeedleTarget const&, NeedleTarget const&) = default;
};

/// The needle target stored on `stack` (any thread; `stack` must not change meanwhile). The caller
/// checks that `stack` is a Nature's Compass.
[[nodiscard]] NeedleTarget needleTarget(ItemStackBase const& stack);

} // namespace nl::feature::needle
