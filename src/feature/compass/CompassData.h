#pragma once

// The Nature's Compass item and its per-stack data (docs/SPEC.md sections 2 and 3).
//
// All search state lives on the ItemStack itself, in its user data (a CompoundTag under the
// key "nl:compass"), so it travels with the item (drops, chests, the off hand) and reaches the
// client in the normal inventory sync (the needle reads it there).
//
// Server thread only. Never keep `Player*` / `ItemStack&` across ticks: remember the compass
// with `CompassData::uid` + the player's unique id and look it up again with `findCompass`.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ItemStack;
class ItemStackBase;
class Player;

namespace nl::feature::compass {

/// `natureslodestone:naturescompass`.
inline constexpr std::string_view kItemId = "natureslodestone:naturescompass";
/// User-data key of the compound holding `CompassData`.
inline constexpr std::string_view kDataKey = "nl:compass";

/// NC's compass_state (the numbers are stored).
enum class CompassState : int {
    Inactive  = 0,
    Searching = 1,
    Found     = 2,
    NotFound  = 3,
};

/// A position found by an earlier search (NC prev_pos: x/z of the sample with its matching y).
struct PrevPos {
    int x = 0;
    int y = 0;
    int z = 0;

    friend bool operator==(PrevPos const&, PrevPos const&) = default;
};

/// Everything NC stores on a compass, plus our `uid`. Missing data = a default value (Inactive).
///
/// The NC "clear search data" semantics are the member functions: each one wipes every search
/// field first (never `damage` / `uid`) and then sets what NC sets.
struct CompassData {
    CompassState         state = CompassState::Inactive;
    std::string          biomeId;               ///< "minecraft:plains"
    int                  foundX        = 0;     ///< x of the matching sample (Found)
    int                  foundZ        = 0;     ///< z of the matching sample (Found)
    int                  searchRadius  = 0;     ///< progress / final radius, a multiple of 500
    int                  samples       = 0;     ///< samples taken (written at the end of a search)
    bool                 displayCoords = false; ///< server displayCoordinates at success time
    std::vector<PrevPos> prevPos;               ///< every position found so far (Search for Next)
    int                  damage = 0;            ///< successful searches (durability)
    /// Ours: a random non-zero id given to the compass at its first search (`ensureUid`), so a
    /// search worker finds the exact compass instance again in later ticks. 0 = none yet.
    std::int64_t uid = 0;

    /// Wipes every search field (state Inactive). Keeps `damage` and `uid`. (Sneak-use reset.)
    void clearSearchData();
    /// Search start (also "Search for Next"): clear, then state Searching, `biomeId`, samples 0,
    /// radius 0. Like NC this drops `prevPos`: a Search for Next worker copies it first and writes
    /// it back (with the new position appended) on success.
    void startSearch(std::string biome);
    /// Success: clear, then state Found, biome, found x/z, prevPos (already holding the new
    /// position), samples, displayCoords. Damage is separate (`addDamage`), as it depends on the
    /// player (no damage in creative).
    void succeed(std::string biome, int x, int z, std::vector<PrevPos> positions, int sampleCount, bool showCoords);
    /// Failure: clear, then state NotFound, biome, radius (floored to a multiple of 500), samples.
    void fail(std::string biome, int radius, int sampleCount);
    /// Gives the compass a uid if it has none. Returns the uid.
    std::int64_t ensureUid();

    friend bool operator==(CompassData const&, CompassData const&) = default;
};

/// `radius` floored to a multiple of 500 (NC search_radius).
[[nodiscard]] constexpr int floorRadius(int radius) { return radius < 0 ? 0 : radius / 500 * 500; }

/// A fresh random non-zero uid.
[[nodiscard]] std::int64_t newUid();

// ---- stacks -------------------------------------------------------------------------------------

/// The stack is a Nature's Compass.
[[nodiscard]] bool isCompass(ItemStackBase const& stack);
/// A new compass with no data.
[[nodiscard]] ItemStack makeCompass();
/// The compass data of `stack` (defaults when absent or not a compass).
[[nodiscard]] CompassData readData(ItemStackBase const& stack);
/// Writes `data` into `stack`'s user data (other user data, e.g. a custom name, is kept).
/// Writing a default `CompassData` removes the key.
void writeData(ItemStackBase& stack, CompassData const& data);

/// Durability (config compassDurability, 0 = unbreakable): damage >= durability.
[[nodiscard]] bool isBroken(CompassData const& data);
[[nodiscard]] bool isBroken(ItemStackBase const& stack);

// ---- compasses held by players ------------------------------------------------------------------

enum class Hand { Main, Off };

/// Where a compass is in a player's inventory. Valid for the current tick only (slots change);
/// across ticks use `findCompass(player, uid)`.
struct CompassSlot {
    Hand hand = Hand::Main;
    /// Inventory slot (main hand: the selected hotbar slot; other inventory slots for
    /// `findCompass`). -1 for the off hand.
    int slot = -1;

    friend bool operator==(CompassSlot const&, CompassSlot const&) = default;
};

/// The compass the player holds: main hand first, then off hand (NC priority). nullopt if none.
[[nodiscard]] std::optional<CompassSlot> heldCompass(Player const& player);
/// The compass with `uid`: the held ones first, then the rest of the inventory. nullopt if the
/// player no longer has it (dropped, stored, ...). `uid` 0 never matches.
[[nodiscard]] std::optional<CompassSlot> findCompass(Player const& player, std::int64_t uid);
/// The stack at `slot` (an empty stack if the slot is out of range).
[[nodiscard]] ItemStack const& stackAt(Player const& player, CompassSlot slot);
/// Reads the data of the compass at `slot` (nullopt if that slot no longer holds a compass).
[[nodiscard]] std::optional<CompassData> dataAt(Player const& player, CompassSlot slot);
/// Writes `data` into the compass at `slot` and syncs the inventory to the client. Returns false
/// (and changes nothing) if the slot no longer holds a compass.
bool storeAt(Player& player, CompassSlot slot, CompassData const& data);

} // namespace nl::feature::compass
