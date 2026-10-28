// Unit U4 `actions`: what a player does with the compass (docs/SPEC.md sections 3.2, 3.4, 3.5
// and 3.7). Implements src/feature/compass/Actions.h.
//
// - Item use (in the air and on blocks), one action per click, applied on the next tick.
// - Search / Search for Next: broken check, XP cost, the UI click sound.
// - Teleport: permission, NC's safe height probe, loading the target column first.
// - Durability: `CompassData::damage` mirrored onto the Bedrock durability bar.

#include "feature/actions/Actions.h"

#include <cmath>
#include <unordered_map>

#include "ll/api/chrono/GameChrono.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "ll/api/event/player/PlayerUseItemEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/thread/ServerThreadExecutor.h"

#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/CompoundTagVariant.h"
#include "mc/deps/nbt/IntTag.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/MinecraftPackets.h"
#include "mc/network/packet/PlaySoundPacket.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/material/Material.h"
#include "mc/world/level/storage/LevelData.h"

#include "core/SafeHeight.h"
#include "feature/compass/Biomes.h"
#include "feature/compass/CompassConfig.h"
#include "feature/compass/CompassData.h"
#include "feature/compass/Gui.h"
#include "feature/compass/Hud.h"
#include "feature/compass/Search.h"
#include "game/WorldDb.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace nl::feature::actions {

namespace {

using compass::CompassState;

// ---- helpers ------------------------------------------------------------------------------------

Player* findPlayer(std::int64_t uniqueId) {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    ActorUniqueID id;
    id.rawID = uniqueId;
    return level->getPlayer(id);
}

std::int64_t idOf(Player const& player) { return player.getOrCreateUniqueID().rawID; }

std::uint64_t currentTick() {
    auto level = ll::service::getLevel();
    return level ? level->getCurrentServerTick().tickID : 0;
}

int xpLevel(Player const& player) {
    auto                     ref = player.getAttribute(Player::LEVEL());
    AttributeInstance const* ptr = ref.mPtr;
    return ptr ? static_cast<int>(ptr->mCurrentValue) : 0;
}

/// NC: takes the cost unless creative, never below level 0, without checking that the player can
/// pay (the screen greys what the player can't afford).
void takeXp(Player& player, int cost) {
    if (cost <= 0 || hasInfiniteXp(player)) return;
    int levels = std::min(cost, xpLevel(player));
    if (levels > 0) player.addLevels(-levels);
}

/// The UI button click NC plays for Search / Search for Next, for this player only.
void playClick(Player& player) {
    auto packet = MinecraftPackets::createPacket(MinecraftPacketIds::PlaySound);
    if (!packet) return;
    auto&       sound = static_cast<PlaySoundPacket&>(*packet);
    auto const& pos   = player.getPosition();
    *sound.mName      = "random.click";
    // The packet carries the position in 1/8 blocks.
    *sound.mPos = BlockPos{
        static_cast<int>(std::floor(pos.x * 8.f)),
        static_cast<int>(std::floor(pos.y * 8.f)),
        static_cast<int>(std::floor(pos.z * 8.f))
    };
    sound.mVolume = 1.f;
    sound.mPitch  = 1.f;
    sound.sendTo(player);
}

// ---- item use -----------------------------------------------------------------------------------

/// Bedrock repeats use events while the button is held, and a click on a block can raise both the
/// block and the air event: events closer than this to the previous one belong to the same click.
constexpr std::uint64_t kSameClickTicks = 6;

std::unordered_map<std::int64_t, std::uint64_t> gLastUseTick;

/// True for the first event of a click (and refreshes the click window).
bool newClick(Player const& player) {
    auto now            = currentTick();
    auto [it, inserted] = gLastUseTick.try_emplace(idOf(player), now);
    bool fresh          = inserted || now < it->second || now - it->second > kSameClickTicks;
    it->second          = now;
    return fresh;
}

/// Applies the use on the next tick: item writes made inside the game's use handling could be
/// overwritten (or rolled back) by it.
void useNextTick(Player& player, bool sneaking) {
    ll::coro::keepThis([id = idOf(player), sneaking]() -> ll::coro::CoroTask<> {
        co_await ll::chrono::ticks(1);
        if (auto* player = findPlayer(id)) onUse(*player, sneaking);
    }).launch(ll::thread::ServerThreadExecutor::getDefault());
}

// ---- teleport -----------------------------------------------------------------------------------

/// NC "free": air, fire, liquid or replaceable.
bool isFree(BlockSource& region, BlockPos const& pos) {
    auto const& block = region.getBlock(pos);
    if (block.isAir()) return true;
    auto const& type = block.getBlockType();
    if (block.getMaterial().mLiquid) return true;
    auto const& name = block.getTypeName();
    if (name == "minecraft:fire" || name == "minecraft:soul_fire") return true;
    return type.canBeBuiltOver(block, region, pos);
}

/// NC's start height: the sea level of the dimension (63 in the overworld).
int seaLevel(Player const& player) {
    if (player.getDimensionId() == 0) return 63;
    return player.getDimension().mSeaLevel;
}

bool columnLoaded(BlockSource& region, int x, int z) {
    auto* chunk = region.getChunkAt(BlockPos{x, 0, z});
    return chunk && chunk->mLoadState->load() >= ChunkState::Loaded;
}

/// Teleports to whole-block coordinates in the current dimension, keeping yaw and pitch.
void moveTo(Player& player, int x, int y, int z) {
    if (player.getVehicle()) player.stopRiding(true, false, false, true);
    bool gliding  = player.isGliding();
    Vec3 velocity = player.getPosDelta();
    Vec2 rotation = player.getRotation();
    player.teleport(
        Vec3{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)},
        player.getDimensionId(),
        rotation
    );
    player.setFallDistance(0.f);
    // NC: unless gliding, no vertical motion and on the ground.
    auto& state = *player.mBuiltInComponents->mStateVectorComponent;
    if (gliding) {
        state.mPosDelta = velocity;
    } else {
        state.mPosDelta = Vec3{velocity.x, 0.f, velocity.z};
    }
}

/// Final step: the column is loaded; probe the height and move.
void finishTeleport(Player& player, int x, int z) {
    auto& region = player.getDimensionBlockSource();
    int   y      = safeTeleportY(region, seaLevel(player), x, z);
    moveTo(player, x, y, z);
    log().info("Player {} teleported to {} {} {}", player.getRealName(), x, y, z);
}

/// A teleport waiting for its target column to load.
struct PendingTeleport {
    Vec3          origin;
    DimensionType dim;
};

/// Players waiting for the target column to load (player unique id -> where they came from).
std::unordered_map<std::int64_t, PendingTeleport> gPendingTeleports;
std::uint64_t                                     gWorldGeneration = 0;

constexpr int kTeleportLoadTicks = 20 * 20;

/// Puts a waiting player back where the teleport started (timeout, logout, mod disabled), so
/// nobody is left at the build limit.
void returnToOrigin(Player& player, PendingTeleport const& pending) {
    if (player.getDimensionId() != pending.dim) return;
    player.teleport(pending.origin, pending.dim, Vec2{player.getRotation()});
    player.setFallDistance(0.f);
}

void teleportWhenLoaded(Player& player, int x, int z) {
    auto id  = idOf(player);
    auto dim = player.getDimensionId();
    if (!gPendingTeleports.try_emplace(id, PendingTeleport{player.getFeetPos(), dim}).second) return;
    int holdY = player.getDimensionBlockSource().getMaxHeight();
    // Go there (above any terrain) so the server loads and sends the column, then probe.
    moveTo(player, x, holdY, z);
    log().info("Player {} teleporting to {} {}: loading the target", player.getRealName(), x, z);
    ll::coro::keepThis([id, x, z, holdY, dim, generation = gWorldGeneration]() -> ll::coro::CoroTask<> {
        for (int t = 0; t < kTeleportLoadTicks; ++t) {
            co_await ll::chrono::ticks(1);
            if (generation != gWorldGeneration || !gPendingTeleports.contains(id)) co_return; // world closed, logout
            auto* player = findPlayer(id);
            auto  feet   = player ? player->getFeetPos() : Vec3{};
            // Gone, dead, or moved away by something else (another dimension, a respawn, /tp).
            if (!player || !player->isAlive() || player->getDimensionId() != dim
                || std::abs(feet.x - static_cast<float>(x)) > 4.f || std::abs(feet.z - static_cast<float>(z)) > 4.f) {
                gPendingTeleports.erase(id);
                co_return;
            }
            auto& region = player->getDimensionBlockSource();
            if (columnLoaded(region, x, z)) {
                gPendingTeleports.erase(id);
                finishTeleport(*player, x, z);
                co_return;
            }
            // Keep the player up there while waiting (no fall, no fall damage).
            player->setFallDistance(0.f);
            if (feet.y < static_cast<float>(holdY - 16)) moveTo(*player, x, holdY, z);
        }
        auto it = gPendingTeleports.find(id);
        if (it == gPendingTeleports.end()) co_return;
        auto pending = it->second;
        gPendingTeleports.erase(it);
        if (auto* player = findPlayer(id)) {
            returnToOrigin(*player, pending);
            log().warn("Player {} teleport to {} {} failed: the target did not load", player->getRealName(), x, z);
        }
    }).launch(ll::thread::ServerThreadExecutor::getDefault());
}

/// Ends every waiting teleport, putting the players back (mod disabled).
void cancelPendingTeleports() {
    for (auto const& [id, pending] : gPendingTeleports) {
        if (auto* player = findPlayer(id)) returnToOrigin(*player, pending);
    }
    gPendingTeleports.clear();
}

// ---- durability ---------------------------------------------------------------------------------

/// The compass `damage` alone (docs/SPEC.md 2.3), without parsing the rest of the data: the sync
/// runs often over whole inventories.
int damageOf(ItemStack const& stack) {
    if (!stack.mUserData || !stack.mUserData->contains(compass::kDataKey)) return 0;
    auto const& data = stack.mUserData->at(compass::kDataKey);
    if (!data.hold(Tag::Compound)) return 0;
    auto const& tag = data.get<CompoundTag>();
    if (!tag.contains("damage")) return 0;
    auto const& damage = tag.at("damage");
    return damage.hold(Tag::Int) ? damage.get<IntTag>().data : 0;
}

/// The Bedrock damage value the compass should show.
int barDamageOf(ItemStack const& stack) {
    return core::durabilityBarDamage(damageOf(stack), compass::compassDurability(), kBarMaxDamage);
}

/// `stack` with the right damage value, or nullopt when it already has it.
std::optional<ItemStack> withBar(ItemStack const& stack) {
    if (!compass::isCompass(stack)) return std::nullopt;
    int want = barDamageOf(stack);
    if (stack.getDamageValue() == want) return std::nullopt;
    ItemStack copy = stack;
    copy.setDamageValue(static_cast<short>(want));
    return copy;
}

constexpr int kDurabilitySyncTicks = 10;

} // namespace

// ---- internal API (Actions.h of this unit) ------------------------------------------------------

int safeTeleportY(BlockSource& region, int start, int x, int z) {
    return core::findTeleportY(start, region.getMinHeight(), region.getMaxHeight(), [&](int y) {
        return isFree(region, BlockPos{x, y, z});
    });
}

bool teleportPending(Player const& player) { return gPendingTeleports.contains(idOf(player)); }

bool handleUseEvent(Player& player, bool sneaking) {
    if (!newClick(player)) return false;
    useNextTick(player, sneaking);
    return true;
}

// ---- contract (src/feature/compass/Actions.h) ---------------------------------------------------

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
    log().info("Compass used by {}: Select Biome", player.getRealName());
    gui::openSelectBiome(player);
    return true;
}

void search(Player& player, std::string const& biomeId) { search(player, biomeId, player.getFeetBlockPos()); }

void search(Player& player, std::string const& biomeId, BlockPos const& start) {
    playClick(player);
    auto slot = compass::heldCompass(player);
    if (!slot || compass::isBroken(compass::stackAt(player, *slot))) return;
    if (!search::startSearch(player, *slot, biomeId, start)) return;
    takeXp(player, biomes::xpLevelsFor(biomeId));
}

void searchForNext(Player& player) {
    playClick(player);
    auto slot = compass::heldCompass(player);
    if (!slot || compass::maxNextSearches() <= 0) return;
    auto data = compass::dataAt(player, *slot);
    if (!data || compass::isBroken(*data) || data->state != CompassState::Found || data->prevPos.empty()) return;
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
    if (!data || data->state != CompassState::Found) return;
    if (teleportPending(player)) return;
    auto& region = player.getDimensionBlockSource();
    if (columnLoaded(region, data->foundX, data->foundZ)) {
        finishTeleport(player, data->foundX, data->foundZ);
    } else {
        teleportWhenLoaded(player, data->foundX, data->foundZ);
    }
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

void refreshDurability(Player& player) {
    auto& inv     = player.getInventory();
    bool  changed = false;
    for (int i = 0; i < inv.getContainerSize(); ++i) {
        if (auto fixed = withBar(inv.getItem(i))) {
            inv.setItem(i, *fixed);
            changed = true;
        }
    }
    if (auto fixed = withBar(player.getOffhandSlot())) {
        player.setOffhandSlot(*fixed);
        changed = true;
    }
    if (changed) player.sendInventory(false);
}

/// Item use -> `onUse`, the durability bar sync, teleport bookkeeping.
class ActionsModule {
public:
    void install() {
        auto& bus = ll::event::EventBus::getInstance();
        mUse      = bus.emplaceListener<ll::event::PlayerUseItemEvent>([](ll::event::PlayerUseItemEvent& event) {
            auto& player = event.self();
            if (player.isClientSide() || !compass::isCompass(event.item())) return;
            handleUseEvent(player, player.isSneaking());
        });
        // A click on a block: NC's use is reached unless the block itself reacts (sneaking skips the
        // block, as in vanilla).
        mUseOn =
            bus.emplaceListener<ll::event::PlayerInteractBlockEvent>([](ll::event::PlayerInteractBlockEvent& event) {
                auto& player = event.self();
                if (player.isClientSide() || !compass::isCompass(event.item())) return;
                bool sneaking = player.isSneaking();
                if (!sneaking) {
                    if (auto block = event.block(); block && block->getBlockType().isInteractiveBlock()) {
                        // The block takes this click; its air event (if any) must not open the screen.
                        newClick(player);
                        return;
                    }
                }
                handleUseEvent(player, sneaking);
            });
        mTick  = bus.emplaceListener<ll::event::ServerLevelTickEvent>([this](ll::event::ServerLevelTickEvent& event) {
            if (++mTicks % kDurabilitySyncTicks != 0) return;
            event.level().forEachPlayer([](Player& player) {
                if (!player.isClientSide()) refreshDurability(player);
                return true;
            });
        });
        mLeave = bus.emplaceListener<ll::event::PlayerDisconnectEvent>([](ll::event::PlayerDisconnectEvent& event) {
            Player& player = event.self();
            auto    id     = idOf(player);
            gLastUseTick.erase(id);
            if (auto it = gPendingTeleports.find(id); it != gPendingTeleports.end()) {
                returnToOrigin(player, it->second); // not saved at the build limit
                gPendingTeleports.erase(it);
            }
        });
        static bool worldHooked = false; // world hooks can't be removed: register them once
        if (!worldHooked) {
            worldHooked = true;
            game::db::onWorldClosed([] {
                gLastUseTick.clear();
                gPendingTeleports.clear();
                ++gWorldGeneration;
            });
        }
    }
    void uninstall() {
        auto& bus = ll::event::EventBus::getInstance();
        bus.removeListener(mUse);
        bus.removeListener(mUseOn);
        bus.removeListener(mTick);
        bus.removeListener(mLeave);
        cancelPendingTeleports();
        gLastUseTick.clear();
        ++gWorldGeneration;
    }

private:
    ll::event::ListenerPtr mUse;
    ll::event::ListenerPtr mUseOn;
    ll::event::ListenerPtr mTick;
    ll::event::ListenerPtr mLeave;
    std::uint64_t          mTicks = 0;
};

} // namespace nl::feature::actions

NL_REGISTER_MODULE(nl::feature::actions::ActionsModule);
