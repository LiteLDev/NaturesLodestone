#pragma once

// Item helpers (server thread).

#include <string>
#include <string_view>

class ItemStack;
class ItemStackBase;

namespace nl::game {

/// A new stack of `id` ("minecraft:compass", "natureslodestone:naturescompass", ...). `ItemStack`
/// has no (name, count, aux) constructor, so this is the way to make one. The result is a null
/// stack (`isNull()`) when the id is not registered (e.g. the behavior pack is missing).
[[nodiscard]] ItemStack makeItem(std::string_view id, int count = 1, int aux = 0);

/// The full id of the stack's item ("" for an empty stack).
[[nodiscard]] std::string itemId(ItemStackBase const& stack);

/// True if `stack` is a non-empty stack of `id`.
[[nodiscard]] bool isItem(ItemStackBase const& stack, std::string_view id);

} // namespace nl::game
