#pragma once

// The HUD lines of a player's held compass (docs/SPEC.md 4.3), shared by the server fallback
// (src/feature/hud/Hud.cpp) and the client's native overlay (src/client/hud/). Works on the server
// players and on the client's LocalPlayer (it reads only the held stacks and the position).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/HudLines.h"
#include "feature/compass/CompassData.h"

class Player;

namespace nl::feature::hud {

/// The NC labels translated into `locale` ("" = default locale).
[[nodiscard]] core::hud::Labels labelsFor(std::string_view locale);

/// The model input for `data` seen from block position (`playerX`, `playerZ`).
[[nodiscard]] core::hud::Input
inputFor(compass::CompassData const& data, std::string_view locale, int playerX, int playerZ);

/// NC's lines for the compass `player` holds (main hand first, then off hand); empty when the
/// player holds no compass or it is Inactive.
[[nodiscard]] std::vector<core::hud::Line> linesFor(Player const& player, std::string_view locale);

/// The fallback string for `lines` with the server's naturescompass_client side and offset.
[[nodiscard]] std::string fallbackText(std::vector<core::hud::Line> const& lines);

namespace detail {
/// The fallback string last sent to `player` (nullopt: nothing shown / never sent).
[[nodiscard]] std::optional<std::string> lastSent(Player const& player);
/// How many fallback strings were sent to `player` since it joined.
[[nodiscard]] std::size_t sendCount(Player const& player);
/// Recomputes and, if it changed, sends `player`'s fallback string now.
void update(Player& player);
} // namespace detail

} // namespace nl::feature::hud
