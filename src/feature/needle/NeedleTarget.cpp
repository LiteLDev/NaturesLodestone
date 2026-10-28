#include "feature/needle/NeedleTarget.h"

#include "mc/deps/nbt/ByteTag.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/CompoundTagVariant.h"
#include "mc/deps/nbt/Int64Tag.h"
#include "mc/deps/nbt/IntTag.h"
#include "mc/world/item/ItemStackBase.h"

#include "feature/compass/CompassData.h"

namespace nl::feature::needle {

namespace {

CompoundTagVariant const* find(CompoundTag const& tag, std::string_view key) {
    auto it = tag.mTags.find(key);
    return it == tag.mTags.end() ? nullptr : &it->second;
}

// The same tag types CompassData reads (Int, also Byte / Int64).
int intOf(CompoundTag const& tag, std::string_view key) {
    auto const* v = find(tag, key);
    if (!v) return 0;
    if (v->hold(Tag::Int)) return v->get<IntTag>().data;
    if (v->hold(Tag::Byte)) return v->get<ByteTag>().data;
    if (v->hold(Tag::Int64)) return static_cast<int>(v->get<Int64Tag>().data);
    return 0;
}

} // namespace

NeedleTarget needleTarget(ItemStackBase const& stack) {
    if (!stack.mUserData) return {};
    auto const* data = find(*stack.mUserData, compass::kDataKey);
    if (!data || !data->hold(Tag::Compound)) return {};
    auto const& tag = data->get<CompoundTag>();
    if (intOf(tag, "compass_state") != static_cast<int>(compass::CompassState::Found)) return {};
    return {true, intOf(tag, "found_x"), intOf(tag, "found_z")};
}

} // namespace nl::feature::needle
