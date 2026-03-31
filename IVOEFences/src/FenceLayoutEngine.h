#pragma once

#include <windows.h>

#include <cstddef>
#include <vector>

class FenceLayoutEngine {
public:
    struct LayoutConfig {
        int topPadding{40};
        int leftPadding{10};
        int rightPadding{10};
        int bottomPadding{10};
        int lineHeight{18};
    };

    struct ItemSlot {
        size_t index{};
        RECT bounds{};
    };

    std::vector<ItemSlot> BuildListLayout(const RECT& clientRect, size_t itemCount, const LayoutConfig& config = LayoutConfig{}) const;
    int HitTest(const std::vector<ItemSlot>& slots, POINT clientPoint) const;
};