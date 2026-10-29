// The biome search (docs/SPEC.md section 3.3): one cooperative worker per compass, run on the
// server thread after each level tick within NC's per-tick time budget, oldest first.
//
// The spiral, the y levels and the Search for Next connection check are pure logic in
// src/core/Spiral; this file samples the dimension's biome source, writes the compass and runs
// the workers.

#include "feature/compass/Search.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/string/HashedString.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/biome/Biome.h"
#include "mc/world/level/biome/GetBiomeOptions.h"
#include "mc/world/level/biome/source/BiomeSource.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/dimension/DimensionHeightRange.h"
#include "mc/world/level/dimension/DimensionType.h"
#include "mc/world/level/levelgen/WorldGenerator.h"

#include "core/Spiral.h"
#include "feature/compass/Actions.h"
#include "feature/compass/CompassConfig.h"
#include "feature/compass/Hud.h"
#include "feature/search/SearchWorker.h"
#include "game/WorldDb.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace nl::feature::search {

namespace {

using Clock = std::chrono::steady_clock;

/// NC: a server tick is 50 ms; workers get what the tick left, at least 10 ms.
constexpr auto kTickLength = std::chrono::milliseconds(50);
constexpr auto kMinBudget  = std::chrono::milliseconds(10);
/// NC's y step between two sampled levels.
constexpr int kYStep = 64;

/// "minecraft:plains" -> "plains"; other namespaces keep the full id.
std::string withoutMinecraftNamespace(std::string const& id) {
    constexpr std::string_view prefix = "minecraft:";
    if (id.starts_with(prefix)) return id.substr(prefix.size());
    return id;
}

std::string fullId(std::string const& name) { return name.find(':') == std::string::npos ? "minecraft:" + name : name; }

/// NC's worker id: 8 random hex digits.
std::string newWorkerId() {
    static std::mt19937 rng{std::random_device{}()};
    static char const   hex[] = "0123456789abcdef";
    std::string         id(8, '0');
    for (auto& c : id) c = hex[rng() % 16];
    return id;
}

std::shared_ptr<Dimension> dimensionOf(int dimId) {
    auto level = ll::service::getLevel();
    if (!level) return {};
    return level->getDimension(DimensionType(dimId)).lock();
}

BiomeSource const* biomeSourceOf(Dimension& dim) {
    WorldGenerator* gen = dim.mWorldGenerator;
    return gen ? &gen->getBiomeSource() : nullptr;
}

/// The biome of the noise cell holding the block position (NC: QuartPos.fromBlock + getNoiseBiome).
Biome const* biomeAt(BiomeSource const& source, int x, int y, int z) {
    BlockPos pos{
        core::blockFromQuart(core::quartFromBlock(x)),
        core::blockFromQuart(core::quartFromBlock(y)),
        core::blockFromQuart(core::quartFromBlock(z))
    };
    GetBiomeOptions options{pos, false};
    return source._getBiome(options);
}

struct Worker {
    std::string        logId    = newWorkerId();
    std::int64_t       uid      = 0;
    std::int64_t       playerId = 0;
    int                dimId    = 0;
    std::string        biomeId; ///< "minecraft:plains"
    std::string        shortId; ///< "plains" (biome names may be registered without the namespace)
    core::SpiralSearch search;

    explicit Worker(core::SpiralSearch::Params params) : search(std::move(params)) {}

    void logLine(std::string_view what) const {
        log().info("BiomeSearchWorker {}: {}: {} radius, {} samples", logId, what, search.radius(), search.samples());
    }
};

/// Running workers in start order (NC's worker list: the first one gets the whole budget).
std::vector<std::unique_ptr<Worker>> gWorkers;

Player* findPlayer(std::int64_t playerId) {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    ActorUniqueID id;
    id.rawID = playerId;
    return level->getPlayer(id);
}

/// Writes the radius progress (NC: while searching, whenever the radius passes a multiple of 500).
void writeProgress(Worker const& w) {
    auto* player = findPlayer(w.playerId);
    if (!player) return;
    auto slot = compass::findCompass(*player, w.uid);
    if (!slot) return;
    auto data = compass::dataAt(*player, *slot);
    if (!data) return;
    data->searchRadius = w.search.progressRadius();
    compass::storeAt(*player, *slot, *data);
    hud::refresh(*player);
}

void succeed(Worker const& w) {
    w.logLine("Search succeeded");
    auto* player = findPlayer(w.playerId);
    auto  slot   = player ? compass::findCompass(*player, w.uid) : std::nullopt;
    auto  data   = slot ? compass::dataAt(*player, *slot) : std::nullopt;
    if (!data) {
        log().warn("BiomeSearchWorker {}: Invalid compass after search", w.logId);
        return;
    }
    std::vector<compass::PrevPos> positions;
    positions.reserve(w.search.prevPos().size());
    for (auto const& p : w.search.prevPos()) positions.push_back({p.x, p.y, p.z});
    auto const& found = w.search.found();
    data->succeed(w.biomeId, found.x, found.z, std::move(positions), w.search.samples(), compass::displayCoordinates());
    if (!actions::hasInfiniteXp(*player)) data->damage += 1;
    compass::storeAt(*player, *slot, *data);
    actions::refreshDurability(*player); // show the new damage on the bar at once (docs/SPEC.md 3.7)
    hud::refresh(*player);
}

void fail(Worker const& w) {
    w.logLine("Search failed");
    auto* player = findPlayer(w.playerId);
    auto  slot   = player ? compass::findCompass(*player, w.uid) : std::nullopt;
    auto  data   = slot ? compass::dataAt(*player, *slot) : std::nullopt;
    if (!data) {
        log().warn("BiomeSearchWorker {}: Invalid compass after search", w.logId);
        return;
    }
    data->fail(w.biomeId, w.search.radius(), w.search.samples());
    compass::storeAt(*player, *slot, *data);
    hud::refresh(*player);
}

/// Runs the first worker's units until the deadline (NC WorldWorkerManager post-tick).
void runWorkers(Clock::time_point tickStart) {
    if (gWorkers.empty()) return;
    auto now      = Clock::now();
    auto budget   = std::max<Clock::duration>(kTickLength - (now - tickStart), kMinBudget);
    auto deadline = now + budget;

    // Biome source of the current worker's dimension (looked up again for each worker, never
    // kept across ticks).
    Worker*                    current = nullptr;
    std::shared_ptr<Dimension> dim;
    BiomeSource const*         source = nullptr;

    while (!gWorkers.empty() && Clock::now() < deadline) {
        Worker& w = *gWorkers.front();
        if (&w != current) {
            current = &w;
            // The player went offline: the search is dropped (no data written).
            if (!findPlayer(w.playerId)) {
                w.logLine("Search stopped");
                gWorkers.erase(gWorkers.begin());
                current = nullptr;
                continue;
            }
            dim    = dimensionOf(w.dimId);
            source = dim ? biomeSourceOf(*dim) : nullptr;
        }
        // No biome source (the dimension went away) or the limits are reached: failure.
        if (!source || !w.search.hasWork()) {
            fail(w);
            gWorkers.erase(gWorkers.begin());
            current = nullptr;
            continue;
        }
        auto event = w.search.doWork([&](int x, int y, int z) {
            Biome const* biome = biomeAt(*source, x, y, z);
            if (!biome) return false;
            auto const& name = biome->mHash->getString();
            return name == w.biomeId || name == w.shortId;
        });
        if (event == core::SpiralSearch::Event::Progress) {
            writeProgress(w);
        } else if (event == core::SpiralSearch::Event::Succeeded) {
            succeed(w);
            gWorkers.erase(gWorkers.begin());
            current = nullptr;
        }
    }
}

void removeWorker(std::int64_t uid) {
    std::erase_if(gWorkers, [&](auto const& w) {
        if (w->uid != uid) return false;
        w->logLine("Search stopped");
        return true;
    });
}

} // namespace

// ---- internals (SearchWorker.h) ------------------------------------------------------------------

std::string sampleBiomeId(int dimId, int x, int y, int z) {
    auto dim = dimensionOf(dimId);
    if (!dim) return {};
    auto const* source = biomeSourceOf(*dim);
    if (!source) return {};
    auto const* biome = biomeAt(*source, x, y, z);
    return biome ? fullId(biome->mHash->getString()) : std::string{};
}

std::vector<int> yLevelsFor(int dimId, int startY) {
    auto dim = dimensionOf(dimId);
    if (!dim) return {};
    // NC: outFromOrigin(startY, minY + 1, maxY, 64) with Java's inclusive top y. The start y is
    // clamped into those bounds: Bedrock's Nether is only 128 high, so a player on the Nether roof
    // (inside Java's 256-high Nether) would otherwise get no level at all.
    DimensionHeightRange const& range = dim->mHeightRange;
    int                         lower = range.mMin + 1;
    int                         upper = range.mMax - 1;
    return core::outFromOrigin(std::clamp(startY, lower, upper), lower, upper, kYStep);
}

bool startWorker(
    Player&                              player,
    compass::CompassSlot                 slot,
    std::string const&                   biomeId,
    BlockPos const&                      start,
    int                                  dimId,
    std::vector<compass::PrevPos> const& prevPos
) {
    auto data = compass::dataAt(player, slot);
    if (!data) return false;
    if (data->uid) stopSearch(data->uid);
    data->startSearch(biomeId);
    auto uid = data->ensureUid();
    if (!compass::storeAt(player, slot, *data)) return false;
    hud::refresh(player);

    core::SpiralSearch::Params params;
    params.startX      = start.x;
    params.startY      = start.y;
    params.startZ      = start.z;
    params.sampleSpace = compass::sampleSpace();
    params.maxRadius   = compass::maxSearchRadius();
    params.maxSamples  = compass::maxSamples();
    params.yLevels     = yLevelsFor(dimId, start.y);
    for (auto const& p : prevPos) params.prevPos.push_back({p.x, p.y, p.z});

    auto w      = std::make_unique<Worker>(std::move(params));
    w->uid      = uid;
    w->playerId = player.getOrCreateUniqueID().rawID;
    w->dimId    = dimId;
    w->biomeId  = biomeId;
    w->shortId  = withoutMinecraftNamespace(biomeId);
    if (!w->search.valid()) {
        // NC: maxRadius or sampleSpace 0 -> the search fails at once.
        fail(*w);
        return true;
    }
    log().info(
        "BiomeSearchWorker {}: Starting search: {} sample space, {} max samples, {} max radius, {} previous locations",
        w->logId,
        w->search.params().sampleSpace,
        w->search.params().maxSamples,
        w->search.params().maxRadius,
        w->search.params().prevPos.size()
    );
    gWorkers.push_back(std::move(w));
    return true;
}

std::vector<std::int64_t> runningUids() {
    std::vector<std::int64_t> out;
    for (auto const& w : gWorkers) out.push_back(w->uid);
    return out;
}

// ---- contract (Search.h) -------------------------------------------------------------------------

bool startSearch(Player& player, compass::CompassSlot slot, std::string const& biomeId, BlockPos const& start) {
    return startWorker(player, slot, biomeId, start, static_cast<int>(player.getDimensionId()), {});
}

bool startSearchForNext(Player& player, compass::CompassSlot slot) {
    auto data = compass::dataAt(player, slot);
    if (!data || data->state != compass::CompassState::Found || data->biomeId.empty()) return false;
    // startSearch clears prev_pos on the compass; the worker keeps a copy and writes it back.
    auto prev  = data->prevPos;
    auto biome = data->biomeId;
    return startWorker(player, slot, biome, player.getFeetBlockPos(), static_cast<int>(player.getDimensionId()), prev);
}

void stopSearch(std::int64_t uid) {
    if (uid) removeWorker(uid);
}

bool isSearching(std::int64_t uid) {
    return uid && std::any_of(gWorkers.begin(), gWorkers.end(), [&](auto const& w) { return w->uid == uid; });
}

void stopAll() { gWorkers.clear(); }

// ---- tick ----------------------------------------------------------------------------------------

namespace {

/// NC measures the tick from ServerTickEvent.Pre and runs the workers at ServerTickEvent.Post: we
/// time the server level's tick and run them right after it.
LL_TYPE_INSTANCE_HOOK(SearchLevelTickHook, ll::memory::HookPriority::Normal, Level, &Level::$tick, void) {
    auto start = Clock::now();
    origin();
    if (gWorkers.empty() || isClientSide()) return;
    runWorkers(start);
}

} // namespace

/// Runs the workers every server tick and drops them when the world closes.
class SearchModule {
public:
    void install() {
        SearchLevelTickHook::hook();
        if (mListening) return;
        mListening = true;
        game::db::onWorldClosed([] { stopAll(); });
    }
    void uninstall() {
        SearchLevelTickHook::unhook();
        stopAll();
    }

private:
    bool mListening = false;
};

} // namespace nl::feature::search

NL_REGISTER_MODULE(nl::feature::search::SearchModule);
