// Selftests of unit U4 `actions` (selftest dx 400..499): use / sneak reset, broken compasses, XP
// costs, the durability bar, teleport permission, NC's teleport height and the teleport itself.

#include <cmath>

#include "ll/api/service/Bedrock.h"

#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/LayeredAbilities.h"
#include "mc/world/actor/player/PermissionsHandler.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/GameType.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"

#include "feature/actions/Actions.h"
#include "feature/compass/Biomes.h"
#include "feature/compass/CompassConfig.h"
#include "feature/compass/CompassData.h"
#include "feature/compass/Search.h"
#include "mod/Config.h"
#include "selftest/SelfTest.h"

namespace {

using nl::selftest::Context;
namespace compass = nl::feature::compass;
namespace actions = nl::feature::actions;
using compass::CompassConfig;
using compass::CompassData;
using compass::CompassState;

/// Restores the general config section when a test ends (also on abort).
struct ConfigGuard {
    CompassConfig saved = nl::config::get<CompassConfig>();
    ~ConfigGuard() { nl::config::mut<CompassConfig>() = saved; }
};

/// Restores the player's permissions, game type and the world's cheat flag when a test ends.
struct PlayerGuard {
    Player&                player;
    CommandPermissionLevel command;
    PlayerPermissionLevel  permission;
    GameType               gameType;
    bool                   cheats;
    explicit PlayerGuard(Player& p)
    : player(p),
      command(p.getAbilities().mPermissions->mCommandPermissions),
      permission(p.getAbilities().mPermissions->mPlayerPermissions),
      gameType(p.getPlayerGameType()),
      cheats(ll::service::getLevel()->getLevelData().mCheatsEnabled) {}
    ~PlayerGuard() {
        setPermissions(player, permission, command);
        player.setPlayerGameType(gameType);
        if (auto level = ll::service::getLevel()) level->getLevelData().setCheatsEnabled(cheats);
    }
    static void setPermissions(Player& p, PlayerPermissionLevel level, CommandPermissionLevel command) {
        p.getAbilities().mPermissions->mPlayerPermissions  = level;
        p.getAbilities().mPermissions->mCommandPermissions = command;
    }
    static void makeOperator(Player& p, bool op) {
        if (op) setPermissions(p, PlayerPermissionLevel::Operator, CommandPermissionLevel::GameDirectors);
        else setPermissions(p, PlayerPermissionLevel::Member, CommandPermissionLevel::Any);
    }
};

int levelOf(Player const& player) {
    auto                     ref = player.getAttribute(Player::LEVEL());
    AttributeInstance const* ptr = ref.mPtr;
    return ptr ? static_cast<int>(ptr->mCurrentValue) : 0;
}

void setLevel(Player& player, int level) { player.addLevels(level - levelOf(player)); }

CompassData foundAt(int x, int z, int damage = 0) {
    CompassData d;
    d.state   = CompassState::Found;
    d.biomeId = "minecraft:plains";
    d.foundX  = x;
    d.foundZ  = z;
    d.prevPos = {
        {x, 64, z}
    };
    d.samples = 1;
    d.damage  = damage;
    d.uid     = compass::newUid();
    return d;
}

/// Puts a compass with `data` into the selected slot; returns its slot.
compass::CompassSlot giveCompass(Context& ctx, Player& player, CompassData const& data) {
    int  selected = player.getSelectedItemSlot();
    auto stack    = compass::makeCompass();
    ctx.require(!stack.isNull(), "compass item exists");
    compass::writeData(stack, data);
    ctx.require(ctx.give(player, stack, selected) == selected, "compass given");
    return compass::CompassSlot{compass::Hand::Main, selected};
}

CompassData dataOf(Player const& player, compass::CompassSlot slot) {
    CompassData broken;
    broken.biomeId = "<no compass>";
    return compass::dataAt(player, slot).value_or(broken);
}

/// Stone from y 55 to 70 at x/z with an air pocket at 66/67 and air at 71/72: NC's probe from 63
/// lands on 66, from 70 on 71 (whatever the terrain there).
void buildPocket(Context& ctx, int x, int z) {
    ctx.setBlock(nl::core::GlobalPos{0, x, 71, z}, "minecraft:air");
    ctx.setBlock(nl::core::GlobalPos{0, x, 72, z}, "minecraft:air");
    for (int y = 55; y <= 70; ++y) {
        bool pocket = y == 66 || y == 67;
        ctx.setBlock(nl::core::GlobalPos{0, x, y, z}, pocket ? "minecraft:air" : "minecraft:stone");
    }
}

} // namespace

NL_REGISTER_SELFTEST("actions.sneak_reset", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard config;
    auto&       player = ctx.player();
    co_await ctx.ticks(1);
    auto slot = giveCompass(ctx, player, CompassData{});
    ctx.require(
        nl::feature::search::startSearch(player, slot, "minecraft:plains", player.getFeetBlockPos()),
        "search started"
    );
    auto searching = dataOf(player, slot);
    ctx.check(searching.state == CompassState::Searching, "the compass is searching");
    ctx.check(actions::onUse(player, true), "sneak-use is handled");
    ctx.check(!nl::feature::search::isSearching(searching.uid), "sneak-use stops the search");
    auto reset = dataOf(player, slot);
    ctx.check(reset.state == CompassState::Inactive && reset.biomeId.empty(), "sneak-use resets to Inactive");
    ctx.check(reset.uid == searching.uid, "the uid is kept");

    // a broken compass resets too, keeping its damage
    nl::config::mut<CompassConfig>().compassDurability = 1;
    giveCompass(ctx, player, foundAt(10, 20, 1));
    ctx.require(compass::isBroken(dataOf(player, slot)), "the compass is broken");
    ctx.check(actions::onUse(player, true), "sneak-use on a broken compass is handled");
    auto brokenReset = dataOf(player, slot);
    ctx.check(brokenReset.state == CompassState::Inactive && brokenReset.prevPos.empty(), "broken compass reset");
    ctx.check(brokenReset.damage == 1, "reset keeps the damage");
    ctx.check(!actions::onUse(player, false), "a broken compass does not open the screen");
    co_await ctx.ticks(2);
});

NL_REGISTER_SELFTEST("actions.use_event", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& player = ctx.player();
    co_await ctx.ticks(1);
    auto slot = giveCompass(ctx, player, foundAt(1, 2));
    ctx.check(actions::handleUseEvent(player, true), "the first event of a click is taken");
    ctx.check(dataOf(player, slot).state == CompassState::Found, "the use applies on the next tick");
    co_await ctx.ticks(2);
    ctx.check(dataOf(player, slot).state == CompassState::Inactive, "a sneaking use resets the compass");

    // events within the same click (held button, block + air event) act once
    compass::storeAt(player, slot, foundAt(3, 4));
    ctx.check(!actions::handleUseEvent(player, true), "a repeated event of the same click is ignored");
    co_await ctx.ticks(3);
    ctx.check(!actions::handleUseEvent(player, true), "held: still the same click");
    co_await ctx.ticks(3);
    ctx.check(dataOf(player, slot).state == CompassState::Found, "repeated events did nothing");
    co_await ctx.ticks(10);
    ctx.check(actions::handleUseEvent(player, true), "a new click is taken again");
    co_await ctx.ticks(2);
    ctx.check(dataOf(player, slot).state == CompassState::Inactive, "and handled");
});

NL_REGISTER_SELFTEST("actions.broken", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard config;
    auto&       player = ctx.player();
    PlayerGuard guard(player);
    co_await ctx.ticks(1);
    player.setPlayerGameType(GameType::Survival);
    nl::config::mut<CompassConfig>().compassDurability = 2;
    nl::config::mut<CompassConfig>().defaultXpLevels   = 1;
    setLevel(player, 5);

    auto slot = giveCompass(ctx, player, foundAt(5, 6, 2));
    ctx.check(compass::isBroken(dataOf(player, slot)), "damage 2 of 2 is broken");
    ctx.check(!actions::onUse(player, false), "use does nothing (PASS)");
    actions::search(player, "minecraft:desert");
    auto afterSearch = dataOf(player, slot);
    ctx.check(afterSearch.state == CompassState::Found && afterSearch.foundX == 5, "search is ignored");
    actions::searchForNext(player);
    ctx.check(dataOf(player, slot).state == CompassState::Found, "search for next is ignored");
    ctx.check(levelOf(player) == 5, "no XP taken");

    nl::config::mut<CompassConfig>().compassDurability = 3;
    ctx.check(!compass::isBroken(dataOf(player, slot)), "damage 2 of 3 is not broken");
    actions::search(player, "minecraft:desert");
    auto d = dataOf(player, slot);
    ctx.check(d.state != CompassState::Found && d.biomeId == "minecraft:desert", "an unbroken compass searches");
    ctx.check(levelOf(player) == 4, "and pays its XP");
    actions::onUse(player, true);
    co_await ctx.ticks(1);
});

NL_REGISTER_SELFTEST("actions.xp", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard config;
    auto&       player = ctx.player();
    PlayerGuard guard(player);
    co_await ctx.ticks(1);
    nl::config::mut<CompassConfig>().defaultXpLevels  = 2;
    nl::config::mut<CompassConfig>().perBiomeXpLevels = {};
    nl::config::mut<CompassConfig>().maxNextSearches  = 25;
    auto slot                                         = giveCompass(ctx, player, CompassData{});
    ctx.require(nl::feature::biomes::xpLevelsFor("minecraft:plains") == 2, "the config XP cost applies");

    player.setPlayerGameType(GameType::Survival);
    setLevel(player, 5);
    ctx.check(actions::canAfford(player, 2) && !actions::canAfford(player, 6), "canAfford compares levels");
    actions::search(player, "minecraft:plains");
    ctx.check(levelOf(player) == 3, "survival: search takes 2 levels (5 -> 3)");
    setLevel(player, 1);
    actions::search(player, "minecraft:plains");
    ctx.check(levelOf(player) == 0, "survival: clamped at level 0, no affordability check");
    ctx.check(dataOf(player, slot).state == CompassState::Searching, "the unaffordable search still starts (NC)");

    compass::storeAt(player, slot, foundAt(7, 8));
    setLevel(player, 4);
    actions::searchForNext(player);
    ctx.check(levelOf(player) == 2, "search for next takes the biome's levels");
    ctx.check(dataOf(player, slot).state == CompassState::Searching, "search for next started");

    nl::config::mut<CompassConfig>().maxNextSearches = 0;
    compass::storeAt(player, slot, foundAt(7, 8));
    actions::searchForNext(player);
    ctx.check(dataOf(player, slot).state == CompassState::Found && levelOf(player) == 2, "maxNextSearches 0: no next");
    nl::config::mut<CompassConfig>().maxNextSearches = 25;
    compass::storeAt(player, slot, CompassData{});
    actions::searchForNext(player);
    ctx.check(dataOf(player, slot).state == CompassState::Inactive && levelOf(player) == 2, "next needs Found");

    player.setPlayerGameType(GameType::Creative);
    co_await ctx.ticks(1);
    ctx.check(actions::hasInfiniteXp(player) && actions::canAfford(player, 3), "creative has infinite XP");
    actions::search(player, "minecraft:plains");
    ctx.check(levelOf(player) == 2, "creative: no XP cost");
    actions::onUse(player, true);
    co_await ctx.ticks(1);
});

NL_REGISTER_SELFTEST("actions.durability_bar", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard config;
    auto&       player = ctx.player();
    co_await ctx.ticks(1);
    auto barOf = [&](compass::CompassSlot s) { return static_cast<int>(compass::stackAt(player, s).getDamageValue()); };

    nl::config::mut<CompassConfig>().compassDurability = 4;
    auto slot                                          = giveCompass(ctx, player, foundAt(1, 1, 1));
    actions::refreshDurability(player);
    ctx.check(barOf(slot) == actions::kBarMaxDamage / 4, "damage 1 of 4 shows a quarter of the bar used");
    ctx.check(dataOf(player, slot).damage == 1 && dataOf(player, slot).state == CompassState::Found, "data kept");

    compass::storeAt(player, slot, foundAt(1, 1, 4));
    co_await ctx.ticks(25); // the periodic sync
    ctx.check(barOf(slot) == actions::kBarMaxDamage - 1, "a broken compass shows an empty bar (periodic sync)");

    // a vanilla repair only changes the bar: the next sync restores it from the data
    ItemStack repaired = compass::stackAt(player, slot);
    repaired.setDamageValue(0);
    player.getInventory().setItem(slot.slot, repaired);
    actions::refreshDurability(player);
    ctx.check(barOf(slot) == actions::kBarMaxDamage - 1 && compass::isBroken(dataOf(player, slot)), "repair undone");

    nl::config::mut<CompassConfig>().compassDurability = 0;
    actions::refreshDurability(player);
    ctx.check(barOf(slot) == 0, "durability 0: no bar");
    compass::storeAt(player, slot, CompassData{});
    nl::config::mut<CompassConfig>().compassDurability = 4;
    actions::refreshDurability(player);
    ctx.check(barOf(slot) == 0, "no damage: no bar");

    // the off hand too
    auto off = compass::makeCompass();
    compass::writeData(off, foundAt(2, 2, 2));
    player.setOffhandSlot(off);
    actions::refreshDurability(player);
    ctx.check(player.getOffhandSlot().getDamageValue() == actions::kBarMaxDamage / 2, "off hand compass bar");
    player.setOffhandSlot(ItemStack::EMPTY_ITEM());
});

NL_REGISTER_SELFTEST("actions.permission", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard config;
    auto&       player = ctx.player();
    PlayerGuard guard(player);
    co_await ctx.ticks(1);
    auto level = ll::service::getLevel();
    ctx.require(level.has_value(), "level");
    auto setCheats = [&](bool on) { level->getLevelData().setCheatsEnabled(on); };

    nl::config::mut<CompassConfig>().allowTeleport = true;
    PlayerGuard::makeOperator(player, true);
    setCheats(false);
    ctx.check(actions::canTeleport(player), "operator, no cheats: allowed");
    PlayerGuard::makeOperator(player, false);
    ctx.check(!actions::canTeleport(player), "member, no cheats: denied");
    player.setPlayerGameType(GameType::Creative);
    ctx.check(!actions::canTeleport(player), "creative alone is not enough");
    player.setPlayerGameType(GameType::Survival);
    setCheats(true);
    ctx.check(actions::canTeleport(player), "member, cheats on: allowed");
    nl::config::mut<CompassConfig>().allowTeleport = false;
    ctx.check(!actions::canTeleport(player), "allowTeleport false: denied (cheats)");
    PlayerGuard::makeOperator(player, true);
    ctx.check(!actions::canTeleport(player), "allowTeleport false: denied (operator)");

    // a denied teleport does nothing
    setCheats(false);
    PlayerGuard::makeOperator(player, false);
    nl::config::mut<CompassConfig>().allowTeleport = true;
    giveCompass(ctx, player, foundAt(player.getFeetBlockPos().x + 40, player.getFeetBlockPos().z));
    auto before = player.getFeetBlockPos();
    actions::teleport(player);
    co_await ctx.ticks(2);
    ctx.check(player.getFeetBlockPos().x == before.x && !actions::teleportPending(player), "denied: not moved");
});

NL_REGISTER_SELFTEST("actions.height", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto pos = ctx.testPos(400);
    ctx.require(co_await ctx.ensureLoaded(pos), "test area loaded");
    buildPocket(ctx, pos.x, pos.z);
    auto& region = ctx.player().getDimensionBlockSource();
    ctx.check(actions::safeTeleportY(region, 63, pos.x, pos.z) == 66, "probe from 63 finds the pocket at 66");
    ctx.check(actions::safeTeleportY(region, 70, pos.x, pos.z) == 71, "probe from 70 lands on top of the stone");
    // replaceable blocks are free
    ctx.setBlock(nl::core::GlobalPos{0, pos.x, 66, pos.z}, "minecraft:snow_layer");
    ctx.check(actions::safeTeleportY(region, 63, pos.x, pos.z) == 66, "a snow layer at the feet is free");
    ctx.setBlock(nl::core::GlobalPos{0, pos.x, 67, pos.z}, "minecraft:stone");
    ctx.check(actions::safeTeleportY(region, 63, pos.x, pos.z) != 66, "a solid head block is not");
});

NL_REGISTER_SELFTEST("actions.teleport", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard config;
    auto        pos = ctx.testPos(450);
    ctx.require(co_await ctx.ensureLoaded(pos), "test area loaded");
    auto&       player = ctx.player();
    PlayerGuard guard(player);
    PlayerGuard::makeOperator(player, true);
    nl::config::mut<CompassConfig>().allowTeleport = true;
    buildPocket(ctx, pos.x, pos.z);

    // only a Found compass teleports
    auto slot = giveCompass(ctx, player, CompassData{});
    auto from = player.getFeetBlockPos();
    actions::teleport(player);
    co_await ctx.ticks(2);
    ctx.check(player.getFeetBlockPos() == from, "Inactive: no teleport");

    compass::storeAt(player, slot, foundAt(pos.x, pos.z));
    Vec2 rotation = player.getRotation();
    actions::teleport(player);
    ctx.check(player.getPosDelta().y == 0.f, "no vertical motion");
    co_await ctx.ticks(2);
    auto feet = player.getFeetPos();
    ctx.check(player.getFeetBlockPos() == BlockPos(pos.x, 66, pos.z), "lands in the pocket at y 66");
    ctx.check(
        feet.x == static_cast<float>(pos.x) && feet.z == static_cast<float>(pos.z),
        "whole-block x/z, not centred"
    );
    auto rot = player.getRotation();
    ctx.check(std::abs(rot.x - rotation.x) < 0.5f && std::abs(rot.y - rotation.y) < 0.5f, "keeps yaw and pitch");
});

NL_REGISTER_SELFTEST("actions.teleport_unloaded", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard config;
    auto        pos = ctx.testPos(480);
    ctx.require(co_await ctx.ensureLoaded(pos), "test area loaded");
    auto&       player = ctx.player();
    PlayerGuard guard(player);
    PlayerGuard::makeOperator(player, true);
    nl::config::mut<CompassConfig>().allowTeleport = true;

    int x = pos.x + 6000, z = pos.z + 6000; // far away: not loaded
    giveCompass(ctx, player, foundAt(x, z));
    actions::teleport(player);
    for (int i = 0; i < 25 * 20 && actions::teleportPending(player); ++i) co_await ctx.ticks(1);
    ctx.require(!actions::teleportPending(player), "the teleport finished");
    co_await ctx.ticks(2);
    auto feet = player.getFeetBlockPos();
    ctx.check(feet.x == x && feet.z == z, "in the target column");
    auto& region = player.getDimensionBlockSource();
    int   y      = actions::safeTeleportY(region, 63, x, z);
    ctx.check(feet.y == y, "at NC's safe height (" + std::to_string(feet.y) + " vs " + std::to_string(y) + ")");
    ctx.info("landed at " + std::to_string(feet.x) + " " + std::to_string(feet.y) + " " + std::to_string(feet.z));
});
