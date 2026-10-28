// Animated Nature's Compass needle (docs/SPEC.md 3.8), client build only.
//
// Bedrock asks an item for an animation frame (`Item::getAnimationFrameFor(holder, ...)`) and then
// for the icon of that frame (`Item::getIconInfo(stack, frame, ...)`). For the Nature's Compass we
// answer the first with the needle frame of NC's compass logic (core/CompassAngle.h) and the second
// with entry `1 + frame` of the `naturescompass` texture array (item_texture.d/needle.json; entry 0
// is the plain icon vanilla clients show).
//
// getIconInfo: a detour of `ComponentItem::$getIconInfo`, passing every other item through.
// getAnimationFrameFor: `ComponentItem::$getAnimationFrameFor` is identical-code-folded with many
// unrelated functions (a detour there is also reached with other signatures, even with our item as
// `this`), so it can't be detoured. Instead our Item object gets a private copy of its vtable with
// that one slot replaced; every other item keeps the game's vtable. getIconInfo is where we first
// see our Item (one per item registry: the client's, and the integrated server's in local worlds).

#ifdef NL_CLIENT

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/memory/Memory.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/legacy/ActorRuntimeID.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/item/CompassSpriteCalculator.h"
#include "mc/world/item/ItemIconInfoType.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/ResolvedItemIconInfo.h"
#include "mc/world/item/components/ComponentItem.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/Tick.h"

#include "client/Client.h"
#include "client/needle/Needle.h"
#include "core/CompassAngle.h"
#include "feature/compass/CompassData.h"
#include "feature/needle/NeedleTarget.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace nl::client::needle {

namespace {

constexpr std::uint64_t kItemHash = HashedString::computeHash(feature::compass::kItemId);

bool isNeedleItem(Item const& item) { return item.mFullName->getHash() == kItemHash; }

std::mutex   gMutex;
core::Needle gNeedle; // shared by every Nature's Compass, like the Java item property function
std::mt19937 gRandom{std::random_device{}()};

std::int64_t currentTick() {
    if (auto* client = currentClient()) {
        if (auto* level = client->getLevel()) return static_cast<std::int64_t>(level->getCurrentTick().tickID);
    }
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count() / 50;
}

} // namespace

int frameFor(Mob* holder, bool asItemEntity, ItemStack const* stack) {
    core::NeedleView view;
    view.tick = currentTick();
    if (holder && !asItemEntity) {
        view.hasHolder     = true;
        view.isLocalPlayer = static_cast<Actor*>(holder) == static_cast<Actor*>(currentPlayer());
        auto pos           = holder->getFeetPos();
        view.x             = pos.x;
        view.y             = pos.y;
        view.z             = pos.z;
        view.yaw           = holder->getRotation().y;
        view.seed          = static_cast<int>(holder->getRuntimeID().rawID);
        auto target        = stack ? feature::needle::needleTarget(*stack) : feature::needle::NeedleTarget{};
        if (target.found) {
            // The found x/z in the holder's current dimension (NC stores no dimension).
            view.targetX = target.x;
            view.targetY = 0;
            view.targetZ = target.z;
        } else {
            // The world spawn, as a vanilla compass sees it (overworld only).
            auto* client = currentClient();
            auto* region = client ? client->getRegion() : nullptr;
            if (!region) {
                view.hasHolder = false; // no world to point in: spin like a compass without holder
                view.seed      = 0;
            } else {
                auto spawn         = CompassSpriteCalculator::pointTowardsSpawnPoint(region, holder);
                view.targetX       = spawn.x;
                view.targetY       = spawn.y;
                view.targetZ       = spawn.z;
                view.sameDimension = CompassSpriteCalculator::isInOverworldDimension(region, holder);
            }
        }
    }
    std::lock_guard lock{gMutex};
    double random01 =
        gNeedle.spinNeedsRandom(view.tick) ? std::uniform_real_distribution<double>{0.0, 1.0}(gRandom) : 0.0;
    return gNeedle.frame(view, random01);
}

int iconIndexFor(int frame) {
    if (frame < 0 || frame >= core::kNeedleFrames) frame = core::kNeedleFrames / 2;
    return 1 + frame;
}

std::string_view iconName() { return "naturescompass"; }

namespace {

// ---- private vtable of our Item -------------------------------------------------------------------

// Entries copied from the game's vtable: Item and ComponentItem have about 150 virtual functions.
constexpr std::size_t kVtableEntries = 224;

struct PatchedVtable {
    void**                                original = nullptr;
    std::array<void*, kVtableEntries + 1> slots{}; // [0] = the RTTI locator before the vtable
    [[nodiscard]] void**                  vptr() { return slots.data() + 1; }
};

std::mutex                                  gVtableMutex;
std::vector<std::unique_ptr<PatchedVtable>> gTables;  // kept until uninstall
std::vector<Item const*>                    gPatched; // our Items using a patched vtable (current level)
std::atomic<void**>                         gLastPatchedVptr{nullptr};

void**& vptrOf(Item const* item) { return *reinterpret_cast<void***>(const_cast<Item*>(item)); }

// Replaces `Item::getAnimationFrameFor` (same x64 calling convention as the member function).
int animationFrameSlot(Item const*, Mob* holder, bool asItemEntity, ItemStack const* item, bool) {
    return frameFor(holder, asItemEntity, item);
}

std::optional<unsigned> frameSlotIndex() {
    static std::optional<unsigned> index = [] {
        auto i = ll::memory::getVtableIndex(&Item::getAnimationFrameFor);
        if (!i || *i >= kVtableEntries) {
            nl::log().error("needle: Item::getAnimationFrameFor vtable slot not found; the needle will not move");
            return std::optional<unsigned>{};
        }
        return std::optional<unsigned>{*i};
    }();
    return index;
}

void patchItem(Item const* item) {
    if (vptrOf(item) == gLastPatchedVptr.load(std::memory_order_relaxed)) return;
    auto slot = frameSlotIndex();
    if (!slot) return;
    std::lock_guard lock{gVtableMutex};
    void** current = vptrOf(item);
    for (auto const& t : gTables) {
        if (current == t->vptr()) return; // already ours
    }
    PatchedVtable* table = nullptr;
    for (auto const& t : gTables) {
        if (t->original == current) table = t.get();
    }
    if (!table) {
        auto t      = std::make_unique<PatchedVtable>();
        t->original = current;
        for (std::size_t i = 0; i <= kVtableEntries; ++i) t->slots[i] = current[static_cast<std::ptrdiff_t>(i) - 1];
        t->vptr()[*slot] = reinterpret_cast<void*>(&animationFrameSlot);
        table            = t.get();
        gTables.push_back(std::move(t));
    }
    vptrOf(item) = table->vptr();
    gPatched.push_back(item);
    gLastPatchedVptr.store(table->vptr(), std::memory_order_relaxed);
}

// Items die with the level (their vtable pointer goes with them): just forget them.
void forgetItems() {
    {
        std::lock_guard lock{gVtableMutex};
        gPatched.clear();
    }
    std::lock_guard lock{gMutex};
    gNeedle.reset();
}

// Uninstall: give the live items their game vtable back.
void restoreItems() {
    std::lock_guard lock{gVtableMutex};
    for (auto const* item : gPatched) {
        for (auto const& t : gTables) {
            if (vptrOf(item) == t->vptr()) vptrOf(item) = t->original;
        }
    }
    gPatched.clear();
    gLastPatchedVptr.store(nullptr);
}

LL_TYPE_INSTANCE_HOOK(
    NeedleIconHook,
    ll::memory::HookPriority::Normal,
    ComponentItem,
    &ComponentItem::$getIconInfo,
    ResolvedItemIconInfo,
    ItemStackBase const& item,
    int                  newAnimationFrame,
    bool                 inInventoryPane
) {
    if (!isNeedleItem(*this)) return origin(item, newAnimationFrame, inInventoryPane);
    patchItem(this);
    return ResolvedItemIconInfo(std::string{iconName()}, iconIndexFor(newAnimationFrame), ItemIconInfoType::Icon);
}

class NeedleModule {
public:
    void install() {
        NeedleIconHook::hook();
        mExitListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::client::ClientExitLevelEvent>(
            [](auto&) { forgetItems(); }
        );
    }
    void uninstall() {
        ll::event::EventBus::getInstance().removeListener(mExitListener);
        NeedleIconHook::unhook();
        restoreItems();
        forgetItems();
    }

private:
    ll::event::ListenerPtr mExitListener;
};

} // namespace

} // namespace nl::client::needle

NL_REGISTER_MODULE(nl::client::needle::NeedleModule);

#endif // NL_CLIENT
