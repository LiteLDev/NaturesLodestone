// Selftests of the search unit (U3). Unit U3 owns selftest dx 300..399 (these tests place no
// blocks). The e2e world is FLAT (one biome in the overworld): overworld tests that need real
// biomes skip themselves there and run in the `search` e2e scenario (a DEFAULT world). The
// Nether and the End generate normally in both.

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "ll/api/service/Bedrock.h"

#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/biome/Biome.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/dimension/DimensionType.h"

#include "core/Spiral.h"
#include "feature/compass/Actions.h"
#include "feature/compass/CompassConfig.h"
#include "feature/compass/CompassData.h"
#include "feature/compass/Search.h"
#include "feature/search/SearchWorker.h"
#include "mod/Config.h"
#include "selftest/SelfTest.h"

namespace {

using nl::selftest::Context;
namespace compass = nl::feature::compass;
namespace search  = nl::feature::search;
using compass::CompassData;
using compass::CompassState;

constexpr int kOverworld = 0;
constexpr int kNether    = 1;
constexpr int kEnd       = 2;

/// Restores the general config section when the test ends (also on abort).
struct ConfigGuard {
    compass::CompassConfig saved = nl::config::get<compass::CompassConfig>();
    ~ConfigGuard() { nl::config::mut<compass::CompassConfig>() = saved; }
    compass::CompassConfig& operator*() { return nl::config::mut<compass::CompassConfig>(); }
    compass::CompassConfig* operator->() { return &nl::config::mut<compass::CompassConfig>(); }
};

/// Stops the searches of the remembered compasses when the test ends (also on abort), so a long
/// search never blocks the next tests.
struct StopGuard {
    std::vector<std::int64_t> uids;
    ~StopGuard() {
        for (auto uid : uids) search::stopSearch(uid);
    }
};

/// Puts a fresh compass into the selected hotbar slot and returns that slot.
compass::CompassSlot giveHeldCompass(Context& ctx, Player& player) {
    int selected = player.getSelectedItemSlot();
    ctx.require(ctx.give(player, compass::makeCompass(), selected) == selected, "compass put into the selected slot");
    auto slot = compass::heldCompass(player);
    ctx.require(slot.has_value(), "the compass is held");
    return *slot;
}

std::int64_t uidAt(Player& player, compass::CompassSlot slot) {
    auto d = compass::dataAt(player, slot);
    return d ? d->uid : 0;
}

/// Waits until the compass with `uid` has no running search; returns its data.
ll::coro::CoroTask<CompassData> waitDone(Context& ctx, Player& player, std::int64_t uid, int maxTicks = 1200) {
    for (int i = 0; i < maxTicks && search::isSearching(uid); i++) co_await ctx.ticks(1);
    ctx.require(!search::isSearching(uid), "the search finished in time");
    auto slot = compass::findCompass(player, uid);
    ctx.require(slot.has_value(), "the compass is still in the inventory");
    co_return compass::dataAt(player, *slot).value_or(CompassData{});
}

/// What a search that never matches ends with (the core spiral run to the end).
nl::core::SpiralSearch expectedFailure(int startY, int dimId) {
    nl::core::SpiralSearch::Params p;
    p.startY      = startY;
    p.sampleSpace = compass::sampleSpace();
    p.maxRadius   = compass::maxSearchRadius();
    p.maxSamples  = compass::maxSamples();
    p.yLevels     = search::yLevelsFor(dimId, startY);
    nl::core::SpiralSearch s(p);
    while (s.hasWork()) s.doWork([](int, int, int) { return false; });
    return s;
}

std::shared_ptr<Dimension> ensureDimension(int dimId) {
    auto level = ll::service::getLevel();
    if (!level) return {};
    return level->getOrCreateDimension(DimensionType(dimId)).lock();
}

/// The overworld has a single biome (FLAT world): sample a wide grid.
bool singleBiomeOverworld(BlockPos const& at) {
    std::set<std::string> seen;
    for (int i = -2; i <= 2; i++) {
        for (int j = -2; j <= 2; j++)
            seen.insert(search::sampleBiomeId(kOverworld, at.x + i * 1600, at.y, at.z + j * 1600));
    }
    return seen.size() <= 1;
}

double horizontalDistance(int ax, int az, int bx, int bz) {
    double dx = ax - bx, dz = az - bz;
    return std::sqrt(dx * dx + dz * dz);
}

/// Checks a Found result for `biome` searched from `start` in `dimId`.
void checkFound(Context& ctx, CompassData const& d, std::string const& biome, int dimId, std::string const& what) {
    ctx.require(d.state == CompassState::Found, what + ": Found (state " + std::to_string(int(d.state)) + ")");
    ctx.check(d.biomeId == biome, what + ": biome id kept");
    ctx.require(!d.prevPos.empty(), what + ": prev_pos holds the position");
    auto const& last = d.prevPos.back();
    ctx.check(last.x == d.foundX && last.z == d.foundZ, what + ": prev_pos ends with the found x/z");
    ctx.check(
        search::sampleBiomeId(dimId, last.x, last.y, last.z) == biome,
        what + ": the biome source has " + biome + " at the found position (got "
            + search::sampleBiomeId(dimId, last.x, last.y, last.z) + ")"
    );
    ctx.check(d.searchRadius == 0, what + ": no radius on success");
    ctx.check(d.displayCoords == compass::displayCoordinates(), what + ": display_coords from the config");
    ctx.check(
        horizontalDistance(d.foundX, d.foundZ, 0, 0) >= 0 && d.samples <= compass::maxSamples(),
        what + ": samples within maxSamples"
    );
}

} // namespace

NL_REGISTER_SELFTEST("search.sample_overworld", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& player = ctx.player();
    co_await ctx.ticks(1);
    auto pos = player.getFeetBlockPos();
    auto id  = search::sampleBiomeId(kOverworld, pos.x, pos.y, pos.z);
    ctx.require(!id.empty(), "the overworld generator gives a biome");
    ctx.check(id.starts_with("minecraft:"), "ids are namespaced: " + id);
    // The generator agrees with the loaded chunk (the chunk was generated from the same source).
    auto& region = player.getDimensionBlockSource();
    auto  loaded = region.getBiome(pos).mHash->getString();
    ctx.check(id == loaded || id == "minecraft:" + loaded, "generator biome " + id + " = loaded chunk biome " + loaded);
    auto ys = search::yLevelsFor(kOverworld, pos.y);
    ctx.require(!ys.empty() && ys.front() == pos.y, "the first y level is the start y");
    auto& dim = player.getDimension();
    for (int y : ys)
        ctx.check(y > dim.mHeightRange->mMin && y < dim.mHeightRange->mMax, "y levels within the build height");
    ctx.info("overworld biome at spawn: " + id + ", " + std::to_string(ys.size()) + " y levels");
});

// The generator's biome away from the origin agrees with the generated chunk there (a scale or
// coordinate mix-up in the sampling only shows far from 0, 0).
NL_REGISTER_SELFTEST("search.sample_matches_chunks", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& player = ctx.player();
    co_await ctx.ticks(1);
    auto const origin = player.getFeetBlockPos();
    int        agree = 0, total = 0;
    for (auto [dx, dz] : {std::pair{420, 180}, std::pair{-260, 520}, std::pair{610, -330}}) {
        BlockPos pos{origin.x + dx, 0, origin.z + dz};
        if (!co_await ctx.ensureLoaded(nl::core::GlobalPos{0, pos.x, origin.y, pos.z})) {
            ctx.info("chunk at " + std::to_string(pos.x) + ", " + std::to_string(pos.z) + " did not load");
            continue;
        }
        auto& region = player.getDimensionBlockSource();
        pos.y        = region.getAboveTopSolidBlock(pos.x, pos.z, false, false);
        auto id      = search::sampleBiomeId(kOverworld, pos.x, pos.y, pos.z);
        auto raw     = region.getBiome(pos).mHash->getString();
        auto loaded  = raw.starts_with("minecraft:") ? raw : "minecraft:" + raw;
        ++total;
        if (id == loaded) ++agree;
        ctx.info("at " + std::to_string(pos.x) + " " + std::to_string(pos.y) + " " + std::to_string(pos.z)
                 + ": generator " + id + ", chunk " + loaded);
    }
    ctx.require(total > 0, "a far chunk loaded");
    ctx.check(agree == total, std::to_string(agree) + "/" + std::to_string(total) + " far samples match their chunks");
});

NL_REGISTER_SELFTEST("search.found_at_start", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& player = ctx.player();
    co_await ctx.ticks(1);
    auto slot  = giveHeldCompass(ctx, player);
    auto start = player.getFeetBlockPos();
    auto biome = search::sampleBiomeId(static_cast<int>(player.getDimensionId()), start.x, start.y, start.z);
    ctx.require(!biome.empty(), "biome at the player");

    ctx.require(search::startSearch(player, slot, biome, start), "startSearch accepts a held compass");
    auto uid = uidAt(player, slot);
    ctx.require(uid != 0, "the compass got a uid");
    ctx.check(search::isSearching(uid), "the search runs");
    auto now = compass::dataAt(player, slot);
    ctx.check(now && now->state == CompassState::Searching && now->biomeId == biome, "Searching right away");

    auto d = co_await waitDone(ctx, player, uid, 100);
    checkFound(ctx, d, biome, kOverworld, "start");
    ctx.check(d.foundX == start.x && d.foundZ == start.z, "found at the start x/z");
    ctx.check(d.samples == 0, "no sample counted for a match at the start");
    ctx.check(d.prevPos.size() == 1 && d.prevPos[0].y == start.y, "prev_pos has the start with the start y");
    int expectedDamage = nl::feature::actions::hasInfiniteXp(player) ? 0 : 1;
    ctx.check(d.damage == expectedDamage, "damage + 1 unless creative (" + std::to_string(d.damage) + ")");
    ctx.check(d.uid == uid, "the uid is kept");
    ctx.check(
        !search::startSearch(player, compass::CompassSlot{compass::Hand::Off, -1}, biome, start),
        "no compass: false"
    );
});

NL_REGISTER_SELFTEST("search.not_found", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard cfg;
    cfg->maxSamples = 10;
    auto& player    = ctx.player();
    co_await ctx.ticks(1);
    auto slot  = giveHeldCompass(ctx, player);
    auto start = player.getFeetBlockPos();
    ctx.require(search::startSearch(player, slot, "minecraft:nl_no_such_biome", start), "search started");
    auto d   = co_await waitDone(ctx, player, uidAt(player, slot), 100);
    auto exp = expectedFailure(start.y, kOverworld);
    ctx.check(d.state == CompassState::NotFound, "Not Found");
    ctx.check(d.biomeId == "minecraft:nl_no_such_biome", "biome id kept");
    ctx.check(d.samples == 11, "maxSamples + 1 samples (" + std::to_string(d.samples) + ")");
    ctx.check(d.samples == exp.samples(), "samples as the core spiral");
    ctx.check(d.searchRadius == compass::floorRadius(exp.radius()), "radius floored to 500");
    ctx.check(d.damage == 0 && d.prevPos.empty(), "no damage, no prev_pos on failure");

    // Wide sample spacing: the final radius passes 500 and is floored.
    cfg->sampleSpaceModifier = 200; // 800 blocks
    cfg->maxSamples          = 3;
    ctx.require(search::startSearch(player, slot, "minecraft:nl_no_such_biome", start), "search started (wide)");
    d   = co_await waitDone(ctx, player, uidAt(player, slot), 100);
    exp = expectedFailure(start.y, kOverworld);
    ctx.check(d.state == CompassState::NotFound && d.samples == 4, "wide: Not Found after 4 samples");
    ctx.check(
        exp.radius() > 500 && d.searchRadius == compass::floorRadius(exp.radius()),
        "wide: radius floored (" + std::to_string(d.searchRadius) + ")"
    );
    ctx.check(d.searchRadius == 1500, "wide: (1600, -800) -> radius 1788 -> 1500");

    // A radius limit below the second sample: two samples, then failure.
    cfg->sampleSpaceModifier = 16;
    cfg->maxSamples          = 50000;
    cfg->radiusModifier      = 10; // 40 blocks
    ctx.require(search::startSearch(player, slot, "minecraft:nl_no_such_biome", start), "search started (radius)");
    d = co_await waitDone(ctx, player, uidAt(player, slot), 100);
    ctx.check(
        d.state == CompassState::NotFound && d.samples == 2,
        "radius limit: 2 samples (" + std::to_string(d.samples) + ")"
    );
});

NL_REGISTER_SELFTEST("search.zero_limits_fail_at_once", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard cfg;
    auto&       player = ctx.player();
    co_await ctx.ticks(1);
    auto slot  = giveHeldCompass(ctx, player);
    auto start = player.getFeetBlockPos();
    for (int which = 0; which < 2; which++) {
        *cfg = nl::config::get<compass::CompassConfig>();
        if (which == 0) cfg->radiusModifier = 0;
        else cfg->sampleSpaceModifier = 0;
        ctx.require(search::startSearch(player, slot, "minecraft:plains", start), "search started");
        auto uid = uidAt(player, slot);
        ctx.check(!search::isSearching(uid), "no worker when a limit is 0");
        auto d = compass::dataAt(player, slot);
        ctx.check(d && d->state == CompassState::NotFound, "Not Found at once");
        ctx.check(d && d->samples == 0 && d->searchRadius == 0, "0 samples, radius 0");
        cfg->radiusModifier      = 2500;
        cfg->sampleSpaceModifier = 16;
    }
});

NL_REGISTER_SELFTEST("search.stop", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard cfg;
    cfg->maxSamples     = 1000000;
    cfg->radiusModifier = 1000000;
    auto& player        = ctx.player();
    co_await ctx.ticks(1);
    auto slot  = giveHeldCompass(ctx, player);
    auto start = player.getFeetBlockPos();
    ctx.require(search::startSearch(player, slot, "minecraft:nl_no_such_biome", start), "search started");
    auto      uid = uidAt(player, slot);
    StopGuard guard{{uid}};
    co_await ctx.ticks(1);
    // A fixed biome source (flat world) can sample a million columns within a couple of ticks:
    // either way the radius on the compass is a multiple of 500 by now (progress or failure).
    auto progress = compass::dataAt(player, slot);
    ctx.check(
        progress && progress->searchRadius > 0 && progress->searchRadius % 500 == 0,
        "the radius progress is written (" + std::to_string(progress ? progress->searchRadius : -1) + ")"
    );
    if (search::isSearching(uid)) {
        ctx.check(progress && progress->state == CompassState::Searching, "Searching while it runs");
        ctx.check(progress && progress->samples == 0, "samples are written only at the end");
    }

    // A new search of the same compass replaces the worker.
    ctx.require(search::startSearch(player, slot, "minecraft:nl_other_missing_biome", start), "second search");
    ctx.check(uidAt(player, slot) == uid, "same uid");
    auto uids = search::runningUids();
    ctx.check(std::count(uids.begin(), uids.end(), uid) == 1, "one worker per compass");

    search::stopSearch(uid);
    ctx.check(!search::isSearching(uid), "stopped");
    auto atStop = compass::dataAt(player, slot);
    co_await ctx.ticks(3);
    auto d = compass::dataAt(player, slot);
    ctx.check(d && d->state == CompassState::Searching, "stopping writes nothing (still Searching)");
    ctx.check(d && d->biomeId == "minecraft:nl_other_missing_biome", "the data of the second search");
    ctx.check(d && atStop && *d == *atStop, "nothing written after the stop");
});

NL_REGISTER_SELFTEST("search.compass_gone", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard cfg;
    cfg->maxSamples = 50;
    auto& player    = ctx.player();
    co_await ctx.ticks(1);
    auto slot  = giveHeldCompass(ctx, player);
    auto start = player.getFeetBlockPos();
    ctx.require(search::startSearch(player, slot, "minecraft:nl_no_such_biome", start), "search started");
    auto      uid   = uidAt(player, slot);
    ItemStack taken = compass::stackAt(player, slot);
    // The compass leaves the inventory before the worker ends: it finishes without writing.
    player.getInventory().setItem(slot.slot, ItemStack::EMPTY_ITEM());
    ctx.check(!compass::findCompass(player, uid), "the compass left the inventory");
    for (int i = 0; i < 100 && search::isSearching(uid); i++) co_await ctx.ticks(1);
    ctx.check(!search::isSearching(uid), "the worker ended");
    ctx.check(compass::readData(taken).state == CompassState::Searching, "a removed compass stays Searching");
});

NL_REGISTER_SELFTEST("search.fifo", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard cfg;
    cfg->maxSamples = 20000;
    auto& player    = ctx.player();
    co_await ctx.ticks(1);
    int  selected = player.getSelectedItemSlot();
    auto slotA    = giveHeldCompass(ctx, player);
    auto start    = player.getFeetBlockPos();
    ctx.require(search::startSearch(player, slotA, "minecraft:nl_no_such_biome", start), "search A");
    auto      uidA = uidAt(player, slotA);
    StopGuard guard{{uidA}};
    // Move A away from the hand; B is the held one.
    ItemStack a     = compass::stackAt(player, slotA);
    int       aSlot = (selected + 1) % 9;
    player.getInventory().setItem(aSlot, a);
    player.getInventory().setItem(selected, ItemStack::EMPTY_ITEM());
    auto slotB = giveHeldCompass(ctx, player);
    auto biome = search::sampleBiomeId(kOverworld, start.x, start.y, start.z);
    ctx.require(search::startSearch(player, slotB, biome, start), "search B");
    auto uidB = uidAt(player, slotB);
    ctx.require(uidA != uidB && uidA != 0 && uidB != 0, "two compasses, two uids");
    auto uids = search::runningUids();
    ctx.require(uids.size() >= 2 && uids[uids.size() - 2] == uidA && uids.back() == uidB, "workers in start order");
    // B can only run once A is done: the first worker gets the whole budget.
    for (int i = 0; i < 1200 && search::isSearching(uidB); i++) {
        if (search::isSearching(uidA)) ctx.check(search::isSearching(uidB), "B waits for A");
        co_await ctx.ticks(1);
    }
    ctx.check(!search::isSearching(uidA) && !search::isSearching(uidB), "both finished");
    auto dA = compass::findCompass(player, uidA);
    auto dB = compass::findCompass(player, uidB);
    ctx.require(dA && dB, "both compasses found by uid");
    ctx.check(compass::dataAt(player, *dA)->state == CompassState::NotFound, "A Not Found");
    ctx.check(compass::dataAt(player, *dB)->state == CompassState::Found, "B Found");
    ctx.check(compass::dataAt(player, *dA)->samples == 20001, "A sampled maxSamples + 1 columns");
});

NL_REGISTER_SELFTEST("search.nether", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto nether = ensureDimension(kNether);
    ctx.require(nether != nullptr, "the Nether exists");
    auto& player = ctx.player();
    co_await ctx.ticks(1);
    static std::set<std::string> const netherBiomes{
        "minecraft:hell", // Bedrock's id of the Nether Wastes
        "minecraft:crimson_forest",
        "minecraft:warped_forest",
        "minecraft:soulsand_valley",
        "minecraft:basalt_deltas",
    };
    std::set<std::string> seen;
    for (int i = 0; i < 16; i++) {
        auto id = search::sampleBiomeId(kNether, i * 300, 64, -i * 200);
        ctx.check(netherBiomes.contains(id), "a Nether biome: " + id);
        seen.insert(id);
    }
    ctx.check(seen.size() > 1, "the Nether has several biomes");
    ctx.check(search::yLevelsFor(kNether, 64) == std::vector<int>{64}, "Nether y levels from 64: {64}");
    ctx.check(
        search::yLevelsFor(kNether, 200) == std::vector<int>{127, 63},
        "a start above the Nether roof is clamped to the top level"
    );

    auto              slot = giveHeldCompass(ctx, player);
    BlockPos          start{0, 64, 0};
    std::string const target = "minecraft:crimson_forest";
    ctx.require(search::startWorker(player, slot, target, start, kNether, {}), "Nether search started");
    auto uid = uidAt(player, slot);
    auto d   = co_await waitDone(ctx, player, uid);
    checkFound(ctx, d, target, kNether, "Nether");
    ctx.info(
        "crimson_forest at " + std::to_string(d.foundX) + ", " + std::to_string(d.foundZ) + " after "
        + std::to_string(d.samples) + " samples"
    );

    // Search for Next: a different instance, not connected to the first.
    auto first = d.prevPos;
    ctx.require(search::startWorker(player, slot, target, start, kNether, first), "Search for Next started");
    auto now = compass::dataAt(player, slot);
    ctx.check(
        now && now->state == CompassState::Searching && now->prevPos.empty(),
        "next: Searching, prev_pos cleared on the item"
    );
    auto n = co_await waitDone(ctx, player, uid);
    checkFound(ctx, n, target, kNether, "Nether next");
    ctx.require(n.prevPos.size() == 2, "next: prev_pos has both positions");
    ctx.check(n.prevPos[0] == first[0], "next: the first position is kept");
    ctx.check(
        horizontalDistance(n.foundX, n.foundZ, first[0].x, first[0].z) > 2.0 * compass::sampleSpace(),
        "next: more than 2 sample spaces from the first"
    );
    ctx.info("next crimson_forest at " + std::to_string(n.foundX) + ", " + std::to_string(n.foundZ));
});

NL_REGISTER_SELFTEST("search.end", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard cfg;
    auto        end = ensureDimension(kEnd);
    ctx.require(end != nullptr, "the End exists");
    auto& player = ctx.player();
    co_await ctx.ticks(1);
    auto id = search::sampleBiomeId(kEnd, 0, 64, 0);
    ctx.check(id == "minecraft:the_end", "the End biome: " + id);

    auto     slot = giveHeldCompass(ctx, player);
    BlockPos start{100, 64, 100};
    ctx.require(search::startWorker(player, slot, "minecraft:the_end", start, kEnd, {}), "End search started");
    auto d = co_await waitDone(ctx, player, uidAt(player, slot), 100);
    checkFound(ctx, d, "minecraft:the_end", kEnd, "End");
    ctx.check(d.foundX == 100 && d.foundZ == 100 && d.samples == 0, "found at the start");

    cfg->maxSamples = 20;
    ctx.require(search::startWorker(player, slot, "minecraft:plains", start, kEnd, {}), "plains in the End");
    d = co_await waitDone(ctx, player, uidAt(player, slot), 100);
    ctx.check(d.state == CompassState::NotFound && d.samples == 21, "no plains in the End");
});

NL_REGISTER_SELFTEST("search.overworld_biomes", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& player = ctx.player();
    co_await ctx.ticks(1);
    auto start = player.getFeetBlockPos();
    if (singleBiomeOverworld(start)) {
        ctx.info("single-biome overworld (flat world): real overworld searches run in the search e2e scenario");
        co_return;
    }
    auto slot = giveHeldCompass(ctx, player);
    for (std::string target : {"minecraft:plains", "minecraft:desert", "minecraft:ocean", "minecraft:forest"}) {
        ctx.require(search::startSearch(player, slot, target, start), "search " + target);
        auto d = co_await waitDone(ctx, player, uidAt(player, slot), 6000);
        checkFound(ctx, d, target, kOverworld, target);
        ctx.check(
            horizontalDistance(d.foundX, d.foundZ, start.x, start.z)
                <= compass::maxSearchRadius() + compass::sampleSpace(),
            target + ": within the search radius"
        );
        ctx.info(
            target + " at " + std::to_string(d.foundX) + ", " + std::to_string(d.foundZ) + " after "
            + std::to_string(d.samples) + " samples"
        );
    }
    // Search for Next of the last Found biome from the player's position.
    auto before = compass::dataAt(player, slot);
    ctx.require(before && before->state == CompassState::Found, "Found before Search for Next");
    ctx.require(search::startSearchForNext(player, slot), "Search for Next");
    auto n = co_await waitDone(ctx, player, uidAt(player, slot), 6000);
    checkFound(ctx, n, before->biomeId, kOverworld, "next");
    ctx.require(n.prevPos.size() == 2, "next: two positions");
    ctx.check(
        horizontalDistance(n.foundX, n.foundZ, before->foundX, before->foundZ) > 2.0 * compass::sampleSpace(),
        "next: a different instance"
    );
    ctx.info("next " + before->biomeId + " at " + std::to_string(n.foundX) + ", " + std::to_string(n.foundZ));
    // Search for Next needs a Found compass.
    auto plain = compass::makeCompass();
    int  other = (player.getSelectedItemSlot() + 2) % 9;
    ctx.give(player, plain, other);
    ctx.check(
        !search::startSearchForNext(player, compass::CompassSlot{compass::Hand::Main, other}),
        "next needs Found"
    );
});
