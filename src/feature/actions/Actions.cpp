// Default (minimal) implementation of the actions contract (src/feature/compass/Actions.h).
// Unit U4 `actions` replaces this file: use on blocks, XP costs, durability damage, the safe-y
// teleport. This stub already wires item use -> Select Biome screen / sneak reset, and starts
// searches, so the default flow works end to end.

#include "feature/compass/Actions.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/player/PlayerUseItemEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"

#include "feature/compass/Biomes.h"
#include "feature/compass/CompassConfig.h"
#include "feature/compass/CompassData.h"
#include "feature/compass/Gui.h"
#include "feature/compass/Hud.h"
#include "feature/compass/Search.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace nl::feature::actions {

namespace {

int xpLevel(Player const& player) {
    auto                     ref = player.getAttribute(Player::LEVEL());
    AttributeInstance const* ptr = ref.mPtr;
    return ptr ? static_cast<int>(ptr->mCurrentValue) : 0;
}

/// NC: takes the cost unless creative, never below level 0.
void takeXp(Player& player, int cost) {
    if (cost > 0 && !player.isCreative()) player.addLevels(-std::min(cost, xpLevel(player)));
}

} // namespace

bool onUse(Player& player, bool sneaking) {
    auto slot = compass::heldCompass(player);
    if (!slot) return false;
    auto data = compass::dataAt(player, *slot);
    if (!data) return false;
    if (sneaking) {
        if (data->uid) search::stopSearch(data->uid);
        data->clearSearchData();
        compass::storeAt(player, *slot, *data);
        hud::refresh(player);
        log().info("Compass reset by {}", player.getRealName());
        return true;
    }
    if (compass::isBroken(*data)) return false;
    gui::openSelectBiome(player);
    return true;
}

void search(Player& player, std::string const& biomeId) {
    auto slot = compass::heldCompass(player);
    if (!slot || compass::isBroken(compass::stackAt(player, *slot))) return;
    if (!search::startSearch(player, *slot, biomeId, player.getFeetBlockPos())) return;
    takeXp(player, biomes::xpLevelsFor(biomeId));
}

void searchForNext(Player& player) {
    auto slot = compass::heldCompass(player);
    if (!slot || compass::maxNextSearches() <= 0) return;
    auto data = compass::dataAt(player, *slot);
    if (!data || compass::isBroken(*data) || data->state != compass::CompassState::Found) return;
    if (!search::startSearchForNext(player, *slot)) return;
    takeXp(player, biomes::xpLevelsFor(data->biomeId));
}

void teleport(Player& player) {
    if (!canTeleport(player)) {
        log().info("Player {} tried to teleport but does not have permission.", player.getRealName());
        return;
    }
    auto slot = compass::heldCompass(player);
    if (!slot) return;
    auto data = compass::dataAt(player, *slot);
    if (!data || data->state != compass::CompassState::Found) return;
    // Stub height: the top of the found column when it is loaded (U4 adds NC's safe-height probe).
    auto&    region = player.getDimensionBlockSource();
    BlockPos column{data->foundX, 0, data->foundZ};
    if (!region.hasChunksAt(column, 0, false)) {
        log().info("Player {} teleport skipped: target not loaded (default teleport)", player.getRealName());
        return;
    }
    int  y = region.getAboveTopSolidBlock(data->foundX, data->foundZ, false, false);
    Vec3 target{static_cast<float>(data->foundX), static_cast<float>(y), static_cast<float>(data->foundZ)};
    player.teleport(target, player.getDimensionId(), player.getRotation());
    log().info("Player {} teleported to {} {}", player.getRealName(), data->foundX, data->foundZ);
}

bool canTeleport(Player const& player) {
    if (!compass::allowTeleport()) return false;
    if (player.isOperator() || player.getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors) return true;
    auto level = ll::service::getLevel();
    return level && level->getLevelData().mCheatsEnabled;
}

bool hasInfiniteXp(Player const& player) { return player.isCreative(); }

bool canAfford(Player const& player, int levels) {
    return levels <= 0 || hasInfiniteXp(player) || xpLevel(player) >= levels;
}

/// Item use -> `onUse`.
class ActionsModule {
public:
    void install() {
        mUse = ll::event::EventBus::getInstance().emplaceListener<ll::event::PlayerUseItemEvent>(
            [](ll::event::PlayerUseItemEvent& event) {
                if (!compass::isCompass(event.item())) return;
                auto& player = event.self();
                if (player.isClientSide()) return;
                onUse(player, player.isSneaking());
            }
        );
    }
    void uninstall() { ll::event::EventBus::getInstance().removeListener(mUse); }

private:
    ll::event::ListenerPtr mUse;
};

} // namespace nl::feature::actions

NL_REGISTER_MODULE(nl::feature::actions::ActionsModule);
