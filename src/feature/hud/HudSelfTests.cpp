// Selftests of the HUD unit (U6): the line model on real compasses, the fallback string and its
// change-only sending. Unit U6 owns selftest dx 600..699 (these tests place no blocks).

#include "mc/server/SimulatedPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"

#include "core/HudLines.h"
#include "feature/compass/Biomes.h"
#include "feature/compass/CompassConfig.h"
#include "feature/compass/CompassData.h"
#include "feature/compass/Hud.h"
#include "feature/hud/HudModel.h"
#include "game/Items.h"
#include "mod/Config.h"
#include "selftest/SelfTest.h"

namespace {

using nl::selftest::Context;
namespace compass = nl::feature::compass;
namespace hud     = nl::feature::hud;
namespace core    = nl::core::hud;

/// Puts a compass with `data` into the test player's selected slot. Returns the slot.
compass::CompassSlot holdCompass(Context& ctx, Player& p, compass::CompassData const& data) {
    auto stack = compass::makeCompass();
    ctx.require(!stack.isNull(), "compass item exists");
    compass::writeData(stack, data);
    int slot = ctx.give(p, stack, p.getSelectedItemSlot());
    ctx.require(slot >= 0, "compass given");
    auto held = compass::heldCompass(p);
    ctx.require(held.has_value(), "the player holds the compass");
    return *held;
}

compass::CompassData searching() {
    compass::CompassData d;
    d.startSearch("minecraft:plains");
    d.searchRadius = 2500;
    return d;
}

/// Restores the client config section after a test changed it.
struct ClientConfigGuard {
    compass::CompassClientConfig saved = nl::config::get<compass::CompassClientConfig>();
    ~ClientConfigGuard() { nl::config::mut<compass::CompassClientConfig>() = saved; }
};

} // namespace

NL_REGISTER_SELFTEST("hud.lines_searching", [](Context& ctx) {
    auto& p = ctx.player();
    holdCompass(ctx, p, searching());
    auto lines = hud::linesFor(p, "en_US");
    ctx.require(lines.size() == 6, "Searching has 6 lines, got " + std::to_string(lines.size()));
    ctx.check(lines[0] == core::Line{0, "Status", false}, "line 0 Status (label)");
    ctx.check(lines[1] == core::Line{1, "Searching", true}, "line 1 Searching (value)");
    ctx.check(lines[2] == core::Line{3, "Biome", false}, "line 3 Biome");
    ctx.check(lines[3].index == 4 && lines[3].value && !lines[3].text.empty(), "line 4 biome name");
    ctx.check(lines[3].text == nl::feature::biomes::biomeDisplayName("minecraft:plains", "en_US"), "raw biome name");
    ctx.check(lines[4] == core::Line{6, "Radius", false}, "line 6 Radius");
    ctx.check(lines[5] == core::Line{7, "2500", true}, "line 7 radius value");
});

NL_REGISTER_SELFTEST("hud.lines_found", [](Context& ctx) {
    auto&                p   = ctx.player();
    auto                 pos = p.getFeetBlockPos();
    compass::CompassData d;
    d.succeed(
        "minecraft:plains",
        pos.x + 30,
        pos.z + 40,
        {
            {pos.x + 30, 64, pos.z + 40}
    },
        77,
        true
    );
    holdCompass(ctx, p, d);
    auto lines = hud::linesFor(p, "en_US");
    ctx.require(lines.size() == 8, "Found with coordinates has 8 lines, got " + std::to_string(lines.size()));
    ctx.check(lines[1] == core::Line{1, "Found", true}, "line 1 Found");
    ctx.check(lines[4] == core::Line{6, "Coordinates", false}, "line 6 Coordinates");
    ctx.check(
        lines[5] == core::Line{7, std::to_string(pos.x + 30) + ", " + std::to_string(pos.z + 40), true},
        "line 7 '<x>, <z>'"
    );
    ctx.check(lines[6] == core::Line{9, "Distance", false}, "line 9 Distance");
    ctx.check(lines[7] == core::Line{10, "50", true}, "line 10 live distance 50, got " + lines[7].text);
    d.succeed(
        "minecraft:plains",
        1,
        2,
        {
            {1, 64, 2}
    },
        77,
        false
    );
    holdCompass(ctx, p, d);
    ctx.check(hud::linesFor(p, "en_US").size() == 4, "no coordinates when display_coords is off");
});

NL_REGISTER_SELFTEST("hud.lines_not_found", [](Context& ctx) {
    auto&                p = ctx.player();
    compass::CompassData d;
    d.fail("minecraft:plains", 10123, 50000);
    holdCompass(ctx, p, d);
    auto lines = hud::linesFor(p, "en_US");
    ctx.require(lines.size() == 8, "Not Found has 8 lines");
    ctx.check(lines[1] == core::Line{1, "Not Found", true}, "line 1 Not Found");
    ctx.check(lines[5] == core::Line{7, "10000", true}, "line 7 floored radius");
    ctx.check(lines[6] == core::Line{9, "Samples", false}, "line 9 Samples");
    ctx.check(lines[7] == core::Line{10, "50000", true}, "line 10 samples");
});

NL_REGISTER_SELFTEST("hud.lines_inactive_and_hands", [](Context& ctx) {
    auto& p = ctx.player();
    holdCompass(ctx, p, compass::CompassData{});
    ctx.check(hud::linesFor(p, "en_US").empty(), "Inactive shows nothing");
    // Main hand first, then off hand.
    auto off = compass::makeCompass();
    compass::writeData(off, searching());
    p.setOffhandSlot(off);
    ctx.check(hud::linesFor(p, "en_US").empty(), "the main-hand compass wins over the off hand");
    ctx.give(p, nl::game::makeItem("minecraft:stick"), p.getSelectedItemSlot());
    auto lines = hud::linesFor(p, "en_US");
    ctx.check(lines.size() == 6 && lines[1].text == "Searching", "the off-hand compass is used without one in hand");
    p.setOffhandSlot(ItemStack::EMPTY_ITEM());
    ctx.check(hud::linesFor(p, "en_US").empty(), "no compass: nothing");
});

NL_REGISTER_SELFTEST("hud.lines_translated", [](Context& ctx) {
    auto& p = ctx.player();
    holdCompass(ctx, p, searching());
    auto lines = hud::linesFor(p, "zh_CN");
    ctx.require(lines.size() == 6, "zh_CN lines");
    ctx.check(lines[0].text == "状态", "zh_CN Status, got " + lines[0].text);
    ctx.check(lines[1].text == "搜索中", "zh_CN Searching, got " + lines[1].text);
});

NL_REGISTER_SELFTEST("hud.fallback_encoding", [](Context& ctx) {
    ClientConfigGuard guard;
    auto&             p = ctx.player();
    holdCompass(ctx, p, searching());
    auto  lines = hud::linesFor(p, "en_US");
    auto& cfg   = nl::config::mut<compass::CompassClientConfig>();
    for (auto [side, offset] : {
             std::pair{"LEFT",  1 },
             std::pair{"right", 0 },
             std::pair{"RIGHT", 50},
             std::pair{"x",     99}
    }) {
        cfg.overlaySide       = side;
        cfg.overlayLineOffset = offset;
        auto text             = hud::fallbackText(lines);
        auto decoded          = core::decodeFallback(text);
        ctx.require(decoded.has_value(), "fallback text decodes");
        bool right = std::string_view(side) == "right" || std::string_view(side) == "RIGHT";
        ctx.check(decoded->side == (right ? core::Side::Right : core::Side::Left), std::string("side for ") + side);
        ctx.check(decoded->offset == compass::overlayLineOffset(), "offset " + std::to_string(offset) + " (clamped)");
        ctx.check(decoded->lines == lines, "lines survive the round trip");
    }
    ctx.check(compass::overlayLineOffset() == 50, "offset clamped to 50");
});

NL_REGISTER_SELFTEST("hud.fallback_sending", [](Context& ctx) -> ll::coro::CoroTask<> {
    ClientConfigGuard guard;
    Player&           p = ctx.spawnPlayer("nl_hud_send");
    // Inactive (no compass at all, then an Inactive compass): nothing is ever sent.
    hud::refresh(p);
    co_await ctx.ticks(4);
    ctx.check(hud::detail::sendCount(p) == 0, "nothing sent without a compass");
    holdCompass(ctx, p, compass::CompassData{});
    hud::refresh(p);
    co_await ctx.ticks(4);
    ctx.check(hud::detail::sendCount(p) == 0, "nothing sent for an Inactive compass");
    ctx.check(!hud::detail::lastSent(p).has_value(), "no last string for Inactive");
    // Searching: sent once, not again while unchanged.
    auto slot = holdCompass(ctx, p, searching());
    hud::refresh(p);
    ctx.check(hud::detail::sendCount(p) == 1, "Searching sent at once by refresh");
    auto sent = hud::detail::lastSent(p);
    ctx.require(sent.has_value() && core::isFallback(*sent), "a marked string was sent");
    co_await ctx.ticks(6);
    ctx.check(hud::detail::sendCount(p) == 1, "unchanged content is not sent again");
    // A change is sent by the tick update without refresh.
    auto d         = searching();
    d.searchRadius = 3000;
    compass::storeAt(p, slot, d);
    co_await ctx.ticks(4);
    ctx.check(hud::detail::sendCount(p) == 2, "a radius change is sent");
    ctx.check(hud::detail::lastSent(p)->find("3000") != std::string::npos, "the new radius is in the string");
    // A side change (server naturescompass_client) is sent too.
    nl::config::mut<compass::CompassClientConfig>().overlaySide = "RIGHT";
    co_await ctx.ticks(4);
    ctx.check(hud::detail::sendCount(p) == 3, "a side change is sent");
    ctx.check(hud::detail::lastSent(p)->starts_with(core::kMarkerRight), "right marker");
    // Back to Inactive: one clearing marker, then nothing more.
    compass::storeAt(p, slot, compass::CompassData{});
    hud::refresh(p);
    ctx.check(hud::detail::sendCount(p) == 4, "the block is cleared once");
    auto cleared = core::decodeFallback(*hud::detail::lastSent(p));
    ctx.check(cleared.has_value() && cleared->lines.empty(), "the clearing string is a bare marker");
    co_await ctx.ticks(6);
    ctx.check(hud::detail::sendCount(p) == 4, "nothing more is sent while Inactive");
});
