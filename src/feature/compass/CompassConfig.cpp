#include "feature/compass/CompassConfig.h"

#include <algorithm>
#include <cctype>

#include "mod/Config.h"

NL_REGISTER_CONFIG("naturescompass", nl::feature::compass::CompassConfig);
NL_REGISTER_CONFIG("naturescompass_client", nl::feature::compass::CompassClientConfig);

namespace nl::feature::compass {

CompassConfig const& rawConfig() { return config::get<CompassConfig>(); }

bool allowTeleport() { return rawConfig().allowTeleport; }
int  maxNextSearches() { return std::clamp(rawConfig().maxNextSearches, 0, 10000); }
bool displayCoordinates() { return rawConfig().displayCoordinates; }
int  radiusModifier() { return std::clamp(rawConfig().radiusModifier, 0, 1000000); }
int  sampleSpaceModifier() { return std::clamp(rawConfig().sampleSpaceModifier, 0, 1000000); }
int  defaultXpLevels() { return std::clamp(rawConfig().defaultXpLevels, 0, 3); }
int  maxSamples() { return std::clamp(rawConfig().maxSamples, 0, 1000000); }
int  compassDurability() { return std::clamp(rawConfig().compassDurability, 0, 10000); }
int  maxSearchRadius() { return kBiomeSize * radiusModifier(); }
int  sampleSpace() { return kBiomeSize * sampleSpaceModifier(); }

CompassClientConfig const& rawClientConfig() { return config::get<CompassClientConfig>(); }

bool displayWithChatOpen() { return rawClientConfig().displayWithChatOpen; }
bool fixBiomeNames() { return rawClientConfig().fixBiomeNames; }
int  overlayLineOffset() { return std::clamp(rawClientConfig().overlayLineOffset, 0, 50); }

bool overlayOnRight() {
    std::string side = rawClientConfig().overlaySide;
    std::transform(side.begin(), side.end(), side.begin(), [](unsigned char c) { return std::toupper(c); });
    return side == "RIGHT";
}

} // namespace nl::feature::compass
