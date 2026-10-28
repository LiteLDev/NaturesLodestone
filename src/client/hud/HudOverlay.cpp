// Native HUD overlay of the client build (docs/SPEC.md 4.3): NC's line block for the local player,
// drawn after the HUD screen with the game's UI font, in local worlds and on servers alike (it reads
// the held compass's synced item data). Marked fallback titles from the server (src/feature/hud/)
// are dropped here, so this overlay is the only HUD block a modded client shows.

#ifdef NL_CLIENT

#include <algorithm>
#include <string>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/gui/ScreenSizeData.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/network/packet/SetTitlePacket.h"

#include "client/Client.h"
#include "core/HudLines.h"
#include "feature/compass/CompassConfig.h"
#include "feature/hud/HudModel.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace nl::client::hud {

namespace {

namespace model = nl::feature::hud;
namespace core  = nl::core::hud;

/// The HUD screen (the name `ScreenView::getScreenName` / `getTopScreenName` report).
bool isHudScreen(std::string_view name) { return name == "hud_screen"; }
bool isChatScreen(std::string_view name) { return name.find("chat_screen") != std::string_view::npos; }

/// NC: hidden with the HUD hidden (F1) and while any screen but chat (displayWithChatOpen) is open.
bool overlayVisible(ClientInstance& client) {
    if (client.getOptions().getHideHud()) return false;
    auto top = client.getTopScreenName();
    if (isHudScreen(top)) return true;
    return isChatScreen(top) && feature::compass::displayWithChatOpen();
}

void drawOverlay(ll::event::AfterUIRenderEvent& ev) {
    { // TEMP diagnostics
        static std::vector<std::string> seen;
        auto                            n = ev.screenView().getScreenName();
        if (seen.size() < 12 && std::find(seen.begin(), seen.end(), n) == seen.end()) {
            seen.push_back(n);
            auto* c = currentClient();
            log().info("HUD diag screen '{}' top '{}'", n, c ? c->getTopScreenName() : "");
        }
    }
    if (!isHudScreen(ev.screenView().getScreenName())) return;
    auto* client = currentClient();
    if (!client) return;
    auto* player = client->getLocalPlayer();
    if (!player || !overlayVisible(*client)) return;
    auto lines = model::linesFor(*player, clientLocale());
    if (lines.empty()) return;

    auto& ctx    = ev.uiRenderContext();
    auto  handle = client->getFontHandle();
    auto& font   = handle.getFont();
    float width  = client->getGuiData()->mScreenSizeData->clientUIScreenSize->x;
    bool  right  = feature::compass::overlayOnRight();
    int   offset = feature::compass::overlayLineOffset();
    auto  align  = right ? ui::TextAlignment::Right : ui::TextAlignment::Left;
    for (auto const& line : lines) {
        float         y = static_cast<float>(core::lineY(line.index, offset));
        RectangleArea rect{};
        rect._x0 = right ? 0.f : static_cast<float>(core::kEdgeMargin);
        rect._x1 = right ? width - core::kEdgeMargin : width;
        rect._y0 = y;
        rect._y1 = y + 9.f;
        TextMeasureData  measure{1.f, 0.f, true, false, false, align};
        CaretMeasureData caret{-1, false};
        mce::Color       color(line.value ? core::kValueColor : core::kLabelColor);
        ctx.drawText(font, rect, std::string(line.text), color, 1.f, align, measure, caret);
    }
    ctx.flushText(0.f, std::nullopt);
    static bool logged = false;
    if (!logged) {
        logged = true;
        log().info("HUD overlay drawn ({} lines, screen width {})", lines.size(), width);
    }
}

} // namespace

/// Drops the server's marked fallback titles: this client draws the block natively.
LL_TYPE_INSTANCE_HOOK(
    DropFallbackTitleHook,
    ll::memory::HookPriority::Normal,
    ClientNetworkHandler,
    &ClientNetworkHandler::$handle,
    void,
    NetworkIdentifier const& source,
    SetTitlePacket const&    packet
) {
    if (packet.mType == SetTitlePacketPayload::TitleType::Title && core::isFallback(*packet.mTitleText)) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            log().info("HUD fallback title dropped: the native overlay draws the HUD");
        }
        return;
    }
    origin(source, packet);
}

class HudOverlayModule {
public:
    void install() {
        DropFallbackTitleHook::hook();
        mListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::AfterUIRenderEvent>(
            [](ll::event::AfterUIRenderEvent& ev) { drawOverlay(ev); }
        );
    }
    void uninstall() {
        ll::event::EventBus::getInstance().removeListener(mListener);
        DropFallbackTitleHook::unhook();
    }

private:
    ll::event::ListenerPtr mListener;
};

} // namespace nl::client::hud

NL_REGISTER_MODULE(nl::client::hud::HudOverlayModule);

#endif // NL_CLIENT
