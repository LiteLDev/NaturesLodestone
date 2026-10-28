#pragma once

// Animated Nature's Compass needle (client build only; docs/SPEC.md 3.8). The hooks live in
// NeedleHook.cpp; these functions are what they compute, for selftests.

#ifdef NL_CLIENT

#include <string_view>

class ItemStack;
class Mob;

namespace nl::client::needle {

/// The needle frame (0..31) of `stack` for `holder` (nullptr or `asItemEntity`: no living holder,
/// the needle spins). Advances the shared wobble at most once per game tick.
[[nodiscard]] int frameFor(Mob* holder, bool asItemEntity, ItemStack const* stack);

/// The entry of the `naturescompass` texture array that shows needle `frame` (1 + frame; entry 0 is
/// the plain icon). Out-of-range frames show the needle pointing up.
[[nodiscard]] int iconIndexFor(int frame);

/// The texture key (`item_texture.d/needle.json`).
[[nodiscard]] std::string_view iconName();

} // namespace nl::client::needle

#endif // NL_CLIENT
