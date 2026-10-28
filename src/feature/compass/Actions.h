#pragma once

// Contract: what a player does with the compass (owner: unit U4 `actions`, implementation in
// src/feature/actions/). docs/SPEC.md sections 3.2, 3.4 and 3.5.
//
// The equivalent of NC's item use and its three client->server packets. Each action works on
// the compass the player holds (main hand first, then off hand) at the time of the call. Server
// thread.

#include <string>

class BlockPos;
class Player;

namespace nl::feature::actions {

/// Item use (right click). Sneaking: stop the search and reset the compass (also when broken).
/// Otherwise: nothing when broken, else open the Select Biome screen. Returns true when the use
/// was handled (NC CONSUME / SUCCESS), false for NC PASS.
bool onUse(Player& player, bool sneaking);

/// "Search" (NC search packet): unless broken, takes the biome's XP cost (not in creative) and
/// starts a search for `biomeId` from the player's block position.
void search(Player& player, std::string const& biomeId);

/// "Search" from an explicit start position (block position; its y is the first y level), e.g.
/// the position the player had when the screen sent the request. Same rules as `search`.
void search(Player& player, std::string const& biomeId, BlockPos const& start);

/// "Search for Next": unless broken and only when the compass is Found and maxNextSearches > 0,
/// takes the XP cost and searches for the next instance of the Found biome.
void searchForNext(Player& player);

/// "Teleport": when `canTeleport` and the compass is Found, teleports the player to the found
/// x/z at a safe height in the current dimension. Logs a denial otherwise.
void teleport(Player& player);

/// Teleport permission: config allowTeleport && (operator || world cheats enabled).
[[nodiscard]] bool canTeleport(Player const& player);

/// NC "infinite materials" (creative): no XP cost, no durability damage.
[[nodiscard]] bool hasInfiniteXp(Player const& player);

/// The player can pay `levels` XP levels (or has infinite XP).
[[nodiscard]] bool canAfford(Player const& player, int levels);

/// Mirrors the `damage` of every compass the player carries onto its Bedrock durability bar
/// (docs/SPEC.md 3.7). Runs by itself every few ticks; writers of `damage` may call it to show a
/// change at once.
void refreshDurability(Player& player);

} // namespace nl::feature::actions
