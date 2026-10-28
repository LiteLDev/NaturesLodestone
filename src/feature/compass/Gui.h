#pragma once

// Contract: the Select Biome screen (owner: unit U5 `gui`, implementation in src/feature/gui/).
// docs/SPEC.md section 4.2.
//
// A server-driven form, so it works in local worlds and for modded and vanilla clients on BDS.
// Its buttons call the server actions (src/feature/compass/Actions.h).

class Player;

namespace nl::feature::gui {

/// Opens the Select Biome screen for the compass the player holds (the NC "sync" happens here:
/// the screen reads the catalog, permissions and the compass state when it opens). Does nothing
/// if the player holds no compass.
void openSelectBiome(Player& player);

} // namespace nl::feature::gui
