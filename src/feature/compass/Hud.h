#pragma once

// Contract: the HUD lines (owner: unit U6 `hud`, implementation in src/feature/hud/ and
// src/client/hud/). docs/SPEC.md section 4.3.
//
// The HUD follows the held compass by itself every tick; writers of compass data call
// `refresh` so a change shows at once instead of on the next update.

class Player;

namespace nl::feature::hud {

/// The compass data of `player` changed: redraw now.
void refresh(Player& player);

} // namespace nl::feature::hud
