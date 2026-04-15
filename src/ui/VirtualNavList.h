
#pragma once
#define NOMINMAX
#include <functional>
#include <vector>
#include <string>
#include <windows.h>
#include "core/ThemePlatform.h"

// VirtualNavList: Paint-only, theme-aware, no HWNDs per item.
class VirtualNavList {
public:
    struct Item {
        std::wstring text;
        std::wstring iconGlyph;
        bool selected = false;
        bool enabled = true;
    };

    using ItemClickCallback = std::function<void(size_t)>;

    VirtualNavList(HWND parent, const ThemePlatform* themePlatform);
    ~VirtualNavList();

    void SetItems(const std::vector<Item>& items);
    void SetSelectedIndex(size_t index);
    void SetOnItemClick(ItemClickCallback cb);
    void SetFonts(HFONT textFont, HFONT iconFont);
    void Invalidate();
    HWND GetHwnd() const;
    size_t GetSelectedIndex() const;

private:
    HWND m_hwnd;
    std::vector<Item> m_items;
    size_t m_selectedIndex = 0;
    ItemClickCallback m_onItemClick;
    const ThemePlatform* m_themePlatform;
    HFONT m_textFont = nullptr;
    HFONT m_iconFont = nullptr;
    int m_hoverIndex = -1;
    bool m_trackingMouse = false;

    // Virtualization state
    int m_scrollOffset = 0; // in pixels
    int m_maxScroll = 0;
    int m_visibleCount = 0;
    void UpdateScrollInfo();
    void ScrollTo(int offset);
    void EnsureVisible(size_t index);

    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void Paint(HDC hdc);
    int ItemFromPoint(int y) const;
    void EnsureWindowClass();
};
