// The HUD contract (src/feature/compass/Hud.h) and the server fallback HUD (docs/SPEC.md 4.3).
//
// Players without the native overlay (vanilla clients on BDS) get NC's line block as a marked
// title string (core/HudLines.h). The RP's hud_screen.json modification (assets/pack/rp/ui/)
// keeps the last marked title, hides it from the vanilla title and draws it as the line block at
// NC's place. The string is sent only when it changes (state, data, live distance, language,
// server naturescompass_client side/offset) and again after a dimension change or respawn. An
// Inactive / missing compass sends nothing, except one bare marker that clears a block shown
// before.
//
// Clients running the client build draw the block natively (src/client/hud/) and drop marked
// titles in their network handler, so the server does not need to know who runs the mod: it
// sends the fallback to everyone (in local worlds too, where the host's client drops it).

#include "feature/compass/Hud.h"

#include <unordered_map>
#include <unordered_set>

#include "ll/api/command/CommandHandle.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerRespawnEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/network/packet/SetTitlePacket.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/Level.h"

#include "feature/compass/CompassConfig.h"
#include "feature/compass/CompassData.h"
#include "feature/compass/Search.h"
#include "feature/hud/HudModel.h"
#include "game/WorldDb.h"
#include "mod/Commands.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace nl::feature::hud {

namespace {

/// Ticks between two fallback updates (the live distance changes as the player moves).
constexpr int kUpdateInterval = 2;
/// Ticks between two sweeps that forget players no longer online.
constexpr int kPruneInterval = 200;

struct Sent {
    std::string text;          ///< last string sent
    int         dimension = 0; ///< dimension at that time (a change re-sends)
    bool        force     = false;
    std::size_t count     = 0; ///< strings sent since the player joined
};

std::unordered_map<std::int64_t, Sent> gSent;

std::int64_t keyOf(Player const& player) { return player.getOrCreateUniqueID().rawID; }

void sendTitle(Player& player, std::string const& text) {
    SetTitlePacket pkt(SetTitlePacketPayload::TitleType::Title, text, std::nullopt);
    pkt.sendTo(player);
}

} // namespace

namespace detail {

std::optional<std::string> lastSent(Player const& player) {
    auto it = gSent.find(keyOf(player));
    if (it == gSent.end()) return std::nullopt;
    return it->second.text;
}

std::size_t sendCount(Player const& player) {
    auto it = gSent.find(keyOf(player));
    return it == gSent.end() ? 0 : it->second.count;
}

void update(Player& player) {
    auto lines = linesFor(player, localeOf(player));
    auto key   = keyOf(player);
    auto it    = gSent.find(key);
    int  dim   = static_cast<int>(player.getDimensionId());
    if (lines.empty()) {
        // Nothing to show: never send for a player that shows nothing; clear a shown block once.
        if (it == gSent.end()) return;
        auto shown = core::hud::decodeFallback(it->second.text);
        if (shown && !shown->lines.empty()) {
            auto clear = fallbackText({});
            sendTitle(player, clear);
            it->second.text = clear;
            ++it->second.count;
        }
        it->second.force     = false;
        it->second.dimension = dim;
        return;
    }
    auto text = fallbackText(lines);
    if (it == gSent.end()) it = gSent.emplace(key, Sent{}).first;
    auto& s = it->second;
    if (!s.force && s.count > 0 && s.text == text && s.dimension == dim) return;
    sendTitle(player, text);
    s.text      = std::move(text);
    s.dimension = dim;
    s.force     = false;
    ++s.count;
}

} // namespace detail

void refresh(Player& player) { detail::update(player); }

// ---- debug sub-commands (e2e) -------------------------------------------------------------------

struct HudOffsetParams {
    int offset = 1;
};

namespace {

/// Writes a sample HUD state into the held compass (operators; for e2e screenshots).
bool applyDebugState(Player& player, std::string_view state) {
    auto slot = compass::heldCompass(player);
    if (!slot) return false;
    auto data = compass::dataAt(player, *slot);
    if (!data) return false;
    if (data->uid) search::stopSearch(data->uid);
    auto        pos   = player.getFeetBlockPos();
    std::string biome = "minecraft:cherry_grove";
    if (state == "searching") {
        data->startSearch(biome);
        data->searchRadius = 1500;
    } else if (state == "found" || state == "foundnocoords") {
        data->succeed(
            biome,
            pos.x + 300,
            pos.z - 400,
            {
                {pos.x + 300, 64, pos.z - 400}
        },
            4321,
            state == "found"
        );
    } else if (state == "notfound") {
        data->fail(biome, compass::maxSearchRadius(), compass::maxSamples());
    } else {
        data->clearSearchData();
    }
    if (!compass::storeAt(player, *slot, *data)) return false;
    refresh(player);
    log().info("HUD debug state {} for {}", state, player.getRealName());
    return true;
}

void registerCommands(ll::command::CommandHandle& cmd) {
    for (std::string state : {"searching", "found", "foundnocoords", "notfound", "inactive"}) {
        cmd.overload().text("hud").text(state).execute([state](CommandOrigin const& origin, CommandOutput& output) {
            if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
            auto* player = commands::playerOf(origin);
            if (!player) return output.error("Players only.");
            if (!applyDebugState(*player, state)) {
                return output.error(trFor(*player, "nl.hud.debug.holdCompass", "Hold a Nature's Compass."));
            }
            output.success(trFor(*player, "nl.hud.debug.state", "HUD state: {}", state));
        });
    }
    for (std::string side : {"LEFT", "RIGHT"}) {
        cmd.overload().text("hud").text("side").text(side).execute(
            [side](CommandOrigin const& origin, CommandOutput& output) {
                if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
                config::mut<compass::CompassClientConfig>().overlaySide = side;
                log().info("HUD debug overlaySide {}", side);
                output.success("overlaySide = " + side + " (until reload)");
            }
        );
    }
    cmd.overload<HudOffsetParams>().text("hud").text("offset").required("offset").execute(
        [](CommandOrigin const& origin, CommandOutput& output, HudOffsetParams const& p) {
            if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
            config::mut<compass::CompassClientConfig>().overlayLineOffset = p.offset;
            log().info("HUD debug overlayLineOffset {}", p.offset);
            output.success("overlayLineOffset = " + std::to_string(compass::overlayLineOffset()) + " (until reload)");
        }
    );
    for (bool on : {true, false}) {
        cmd.overload()
            .text("hud")
            .text("chat")
            .text(on ? "on" : "off")
            .execute([on](CommandOrigin const& origin, CommandOutput& output) {
                if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
                config::mut<compass::CompassClientConfig>().displayWithChatOpen = on;
                log().info("HUD debug displayWithChatOpen {}", on);
                output.success(std::string("displayWithChatOpen = ") + (on ? "true" : "false") + " (until reload)");
            });
    }
}

} // namespace

/// Runs the fallback updates and forgets players that leave.
class HudModule {
public:
    void install() {
        auto& bus = ll::event::EventBus::getInstance();
        mListeners.push_back(bus.emplaceListener<ll::event::ServerLevelTickEvent>([this](auto&) {
            if (++mTick % kUpdateInterval) return;
            auto level = ll::service::getLevel();
            if (!level) return;
            bool                             prune = mTick % kPruneInterval == 0;
            std::unordered_set<std::int64_t> online;
            level->forEachPlayer([&](Player& p) {
                detail::update(p);
                if (prune) online.insert(keyOf(p));
                return true;
            });
            // Players that left without a disconnect event (simulated players of selftests).
            if (prune) std::erase_if(gSent, [&](auto const& e) { return !online.contains(e.first); });
        }));
        mListeners.push_back(bus.emplaceListener<ll::event::PlayerDisconnectEvent>([](auto& ev) {
            gSent.erase(keyOf(ev.self()));
        }));
        mListeners.push_back(bus.emplaceListener<ll::event::PlayerRespawnEvent>([](auto& ev) {
            if (auto it = gSent.find(keyOf(ev.self())); it != gSent.end()) it->second.force = true;
        }));
        if (!mHooked) {
            mHooked = true;
            game::db::onWorldClosed([] { gSent.clear(); });
        }
    }
    void uninstall() {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto& l : mListeners) bus.removeListener(l);
        mListeners.clear();
        // Clear the blocks vanilla clients still show (the RP keeps the last marked title).
        if (auto level = ll::service::getLevel()) {
            level->forEachPlayer([](Player& p) {
                auto it = gSent.find(keyOf(p));
                if (it == gSent.end()) return true;
                auto shown = core::hud::decodeFallback(it->second.text);
                if (shown && !shown->lines.empty()) sendTitle(p, fallbackText({}));
                return true;
            });
        }
        gSent.clear();
    }

private:
    std::vector<ll::event::ListenerPtr> mListeners;
    int                                 mTick   = 0;
    bool                                mHooked = false;
};

} // namespace nl::feature::hud

NL_REGISTER_MODULE(nl::feature::hud::HudModule);
NL_REGISTER_SUBCOMMAND(nl::feature::hud::registerCommands);
