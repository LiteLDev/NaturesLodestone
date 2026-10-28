// Selftests of the compass unit (U1): item, data on real stacks, held compasses, config.
// Unit U1 owns selftest dx 100..199 (these tests place no blocks).

#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"

#include "feature/compass/Actions.h"
#include "feature/compass/CompassConfig.h"
#include "feature/compass/CompassData.h"
#include "feature/compass/Search.h"
#include "game/Items.h"
#include "mod/Config.h"
#include "selftest/SelfTest.h"

namespace {

using nl::selftest::Context;
namespace compass = nl::feature::compass;
using compass::CompassData;
using compass::CompassState;

CompassData sampleData() {
    CompassData d;
    d.state         = CompassState::Found;
    d.biomeId       = "minecraft:cherry_grove";
    d.foundX        = -1234;
    d.foundZ        = 5678;
    d.searchRadius  = 1500;
    d.samples       = 321;
    d.displayCoords = true;
    d.prevPos       = {
        {1,    64,  2   },
        {-300, -10, 4000}
    };
    d.damage = 3;
    d.uid    = 0x123456789abcLL;
    return d;
}

} // namespace

NL_REGISTER_SELFTEST("compass.make_item", [](Context& ctx) {
    auto vanilla = nl::game::makeItem("minecraft:compass", 2);
    ctx.check(!vanilla.isNull(), "makeItem makes a vanilla compass");
    ctx.check(nl::game::itemId(vanilla) == "minecraft:compass", "itemId is the full id");
    ctx.check(vanilla.mCount == 2, "makeItem keeps the count");
    ctx.check(nl::game::makeItem("natureslodestone:no_such_item").isNull(), "unknown ids give a null stack");
    auto nc = compass::makeCompass();
    ctx.require(!nc.isNull(), "natureslodestone:naturescompass is registered (behavior pack loaded)");
    ctx.check(compass::isCompass(nc), "isCompass(makeCompass())");
    ctx.check(!compass::isCompass(vanilla), "a vanilla compass is not a Nature's Compass");
    ctx.check(compass::readData(nc) == CompassData{}, "a new compass has no data (Inactive)");
});

NL_REGISTER_SELFTEST("compass.data_roundtrip", [](Context& ctx) {
    auto stack = compass::makeCompass();
    ctx.require(!stack.isNull(), "compass item exists");
    auto d = sampleData();
    compass::writeData(stack, d);
    ctx.require(stack.mUserData != nullptr, "user data written");
    ctx.check(compass::readData(stack) == d, "every field reads back");
    ItemStack copy = stack;
    ctx.check(compass::readData(copy) == d, "a copied stack keeps the data");
    compass::writeData(stack, CompassData{});
    ctx.check(compass::readData(stack) == CompassData{}, "writing default data clears it");
    ctx.check(stack.mUserData == nullptr || !stack.mUserData->contains(compass::kDataKey), "the data key is removed");
    // data on a non-compass stack is ignored
    auto vanilla = nl::game::makeItem("minecraft:compass");
    compass::writeData(vanilla, d);
    ctx.check(compass::readData(vanilla) == CompassData{}, "readData ignores non-compass stacks");
});

NL_REGISTER_SELFTEST("compass.clear_semantics", [](Context& ctx) {
    auto d = sampleData();
    d.startSearch("minecraft:plains");
    ctx.check(d.state == CompassState::Searching && d.biomeId == "minecraft:plains", "start: Searching + biome");
    ctx.check(d.foundX == 0 && d.foundZ == 0 && d.prevPos.empty() && !d.displayCoords, "start clears the rest");
    ctx.check(d.samples == 0 && d.searchRadius == 0, "start: samples and radius 0");
    ctx.check(d.damage == 3 && d.uid == 0x123456789abcLL, "start keeps damage and uid");

    d.succeed(
        "minecraft:plains",
        64,
        -128,
        {
            {64, 70, -128}
    },
        12,
        true
    );
    ctx.check(d.state == CompassState::Found && d.foundX == 64 && d.foundZ == -128, "success: Found at x/z");
    ctx.check(d.prevPos.size() == 1 && d.samples == 12 && d.displayCoords, "success: prevPos, samples, coords");
    ctx.check(d.searchRadius == 0, "success clears the radius");

    d.fail("minecraft:plains", 1999, 77);
    ctx.check(
        d.state == CompassState::NotFound && d.searchRadius == 1500 && d.samples == 77,
        "failure: radius floored"
    );
    ctx.check(d.prevPos.empty() && d.foundX == 0, "failure clears found data");

    d.clearSearchData();
    ctx.check(d.state == CompassState::Inactive && d.biomeId.empty(), "clear: Inactive");
    ctx.check(d.damage == 3 && d.uid == 0x123456789abcLL, "clear keeps damage and uid");

    ctx.check(
        compass::floorRadius(499) == 0 && compass::floorRadius(500) == 500 && compass::floorRadius(10999) == 10500,
        "floorRadius"
    );
    CompassData fresh;
    auto        uid = fresh.ensureUid();
    ctx.check(uid != 0 && fresh.ensureUid() == uid, "ensureUid gives one stable non-zero uid");
});

NL_REGISTER_SELFTEST("compass.held", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& player = ctx.player();
    co_await ctx.ticks(1);
    int selected = player.getSelectedItemSlot();
    ctx.check(!compass::heldCompass(player), "no compass held at first");

    auto stack = compass::makeCompass();
    ctx.require(ctx.give(player, stack, selected) == selected, "compass put into the selected slot");
    auto main = compass::heldCompass(player);
    ctx.require(main && main->hand == compass::Hand::Main && main->slot == selected, "main hand compass found");

    auto d = sampleData();
    ctx.require(compass::storeAt(player, *main, d), "storeAt writes the held compass");
    ctx.check(compass::dataAt(player, *main) == d, "dataAt reads it back");
    auto byUid = compass::findCompass(player, d.uid);
    ctx.check(byUid && *byUid == *main, "findCompass finds it by uid");
    ctx.check(!compass::findCompass(player, 0), "uid 0 never matches");

    // move it to another slot and into the off hand
    ItemStack moved = compass::stackAt(player, *main);
    int       other = (selected + 5) % 9 + 9; // a non-hotbar slot
    player.getInventory().setItem(selected, ItemStack::EMPTY_ITEM());
    player.getInventory().setItem(other, moved);
    ctx.check(!compass::heldCompass(player), "a compass in the inventory is not held");
    auto found = compass::findCompass(player, d.uid);
    ctx.check(
        found && found->hand == compass::Hand::Main && found->slot == other,
        "findCompass searches the inventory"
    );

    player.getInventory().setItem(other, ItemStack::EMPTY_ITEM());
    player.setOffhandSlot(moved);
    auto off = compass::heldCompass(player);
    ctx.require(off && off->hand == compass::Hand::Off, "off hand compass found");
    ctx.check(compass::dataAt(player, *off) == d, "off hand data reads back");
    d.clearSearchData();
    ctx.check(compass::storeAt(player, *off, d), "storeAt writes the off hand");
    ctx.check(compass::dataAt(player, *off) == d, "off hand write reads back");

    // main hand has priority
    ctx.give(player, compass::makeCompass(), selected);
    auto both = compass::heldCompass(player);
    ctx.check(both && both->hand == compass::Hand::Main, "main hand has priority over the off hand");
    player.setOffhandSlot(ItemStack::EMPTY_ITEM());
    ctx.check(
        !compass::storeAt(player, compass::CompassSlot{compass::Hand::Off, -1}, d),
        "storeAt fails on an empty slot"
    );
});

NL_REGISTER_SELFTEST("compass.config", [](Context& ctx) {
    compass::CompassConfig c;
    ctx.check(c.allowTeleport && c.maxNextSearches == 25 && c.displayCoordinates, "general defaults (1)");
    ctx.check(c.radiusModifier == 2500 && c.sampleSpaceModifier == 16 && c.maxSamples == 50000, "general defaults (2)");
    ctx.check(c.biomeBlacklist.empty() && c.perBiomeXpLevels.empty(), "general defaults (3)");
    ctx.check(c.defaultXpLevels == 0 && c.compassDurability == 0, "general defaults (4)");
    compass::CompassClientConfig cc;
    ctx.check(
        cc.displayWithChatOpen && cc.fixBiomeNames && cc.overlayLineOffset == 1 && cc.overlaySide == "LEFT",
        "client defaults"
    );

    // clamping on read
    auto saved                                                = nl::config::get<compass::CompassConfig>();
    nl::config::mut<compass::CompassConfig>().defaultXpLevels = 7;
    nl::config::mut<compass::CompassConfig>().maxNextSearches = -3;
    ctx.check(compass::defaultXpLevels() == 3, "defaultXpLevels clamps to 3");
    ctx.check(compass::maxNextSearches() == 0, "maxNextSearches clamps to 0");
    nl::config::mut<compass::CompassConfig>() = saved;
    ctx.check(
        compass::sampleSpace() == compass::kBiomeSize * compass::sampleSpaceModifier(),
        "sampleSpace = 4 * modifier"
    );

    auto savedClient                                            = nl::config::get<compass::CompassClientConfig>();
    nl::config::mut<compass::CompassClientConfig>().overlaySide = "right";
    ctx.check(compass::overlayOnRight(), "overlaySide is case-insensitive");
    nl::config::mut<compass::CompassClientConfig>().overlaySide = "middle";
    ctx.check(!compass::overlayOnRight(), "unknown overlaySide is LEFT");
    nl::config::mut<compass::CompassClientConfig>() = savedClient;

    // durability
    auto savedDur                                               = nl::config::get<compass::CompassConfig>();
    nl::config::mut<compass::CompassConfig>().compassDurability = 2;
    CompassData d;
    d.damage = 1;
    ctx.check(!compass::isBroken(d), "damage 1 of 2: not broken");
    d.damage = 2;
    ctx.check(compass::isBroken(d), "damage 2 of 2: broken");
    nl::config::mut<compass::CompassConfig>().compassDurability = 0;
    ctx.check(!compass::isBroken(d), "durability 0: never broken");
    nl::config::mut<compass::CompassConfig>() = savedDur;
});

NL_REGISTER_SELFTEST("compass.use_flow", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& player = ctx.player();
    co_await ctx.ticks(1);
    int selected = player.getSelectedItemSlot();
    ctx.require(ctx.give(player, compass::makeCompass(), selected) == selected, "compass given");
    nl::feature::actions::search(player, "minecraft:plains");
    auto slot = compass::heldCompass(player);
    ctx.require(slot.has_value(), "compass still held");
    auto d = compass::dataAt(player, *slot).value_or(CompassData{});
    ctx.check(d.state == CompassState::Searching, "search start writes Searching");
    ctx.check(d.biomeId == "minecraft:plains", "search writes the biome id");
    ctx.check(d.uid != 0, "search gives the compass a uid");
    ctx.check(nl::feature::search::isSearching(d.uid), "the search is running");
    nl::feature::actions::onUse(player, true); // sneak-use: reset (same tick)
    ctx.check(!nl::feature::search::isSearching(d.uid), "reset stops the search");
    auto after = compass::dataAt(player, *slot).value_or(sampleData());
    ctx.check(after.state == CompassState::Inactive && after.biomeId.empty(), "sneak-use resets the compass");
    ctx.check(after.uid == d.uid, "reset keeps the uid");
    co_await ctx.ticks(3);
    auto later = compass::dataAt(player, *slot).value_or(sampleData());
    ctx.check(later.state == CompassState::Inactive, "a stopped search writes nothing");
});
