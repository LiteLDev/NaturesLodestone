#include "feature/compass/CompassData.h"

#include <random>

#include "mc/deps/nbt/ByteTag.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/CompoundTagVariant.h"
#include "mc/deps/nbt/Int64Tag.h"
#include "mc/deps/nbt/IntTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/deps/nbt/StringTag.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"

#include "feature/compass/CompassConfig.h"
#include "game/Items.h"

namespace nl::feature::compass {

// ---- CompassData ----------------------------------------------------------------------------------

void CompassData::clearSearchData() {
    auto keepDamage = damage;
    auto keepUid    = uid;
    *this           = CompassData{};
    damage          = keepDamage;
    uid             = keepUid;
}

void CompassData::startSearch(std::string biome) {
    clearSearchData();
    state        = CompassState::Searching;
    biomeId      = std::move(biome);
    samples      = 0;
    searchRadius = 0;
}

void CompassData::succeed(
    std::string          biome,
    int                  x,
    int                  z,
    std::vector<PrevPos> positions,
    int                  sampleCount,
    bool                 showCoords
) {
    clearSearchData();
    state         = CompassState::Found;
    biomeId       = std::move(biome);
    foundX        = x;
    foundZ        = z;
    prevPos       = std::move(positions);
    samples       = sampleCount;
    displayCoords = showCoords;
}

void CompassData::fail(std::string biome, int radius, int sampleCount) {
    clearSearchData();
    state        = CompassState::NotFound;
    biomeId      = std::move(biome);
    searchRadius = floorRadius(radius);
    samples      = sampleCount;
}

std::int64_t CompassData::ensureUid() {
    if (uid == 0) uid = newUid();
    return uid;
}

std::int64_t newUid() {
    static std::mt19937_64 rng{std::random_device{}()};
    std::int64_t           id = 0;
    while (id == 0) id = static_cast<std::int64_t>(rng());
    return id;
}

// ---- NBT ------------------------------------------------------------------------------------------

namespace {

int intOf(CompoundTag const& c, std::string_view key, int def = 0) {
    if (!c.contains(key)) return def;
    auto const& v = c.at(key);
    if (v.hold(Tag::Int)) return v.get<IntTag>().data;
    if (v.hold(Tag::Byte)) return v.get<ByteTag>().data;
    if (v.hold(Tag::Int64)) return static_cast<int>(v.get<Int64Tag>().data);
    return def;
}

std::int64_t int64Of(CompoundTag const& c, std::string_view key) {
    if (!c.contains(key)) return 0;
    auto const& v = c.at(key);
    if (v.hold(Tag::Int64)) return v.get<Int64Tag>().data;
    if (v.hold(Tag::Int)) return v.get<IntTag>().data;
    return 0;
}

std::string stringOf(CompoundTag const& c, std::string_view key) {
    if (!c.contains(key)) return {};
    auto const& v = c.at(key);
    if (!v.hold(Tag::String)) return {};
    return static_cast<std::string const&>(v.get<StringTag>());
}

CompoundTag const* dataTag(ItemStackBase const& stack) {
    if (!stack.mUserData) return nullptr;
    auto const& root = *stack.mUserData;
    if (!root.contains(kDataKey)) return nullptr;
    auto const& v = root.at(kDataKey);
    if (!v.hold(Tag::Compound)) return nullptr;
    return &v.get<CompoundTag>();
}

CompoundTag toTag(CompassData const& d) {
    CompoundTag c;
    c["compass_state"]  = IntTag{static_cast<int>(d.state)};
    c["biome_id"]       = StringTag{d.biomeId};
    c["found_x"]        = IntTag{d.foundX};
    c["found_z"]        = IntTag{d.foundZ};
    c["search_radius"]  = IntTag{d.searchRadius};
    c["samples"]        = IntTag{d.samples};
    c["display_coords"] = ByteTag{d.displayCoords ? 1 : 0};
    c["damage"]         = IntTag{d.damage};
    c["uid"]            = Int64Tag{d.uid};
    ListTag list;
    for (auto const& p : d.prevPos) {
        CompoundTag pos;
        pos["x"] = IntTag{p.x};
        pos["y"] = IntTag{p.y};
        pos["z"] = IntTag{p.z};
        list.add(pos.copy());
    }
    c["prev_pos"] = std::move(list);
    return c;
}

} // namespace

bool isCompass(ItemStackBase const& stack) { return game::isItem(stack, kItemId); }

ItemStack makeCompass() { return game::makeItem(kItemId); }

CompassData readData(ItemStackBase const& stack) {
    CompassData d;
    if (!isCompass(stack)) return d;
    auto const* c = dataTag(stack);
    if (!c) return d;
    int state       = intOf(*c, "compass_state");
    d.state         = (state >= 0 && state <= 3) ? static_cast<CompassState>(state) : CompassState::Inactive;
    d.biomeId       = stringOf(*c, "biome_id");
    d.foundX        = intOf(*c, "found_x");
    d.foundZ        = intOf(*c, "found_z");
    d.searchRadius  = intOf(*c, "search_radius");
    d.samples       = intOf(*c, "samples");
    d.displayCoords = intOf(*c, "display_coords") != 0;
    d.damage        = intOf(*c, "damage");
    d.uid           = int64Of(*c, "uid");
    if (c->contains("prev_pos")) {
        auto const& v = c->at("prev_pos");
        if (v.hold(Tag::List)) {
            for (auto const& e : v.get<ListTag>()) {
                Tag const* t = e.get();
                if (!t || t->getId() != Tag::Compound) continue;
                auto const& pos = static_cast<CompoundTag const&>(*t);
                d.prevPos.push_back({intOf(pos, "x"), intOf(pos, "y"), intOf(pos, "z")});
            }
        }
    }
    return d;
}

void writeData(ItemStackBase& stack, CompassData const& data) {
    auto tag = stack.mUserData ? stack.mUserData->clone() : std::make_unique<CompoundTag>();
    if (data == CompassData{}) {
        tag->remove(kDataKey);
    } else {
        (*tag)[kDataKey] = toTag(data);
    }
    if (tag->empty()) stack.setUserData(nullptr);
    else stack.setUserData(std::move(tag));
}

bool isBroken(CompassData const& data) {
    int durability = compassDurability();
    return durability > 0 && data.damage >= durability;
}

bool isBroken(ItemStackBase const& stack) { return isCompass(stack) && isBroken(readData(stack)); }

// ---- players --------------------------------------------------------------------------------------

std::optional<CompassSlot> heldCompass(Player const& player) {
    if (isCompass(player.getSelectedItem())) return CompassSlot{Hand::Main, player.getSelectedItemSlot()};
    if (isCompass(player.getOffhandSlot())) return CompassSlot{Hand::Off, -1};
    return std::nullopt;
}

std::optional<CompassSlot> findCompass(Player const& player, std::int64_t uid) {
    if (uid == 0) return std::nullopt;
    auto matches  = [uid](ItemStack const& s) { return isCompass(s) && readData(s).uid == uid; };
    int  selected = player.getSelectedItemSlot();
    if (matches(player.getSelectedItem())) return CompassSlot{Hand::Main, selected};
    if (matches(player.getOffhandSlot())) return CompassSlot{Hand::Off, -1};
    auto& inv = player.getInventory();
    for (int i = 0; i < inv.getContainerSize(); ++i) {
        if (i != selected && matches(inv.getItem(i))) return CompassSlot{Hand::Main, i};
    }
    return std::nullopt;
}

ItemStack const& stackAt(Player const& player, CompassSlot slot) {
    if (slot.hand == Hand::Off) return player.getOffhandSlot();
    auto& inv = player.getInventory();
    if (slot.slot < 0 || slot.slot >= inv.getContainerSize()) return ItemStack::EMPTY_ITEM();
    return inv.getItem(slot.slot);
}

std::optional<CompassData> dataAt(Player const& player, CompassSlot slot) {
    auto const& stack = stackAt(player, slot);
    if (!isCompass(stack)) return std::nullopt;
    return readData(stack);
}

bool storeAt(Player& player, CompassSlot slot, CompassData const& data) {
    auto const& current = stackAt(player, slot);
    if (!isCompass(current)) return false;
    ItemStack copy = current;
    writeData(copy, data);
    if (slot.hand == Hand::Off) {
        player.setOffhandSlot(copy);
    } else {
        player.getInventory().setItem(slot.slot, copy);
    }
    player.sendInventory(false);
    return true;
}

} // namespace nl::feature::compass
