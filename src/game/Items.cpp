#include "game/Items.h"

#include "mc/world/item/ItemStack.h"

namespace nl::game {

ItemStack makeItem(std::string_view id, int count, int aux) {
    ItemStack stack;
    stack.reinit(id, count, aux);
    return stack;
}

std::string itemId(ItemStackBase const& stack) {
    if (stack.isNull()) return {};
    return stack.getTypeName();
}

bool isItem(ItemStackBase const& stack, std::string_view id) { return !stack.isNull() && stack.getTypeName() == id; }

} // namespace nl::game
