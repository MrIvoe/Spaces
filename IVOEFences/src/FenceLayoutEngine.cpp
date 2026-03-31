#include "FenceLayoutEngine.h"

#include <algorithm>

std::vector<FenceLayoutEngine::ItemSlot> FenceLayoutEngine::BuildListLayout(
    const RECT& clientRect,
    size_t itemCount,
    const LayoutConfig& config) const {

    std::vector<ItemSlot> slots;
    slots.reserve(itemCount);

    const int left = static_cast<int>(clientRect.left) + config.leftPadding;
    const int rightEdge = static_cast<int>(clientRect.right) - config.rightPadding;
    const int right = std::max(left, rightEdge);
    const int maxBottom = static_cast<int>(clientRect.bottom) - config.bottomPadding;

    int y = static_cast<int>(clientRect.top) + config.topPadding;
    for (size_t i = 0; i < itemCount; ++i) {
        if (y + config.lineHeight > maxBottom) {
            break;
        }

        ItemSlot slot;
        slot.index = i;
        slot.bounds = RECT{left, y, right, y + config.lineHeight};
        slots.push_back(slot);
        y += config.lineHeight;
    }

    return slots;
}

int FenceLayoutEngine::HitTest(const std::vector<ItemSlot>& slots, POINT clientPoint) const {
    for (const ItemSlot& slot : slots) {
        if (PtInRect(&slot.bounds, clientPoint)) {
            return static_cast<int>(slot.index);
        }
    }

    return -1;
}