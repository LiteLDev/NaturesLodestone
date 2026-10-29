// Selftests of unit U8 `showcase` (selftest dx 800..899): the units working together on one
// compass, through the contracts only (actions -> search -> compass data -> HUD lines), as a
// player would use it (docs/SPEC.md 3.2 - 3.7, 4.3).
//
// `showcase.flow_real_biomes` needs a world with real biomes (the `flow` server scenario runs a
// DEFAULT world); on the FLAT e2e world it skips itself. The other tests run anywhere.

#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/string/HashedString.h"
#include "mc/world/actor/player/LayeredAbilities.h"
#include "mc/world/actor/player/PermissionsHandler.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/crafting/Recipe.h"
#include "mc/world/item/crafting/Recipes.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/GameType.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/material/Material.h"

#include "core/HudLines.h"
#include "feature/actions/Actions.h"
#include "feature/compass/CompassConfig.h"
#include "feature/compass/CompassData.h"
#include "feature/compass/Search.h"
#include "feature/hud/HudModel.h"
#include "feature/search/SearchWorker.h"
#include "game/Items.h"
#include "mod/Config.h"
#include "selftest/SelfTest.h"

namespace nl::feature::showcase {
namespace {

using nl::selftest::Context;
using compass::CompassConfig;
using compass::CompassData;
using compass::CompassState;

constexpr int kOverworld = 0;

/// Restores the general config section, the player's permissions and game type when a test ends.
struct Guard {
    CompassConfig          config = nl::config::get<CompassConfig>();
    Player&                player;
    CommandPermissionLevel command;
    PlayerPermissionLevel  permission;
    GameType               gameType;
    explicit Guard(Player& p)
    : player(p),
      command(p.getAbilities().mPermissions->mCommandPermissions),
      permission(p.getAbilities().mPermissions->mPlayerPermissions),
      gameType(p.getPlayerGameType()) {}
    ~Guard() {
        nl::config::mut<CompassConfig>()                    = config;
        player.getAbilities().mPermissions->mPlayerPermissions  = permission;
        player.getAbilities().mPermissions->mCommandPermissions = command;
        player.setPlayerGameType(gameType);
    }
    void makeOperator() {
        player.getAbilities().mPermissions->mPlayerPermissions  = PlayerPermissionLevel::Operator;
        player.getAbilities().mPermissions->mCommandPermissions = CommandPermissionLevel::GameDirectors;
    }
};

int levelOf(Player const& player) {
    auto                     ref = player.getAttribute(Player::LEVEL());
    AttributeInstance const* ptr = ref.mPtr;
    return ptr ? static_cast<int>(ptr->mCurrentValue) : 0;
}

void setLevel(Player& player, int level) { player.addLevels(level - levelOf(player)); }

/// A fresh compass in the selected hotbar slot, as `/give` + selecting it would do.
compass::CompassSlot giveHeldCompass(Context& ctx, Player& player) {
    int selected = player.getSelectedItemSlot();
    ctx.require(ctx.give(player, compass::makeCompass(), selected) == selected, "compass in the selected slot");
    auto slot = compass::heldCompass(player);
    ctx.require(slot.has_value(), "the compass is held");
    return *slot;
}

CompassData heldData(Context& ctx, Player const& player) {
    auto slot = compass::heldCompass(player);
    ctx.require(slot.has_value(), "the player still holds the compass");
    return compass::dataAt(player, *slot).value_or(CompassData{});
}

/// Waits until the held compass's search has finished (NC's budget spreads it over ticks).
ll::coro::CoroTask<CompassData> waitDone(Context& ctx, Player& player, int maxTicks) {
    auto uid = heldData(ctx, player).uid;
    ctx.require(uid != 0, "the searching compass has a uid");
    for (int i = 0; i < maxTicks && search::isSearching(uid); i++) co_await ctx.ticks(1);
    ctx.require(!search::isSearching(uid), "the search finished in time");
    co_return heldData(ctx, player);
}

std::string lineText(std::vector<core::hud::Line> const& lines, int index) {
    for (auto const& l : lines)
        if (l.index == index) return l.text;
    return "<no line " + std::to_string(index) + ">";
}

bool singleBiomeOverworld(BlockPos const& at) {
    std::set<std::string> seen;
    for (int i = -2; i <= 2; i++)
        for (int j = -2; j <= 2; j++)
            seen.insert(search::sampleBiomeId(kOverworld, at.x + i * 1600, at.y, at.z + j * 1600));
    return seen.size() <= 1;
}

/// Some point of the straight line between two finds is another biome (they are separate
/// patches, as Search for Next promises).
bool separatedOnLine(std::string const& biome, compass::PrevPos const& a, compass::PrevPos const& b) {
    int steps = 64;
    for (int i = 1; i < steps; i++) {
        double t = static_cast<double>(i) / steps;
        int    x = static_cast<int>(a.x + (b.x - a.x) * t);
        int    z = static_cast<int>(a.z + (b.z - a.z) * t);
        bool   match = false;
        for (int y : search::yLevelsFor(kOverworld, a.y))
            if (search::sampleBiomeId(kOverworld, x, y, z) == biome) match = true;
        if (!match) return true;
    }
    return false;
}

bool isFree(BlockSource& region, BlockPos const& pos) {
    auto const& block = region.getBlock(pos);
    if (block.isAir() || block.getMaterial().mLiquid) return true;
    return block.getBlockType().canBeBuiltOver(block, region, pos);
}

} // namespace

// The whole NC flow in a world with real biomes: Search (XP), Found on the HUD, Search for Next
// (a separate patch), durability breaking after two successes, teleport, sneak reset.
NL_REGISTER_SELFTEST("showcase.flow_real_biomes", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& player = ctx.player();
    Guard guard(player);
    co_await ctx.ticks(1);
    if (singleBiomeOverworld(player.getFeetBlockPos())) {
        ctx.info("single-biome overworld (flat world): the real-biome flow runs in the flow e2e scenario");
        co_return;
    }
    auto& cfg              = nl::config::mut<CompassConfig>();
    cfg.compassDurability  = 2;
    cfg.defaultXpLevels    = 1;
    cfg.perBiomeXpLevels   = {};
    cfg.biomeBlacklist     = {};
    cfg.maxNextSearches    = 25;
    cfg.allowTeleport      = true;
    cfg.displayCoordinates = true;
    player.setPlayerGameType(GameType::Survival);
    guard.makeOperator();
    setLevel(player, 5);
    giveHeldCompass(ctx, player);
    std::string const biome = "minecraft:desert";

    // Search: pays 1 level, Searching at once, the HUD says so.
    actions::search(player, biome);
    ctx.check(levelOf(player) == 4, "Search takes the biome's level (5 -> " + std::to_string(levelOf(player)) + ")");
    auto d = heldData(ctx, player);
    ctx.require(d.state == CompassState::Searching && d.biomeId == biome, "Searching right after Search");
    auto lines = hud::linesFor(player, "en_US");
    ctx.check(lineText(lines, 0) == "Status" && lineText(lines, 1) == "Searching", "HUD: Status / Searching");
    ctx.check(lineText(lines, 3) == "Biome" && lineText(lines, 4) == "Desert", "HUD: Biome / Desert");

    // Found: the biome source agrees, the HUD shows coordinates and the live distance.
    d = co_await waitDone(ctx, player, 6000);
    ctx.require(d.state == CompassState::Found, "Found (state " + std::to_string(int(d.state)) + ")");
    ctx.require(d.prevPos.size() == 1, "one position");
    auto first = d.prevPos[0];
    ctx.check(search::sampleBiomeId(kOverworld, first.x, first.y, first.z) == biome, "desert at the found position");
    ctx.check(d.damage == 1 && !compass::isBroken(d), "one success: damage 1 of 2, not broken");
    lines     = hud::linesFor(player, "en_US");
    auto feet = player.getFeetBlockPos();
    ctx.check(lineText(lines, 1) == "Found", "HUD: Found");
    ctx.check(
        lineText(lines, 7) == std::to_string(d.foundX) + ", " + std::to_string(d.foundZ),
        "HUD: coordinates " + lineText(lines, 7)
    );
    ctx.check(
        lineText(lines, 10) == std::to_string(core::hud::horizontalDistance(feet.x, feet.z, d.foundX, d.foundZ)),
        "HUD: distance " + lineText(lines, 10)
    );
    ctx.info("desert at " + std::to_string(d.foundX) + ", " + std::to_string(d.foundZ));

    // Search for Next: another level, a separate desert, the second success breaks the compass.
    actions::searchForNext(player);
    ctx.check(levelOf(player) == 3, "Search for Next takes the level too");
    ctx.require(heldData(ctx, player).state == CompassState::Searching, "Search for Next started");
    d = co_await waitDone(ctx, player, 12000);
    ctx.require(d.state == CompassState::Found && d.biomeId == biome, "Search for Next: Found");
    ctx.require(d.prevPos.size() == 2 && d.prevPos[0] == first, "both positions kept, the first unchanged");
    auto next = d.prevPos[1];
    ctx.check(
        std::hypot(double(next.x - first.x), double(next.z - first.z)) > 2.0 * compass::sampleSpace(),
        "the next desert is not next to the first"
    );
    ctx.check(separatedOnLine(biome, first, next), "another biome lies between the two deserts");
    ctx.check(d.damage == 2 && compass::isBroken(d), "two successes: broken");
    ctx.info("next desert at " + std::to_string(next.x) + ", " + std::to_string(next.z));

    // Broken: no screen, no search, no XP; teleport and sneak reset still work.
    ctx.check(!actions::onUse(player, false), "a broken compass does not open the screen");
    actions::search(player, biome);
    actions::searchForNext(player);
    ctx.check(heldData(ctx, player).state == CompassState::Found && levelOf(player) == 3, "broken: nothing happens");

    ctx.require(actions::canTeleport(player), "the operator may teleport");
    actions::teleport(player);
    for (int i = 0; i < 25 * 20 && actions::teleportPending(player); i++) co_await ctx.ticks(1);
    ctx.require(!actions::teleportPending(player), "the teleport finished");
    co_await ctx.ticks(2);
    feet = player.getFeetBlockPos();
    ctx.check(feet.x == next.x && feet.z == next.z, "teleported into the found column");
    auto& region = player.getDimensionBlockSource();
    ctx.check(
        isFree(region, feet) && isFree(region, BlockPos{feet.x, feet.y + 1, feet.z})
            && !isFree(region, BlockPos{feet.x, feet.y - 1, feet.z}),
        "lands safely: feet and head free, solid below (y " + std::to_string(feet.y) + ")"
    );
    lines = hud::linesFor(player, "en_US");
    ctx.check(lineText(lines, 10) == "0", "HUD distance 0 after the teleport (" + lineText(lines, 10) + ")");

    ctx.check(actions::onUse(player, true), "sneak-use is handled");
    d = heldData(ctx, player);
    ctx.check(d.state == CompassState::Inactive && d.prevPos.empty(), "sneak-use resets the compass");
    ctx.check(d.damage == 2 && compass::isBroken(d), "the reset keeps the damage (still broken)");
    ctx.check(hud::linesFor(player, "en_US").empty(), "HUD: nothing for an Inactive compass");
});

// Durability and XP on any world: searches for the biome at the player succeed at once.
NL_REGISTER_SELFTEST("showcase.durability_and_xp", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& player = ctx.player();
    Guard guard(player);
    co_await ctx.ticks(1);
    auto& cfg             = nl::config::mut<CompassConfig>();
    cfg.compassDurability = 2;
    cfg.defaultXpLevels   = 2;
    cfg.perBiomeXpLevels  = {};
    cfg.biomeBlacklist    = {};
    auto start            = player.getFeetBlockPos();
    auto here             = search::sampleBiomeId(static_cast<int>(player.getDimensionId()), start.x, start.y, start.z);
    ctx.require(!here.empty(), "a biome at the player");

    // Survival: each search pays 2 levels, each success adds 1 damage, the second breaks it.
    player.setPlayerGameType(GameType::Survival);
    setLevel(player, 10);
    giveHeldCompass(ctx, player);
    for (int i = 1; i <= 2; i++) {
        actions::search(player, here);
        ctx.check(levelOf(player) == 10 - 2 * i, "survival search " + std::to_string(i) + " pays 2 levels");
        auto d = co_await waitDone(ctx, player, 200);
        ctx.check(d.state == CompassState::Found && d.damage == i, "success " + std::to_string(i) + ": damage " + std::to_string(d.damage));
    }
    ctx.check(compass::isBroken(heldData(ctx, player)), "broken after compassDurability successes");
    co_await ctx.ticks(12); // the bar follows `damage` within 10 ticks (docs/SPEC.md 3.7)
    ctx.check(
        compass::stackAt(player, *compass::heldCompass(player)).getDamageValue() == actions::kBarMaxDamage - 1,
        "the durability bar is empty (9999)"
    );
    actions::search(player, here);
    ctx.check(levelOf(player) == 6, "a broken compass takes no XP");

    // Creative: no XP, no damage.
    player.setPlayerGameType(GameType::Creative);
    co_await ctx.ticks(1);
    giveHeldCompass(ctx, player);
    actions::search(player, here);
    ctx.check(levelOf(player) == 6, "creative: no XP cost");
    auto d = co_await waitDone(ctx, player, 200);
    ctx.check(d.state == CompassState::Found && d.damage == 0, "creative: Found without damage");
    actions::onUse(player, true);
});

// The repair recipe (Nature's Compass + compass) gives a fresh compass: no damage, no data.
NL_REGISTER_SELFTEST("showcase.repair_recipe", [](Context& ctx) {
    auto level = ll::service::getLevel();
    ctx.require(level.has_value(), "level");
    int found = 0;
    level->getRecipes().forEachRecipeFor(HashedString("crafting_table"), [&](Recipe const& recipe) {
        if (recipe.mRecipeId.get() != "natureslodestone:repair_natures_compass") return;
        found++;
        auto const& results = recipe.getResultItems();
        // check, not require: never throw through the game's iteration
        ctx.check(results.size() == 1, "one result");
        if (results.empty()) return;
        ItemStackBase const& out = results[0];
        ctx.check(compass::isCompass(out), "the result is a Nature's Compass");
        ctx.check(!out.mUserData, "the result has no user data (no damage, no search data)");
        ctx.check(out.getDamageValue() == 0 && out.mCount == 1, "undamaged, one item");
        ctx.check(compass::readData(out) == CompassData{}, "reads as a fresh Inactive compass");
    });
    ctx.check(found == 1, "the repair recipe is registered once (" + std::to_string(found) + ")");
});

} // namespace nl::feature::showcase
