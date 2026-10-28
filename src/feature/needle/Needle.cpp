// Needle unit (U7, docs/SPEC.md 3.8): the server-side debug command and the selftests. The needle
// itself is drawn by the client build (src/client/needle/).
//
//   /naturescompass needle found <x> <z>   (op) marks the held compass Found at x/z (test tooling)
//
// Selftests: dx 700..799 (these tests place no blocks).

#include <cmath>
#include <set>

#include "ll/api/command/CommandHandle.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"

#include "core/CompassAngle.h"
#include "feature/compass/CompassData.h"
#include "feature/needle/NeedleTarget.h"
#include "game/Items.h"
#include "mod/Commands.h"
#include "mod/Log.h"
#include "selftest/SelfTest.h"

#ifdef NL_CLIENT
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemIconInfoType.h"
#include "mc/world/item/ResolvedItemIconInfo.h"

#include "client/Client.h"
#include "client/needle/Needle.h"
#endif

namespace nl::feature::needle {

// Command parameter types need external linkage.
struct NeedleFoundParams {
    int x = 0;
    int z = 0;
};

} // namespace nl::feature::needle

NL_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    using nl::feature::needle::NeedleFoundParams;
    namespace compass = nl::feature::compass;
    cmd.overload<NeedleFoundParams>().text("needle").text("found").required("x").required("z").execute(
        [](CommandOrigin const& origin, CommandOutput& output, NeedleFoundParams const& params) {
            if (!nl::commands::isAdmin(origin)) return output.error("You need operator permission.");
            auto* player = nl::commands::playerOf(origin);
            if (!player) return output.error("Only players can use this command.");
            auto slot = compass::heldCompass(*player);
            if (!slot) return output.error("Hold a Nature's Compass.");
            auto data = compass::dataAt(*player, *slot).value_or(compass::CompassData{});
            auto prev = data.prevPos;
            prev.push_back({params.x, 64, params.z});
            data.succeed("minecraft:plains", params.x, params.z, std::move(prev), 0, true);
            if (!compass::storeAt(*player, *slot, data)) return output.error("Hold a Nature's Compass.");
            nl::log().info("needle: {} compass Found at {}, {} (debug)", player->getRealName(), params.x, params.z);
            output.success("Compass marked Found at " + std::to_string(params.x) + ", " + std::to_string(params.z));
        }
    );
#ifdef NL_CLIENT
    // Client build: the frame the local player's held compass shows now (e2e checks it).
    cmd.overload().text("needle").text("frame").execute([](CommandOrigin const& origin, CommandOutput& output) {
        if (!nl::commands::isAdmin(origin)) return output.error("You need operator permission.");
        auto* player = nl::commands::playerOf(origin);
        auto* local  = nl::client::currentPlayer();
        if (!player || !local) return output.error("Only the local player can use this command.");
        auto slot = compass::heldCompass(*player);
        if (!slot) return output.error("Hold a Nature's Compass.");
        ItemStack const& stack = compass::stackAt(*player, *slot);
        int frame = nl::client::needle::frameFor(static_cast<Mob*>(local), false, &stack);
        auto yaw  = local->getRotation().y;
        nl::log().info("needle: frame {} (yaw {:.1f})", frame, yaw);
        output.success("Needle frame " + std::to_string(frame));
    });
#endif
});

namespace {

using nl::selftest::Context;
namespace compass = nl::feature::compass;
namespace needle  = nl::feature::needle;

} // namespace

NL_REGISTER_SELFTEST("needle.target_from_data", [](Context& ctx) {
    auto stack = compass::makeCompass();
    ctx.require(!stack.isNull(), "compass item exists");
    ctx.check(needle::needleTarget(stack) == needle::NeedleTarget{}, "a new compass points at the spawn");

    compass::CompassData d;
    d.startSearch("minecraft:desert");
    compass::writeData(stack, d);
    ctx.check(!needle::needleTarget(stack).found, "Searching points at the spawn");

    d.succeed("minecraft:desert", -1234, 5678, {{-1234, 70, 5678}}, 12, true);
    compass::writeData(stack, d);
    auto t = needle::needleTarget(stack);
    ctx.check(t.found && t.x == -1234 && t.z == 5678, "Found points at found_x / found_z");

    d.fail("minecraft:desert", 2600, 99);
    compass::writeData(stack, d);
    ctx.check(!needle::needleTarget(stack).found, "Not Found points at the spawn");

    d.clearSearchData();
    compass::writeData(stack, d);
    ctx.check(!needle::needleTarget(stack).found, "a reset compass points at the spawn");
});

#ifdef NL_CLIENT

NL_REGISTER_SELFTEST("needle.icon_frames", [](Context& ctx) {
    namespace cn = nl::client::needle;
    auto stack   = compass::makeCompass();
    ctx.require(!stack.isNull(), "compass item exists");
    Item const* item = stack.mItem.get();
    ctx.require(item != nullptr, "the stack has an item");
    for (int frame = 0; frame < nl::core::kNeedleFrames; ++frame) {
        auto info = item->getIconInfo(stack, frame, true);
        ctx.check(*info.mIconName == cn::iconName(), "icon name naturescompass (frame " + std::to_string(frame) + ")");
        ctx.check(info.mIconFrame == frame + 1, "frame " + std::to_string(frame) + " -> texture entry " + std::to_string(frame + 1));
        ctx.check(info.mIconType == ItemIconInfoType::Icon, "item atlas icon");
    }
    ctx.check(item->getIconInfo(stack, -1, false).mIconFrame == 17, "an unknown frame shows the needle pointing up");
    // Other items are untouched.
    auto vanilla = nl::game::makeItem("minecraft:stick");
    if (!vanilla.isNull()) {
        ctx.check(*vanilla.mItem->getIconInfo(vanilla, 0, true).mIconName != cn::iconName(), "a stick keeps its icon");
    }
});

NL_REGISTER_SELFTEST("needle.points_at_found", [](Context& ctx) {
    auto& player = ctx.player();
    auto  stack  = compass::makeCompass();
    ctx.require(!stack.isNull(), "compass item exists");
    Item const* item = stack.mItem.get();
    (void)item->getIconInfo(stack, 0, true); // lets the frame hook recognise this Item

    auto pos = player.getFeetPos();
    auto yaw = player.getRotation().y;
    int  px  = static_cast<int>(std::floor(pos.x));
    int  pz  = static_cast<int>(std::floor(pos.z));
    struct Case {
        char const* name;
        int         dx, dz;
    };
    int frames[4]{};
    int i = 0;
    for (auto c : {Case{"north", 0, -200}, Case{"east", 200, 0}, Case{"south", 0, 200}, Case{"west", -200, 0}}) {
        compass::CompassData d;
        d.succeed("minecraft:plains", px + c.dx, pz + c.dz, {{px + c.dx, 64, pz + c.dz}}, 1, true);
        compass::writeData(stack, d);
        // A holder that is not the local player: NC's needle without wobble.
        int got      = item->getAnimationFrameFor(&player, false, &stack, true);
        int expected = nl::core::needleFrame(
            nl::core::staticNeedleAngle(nl::core::angleToTarget(pos.x, pos.z, px + c.dx, pz + c.dz), yaw)
        );
        ctx.check(got == expected, std::string{"target "} + c.name + ": frame " + std::to_string(got) + ", expected " + std::to_string(expected));
        frames[i++] = got;
    }
    // Quarter turns are 8 frames apart, clockwise.
    for (int k = 0; k < 4; ++k) {
        int diff = (frames[(k + 1) % 4] - frames[k] + 32) % 32;
        ctx.check(diff == 8, "north/east/south/west are a quarter turn apart (diff " + std::to_string(diff) + ")");
    }
});

NL_REGISTER_SELFTEST("needle.spins_without_holder", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto stack = compass::makeCompass();
    ctx.require(!stack.isNull(), "compass item exists");
    Item const* item = stack.mItem.get();
    (void)item->getIconInfo(stack, 0, true);
    compass::CompassData d;
    d.succeed("minecraft:plains", 0, 0, {{0, 64, 0}}, 1, true);
    compass::writeData(stack, d);
    auto&         player = ctx.player();
    std::set<int> dropped, noHolder;
    for (int t = 0; t < 40; ++t) {
        dropped.insert(item->getAnimationFrameFor(&player, true, &stack, true)); // item entity
        noHolder.insert(item->getAnimationFrameFor(nullptr, false, &stack, true));
        co_await ctx.ticks(1);
    }
    for (auto const* frames : {&dropped, &noHolder}) {
        ctx.check(*frames->begin() >= 0 && *frames->rbegin() < nl::core::kNeedleFrames, "spinning frames in range");
        ctx.check(frames->size() >= 3, "the needle moves without a living holder (" + std::to_string(frames->size()) + " frames in 2 s)");
    }
});

#endif // NL_CLIENT
