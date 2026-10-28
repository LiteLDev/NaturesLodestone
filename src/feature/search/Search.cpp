// Default (minimal) implementation of the search contract (src/feature/compass/Search.h).
// Unit U3 `search` replaces this file with the real spiral search. This stub writes the search
// start data and, one tick later, finds the compass again by its uid and marks it Not Found
// (radius 0, 0 samples), so the whole use flow can already be exercised.

#include "feature/compass/Search.h"

#include <unordered_map>

#include "ll/api/chrono/GameChrono.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/thread/ServerThreadExecutor.h"

#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/Level.h"

#include "feature/compass/Hud.h"
#include "game/WorldDb.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace nl::feature::search {

namespace {

/// Running searches: compass uid -> generation (a newer search of the same compass replaces it).
std::unordered_map<std::int64_t, std::uint64_t> gRunning;
std::uint64_t                                   gGeneration = 0;

void finishLater(std::int64_t playerId, std::int64_t uid, std::uint64_t generation, std::string biomeId) {
    ll::coro::keepThis([playerId, uid, generation, biomeId = std::move(biomeId)]() -> ll::coro::CoroTask<> {
        co_await ll::chrono::ticks(1);
        auto it = gRunning.find(uid);
        if (it == gRunning.end() || it->second != generation) co_return; // stopped or replaced meanwhile
        gRunning.erase(it);
        auto level = ll::service::getLevel();
        if (!level) co_return;
        ActorUniqueID id;
        id.rawID     = playerId;
        auto* player = level->getPlayer(id);
        if (!player) co_return;
        auto slot = compass::findCompass(*player, uid);
        if (!slot) co_return;
        auto data = compass::dataAt(*player, *slot);
        if (!data || data->state != compass::CompassState::Searching) co_return;
        data->fail(biomeId, 0, 0);
        compass::storeAt(*player, *slot, *data);
        hud::refresh(*player);
        log().info("Compass search for {} by {} not found (default search)", biomeId, player->getRealName());
    }).launch(ll::thread::ServerThreadExecutor::getDefault());
}

} // namespace

bool startSearch(Player& player, compass::CompassSlot slot, std::string const& biomeId, BlockPos const& start) {
    auto data = compass::dataAt(player, slot);
    if (!data) return false;
    if (data->uid) stopSearch(data->uid);
    data->startSearch(biomeId);
    auto uid = data->ensureUid();
    if (!compass::storeAt(player, slot, *data)) return false;
    auto generation = ++gGeneration;
    gRunning[uid]   = generation;
    log().info(
        "Compass search for {} by {} started at {} {} {}",
        biomeId,
        player.getRealName(),
        start.x,
        start.y,
        start.z
    );
    hud::refresh(player);
    finishLater(player.getOrCreateUniqueID().rawID, uid, generation, biomeId);
    return true;
}

bool startSearchForNext(Player& player, compass::CompassSlot slot) {
    auto data = compass::dataAt(player, slot);
    if (!data || data->state != compass::CompassState::Found || data->biomeId.empty()) return false;
    return startSearch(player, slot, data->biomeId, player.getFeetBlockPos());
}

void stopSearch(std::int64_t uid) { gRunning.erase(uid); }

bool isSearching(std::int64_t uid) { return gRunning.contains(uid); }

void stopAll() { gRunning.clear(); }

/// Drops running searches when a world closes and when the mod is disabled.
class SearchModule {
public:
    void install() {
        if (mHooked) return;
        mHooked = true;
        game::db::onWorldClosed([] { stopAll(); });
    }
    void uninstall() { stopAll(); }

private:
    bool mHooked = false;
};

} // namespace nl::feature::search

NL_REGISTER_MODULE(nl::feature::search::SearchModule);
