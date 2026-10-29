#include "feature/hud/HudModel.h"

#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"

#include "feature/compass/Biomes.h"
#include "feature/compass/CompassConfig.h"
#include "mod/I18n.h"

namespace nl::feature::hud {

namespace {

std::string label(std::string_view key, std::string_view fallback, std::string_view locale) {
    return std::string(translate(key, fallback, locale));
}

} // namespace

core::hud::Labels labelsFor(std::string_view locale) {
    core::hud::Labels l;
    l.status      = label("string.naturescompass.status", "Status", locale);
    l.searching   = label("string.naturescompass.searching", "Searching", locale);
    l.found       = label("string.naturescompass.found", "Found", locale);
    l.notFound    = label("string.naturescompass.notFound", "Not Found", locale);
    l.biome       = label("string.naturescompass.biome", "Biome", locale);
    l.radius      = label("string.naturescompass.radius", "Radius", locale);
    l.coordinates = label("string.naturescompass.coordinates", "Coordinates", locale);
    l.distance    = label("string.naturescompass.distance", "Distance", locale);
    l.samples     = label("string.naturescompass.samples", "Samples", locale);
    return l;
}

core::hud::Input inputFor(compass::CompassData const& data, std::string_view locale, int playerX, int playerZ) {
    core::hud::Input in;
    in.state = static_cast<core::hud::State>(data.state);
    if (data.state != compass::CompassState::Inactive) {
        in.biomeName = biomes::biomeDisplayName(data.biomeId, locale, false); // NC: raw name on the HUD
    }
    in.foundX        = data.foundX;
    in.foundZ        = data.foundZ;
    in.searchRadius  = data.searchRadius;
    in.samples       = data.samples;
    in.displayCoords = data.displayCoords;
    in.playerX       = playerX;
    in.playerZ       = playerZ;
    return in;
}

std::vector<core::hud::Line> linesFor(Player const& player, std::string_view locale) {
    auto slot = compass::heldCompass(player);
    if (!slot) return {};
    auto data = compass::dataAt(player, *slot);
    if (!data || data->state == compass::CompassState::Inactive) return {};
    auto pos = player.getFeetBlockPos();
    return core::hud::buildLines(inputFor(*data, locale, pos.x, pos.z), labelsFor(locale));
}

std::string fallbackText(std::vector<core::hud::Line> const& lines) {
    return core::hud::encodeFallback(
        lines,
        compass::overlayOnRight() ? core::hud::Side::Right : core::hud::Side::Left,
        compass::overlayLineOffset()
    );
}

} // namespace nl::feature::hud
