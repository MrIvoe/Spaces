#include <functional>
#include "VirtualNavList.h"
#include <algorithm>
#include <cassert>
#include <windowsx.h>
#include "Win32Helpers.h"

namespace {
const wchar_t* kVirtualNavListClass = L"SimpleSpaces_VirtualNavList";
constexpr int kItemHeight = 36;
constexpr int kIconSize = 20;
}

VirtualNavList::VirtualNavList(HWND parent, const ThemePlatform* themePlatform)
    : m_hwnd(nullptr), m_themePlatform(themePlatform) {
    EnsureWindowClass();
    m_hwnd = CreateWindowExW(0, kVirtualNavListClass, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 200, 400, parent, nullptr, GetModuleHandleW(nullptr), this);
    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    UpdateScrollInfo();
}

VirtualNavList::~VirtualNavList() {
    if (m_hwnd) DestroyWindow(m_hwnd);
}

void VirtualNavList::SetItems(const std::vector<Item>& items) {
    m_items = items;
    if (m_items.empty()) {
        m_selectedIndex = 0;
    } else if (m_selectedIndex >= m_items.size()) {
        m_selectedIndex = m_items.size() - 1;
    }
    for (size_t i = 0; i < m_items.size(); ++i) {
        m_items[i].selected = (i == m_selectedIndex);
    }
    m_scrollOffset = 0;
    UpdateScrollInfo();
    Invalidate();
}

void VirtualNavList::SetSelectedIndex(size_t index) {
    if (index < m_items.size()) {
        if (m_selectedIndex == index) {
            EnsureVisible(index);
            return;
        }
        if (m_selectedIndex < m_items.size()) {
            m_items[m_selectedIndex].selected = false;
        }
        m_selectedIndex = index;
        m_items[m_selectedIndex].selected = true;
        EnsureVisible(index);
        Invalidate();
        if (m_onItemClick) {
            m_onItemClick(index);
        }
    }
}

void VirtualNavList::SetOnItemClick(ItemClickCallback cb) {
    m_onItemClick = std::move(cb);
}

void VirtualNavList::Invalidate() {
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

HWND VirtualNavList::GetHwnd() const {
    return m_hwnd;
}

size_t VirtualNavList::GetSelectedIndex() const {
    return m_selectedIndex;
}

void VirtualNavList::EnsureWindowClass() {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = WndProcStatic;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kVirtualNavListClass;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassW(&wc);
        registered = true;
    }
}

LRESULT CALLBACK VirtualNavList::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    VirtualNavList* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<VirtualNavList*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<VirtualNavList*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (pThis) {
        return pThis->WndProc(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT VirtualNavList::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Paint(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int lines = -delta / WHEEL_DELTA * kItemHeight;
        ScrollTo(m_scrollOffset + lines);
        return 0;
    }
    case WM_VSCROLL: {
        int code = LOWORD(wParam);
        int pos = HIWORD(wParam);
        switch (code) {
        case SB_LINEUP:
            ScrollTo(m_scrollOffset - kItemHeight);
            break;
        case SB_LINEDOWN:
            ScrollTo(m_scrollOffset + kItemHeight);
            break;
        case SB_PAGEUP:
            ScrollTo(m_scrollOffset - m_visibleCount * kItemHeight);
            break;
        case SB_PAGEDOWN:
            ScrollTo(m_scrollOffset + m_visibleCount * kItemHeight);
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            ScrollTo(pos);
            break;
        case SB_TOP:
            ScrollTo(0);
            break;
        case SB_BOTTOM:
            ScrollTo(m_maxScroll);
            break;
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int y = GET_Y_LPARAM(lParam);
        int idx = ItemFromPoint(y + m_scrollOffset);
        if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
            SetSelectedIndex(idx);
        }
        SetFocus(hwnd);
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_UP && m_selectedIndex > 0) {
            SetSelectedIndex(m_selectedIndex - 1);
        } else if (wParam == VK_DOWN && m_selectedIndex + 1 < m_items.size()) {
            SetSelectedIndex(m_selectedIndex + 1);
        } else if (wParam == VK_PRIOR) { // Page Up
            SetSelectedIndex(m_selectedIndex >= m_visibleCount ? m_selectedIndex - m_visibleCount : 0);
        } else if (wParam == VK_NEXT) { // Page Down
            SetSelectedIndex(std::min(m_selectedIndex + m_visibleCount, m_items.size() - 1));
        }
        return 0;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        Invalidate();
        return 0;
    case WM_SIZE:
        UpdateScrollInfo();
        Invalidate();
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void VirtualNavList::Paint(HDC hdc) {
    RECT client{};
    GetClientRect(m_hwnd, &client);
    HBRUSH bg = CreateSolidBrush(RGB(245,245,245));
    FillRect(hdc, &client, bg);
    DeleteObject(bg);

    int first = m_scrollOffset / kItemHeight;
    int yOffset = -(m_scrollOffset % kItemHeight);
    int maxVisible = (client.bottom - client.top + kItemHeight - 1) / kItemHeight;
    m_visibleCount = maxVisible;
    for (int i = 0; i < maxVisible && (first + i) < (int)m_items.size(); ++i) {
        int y = yOffset + i * kItemHeight;
        RECT itemRc = {0, y, client.right, y + kItemHeight};
        const auto& item = m_items[first + i];
        COLORREF bgColor = item.selected ? RGB(220,235,255) : RGB(245,245,245);
        HBRUSH itemBg = CreateSolidBrush(bgColor);
        FillRect(hdc, &itemRc, itemBg);
        DeleteObject(itemBg);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, item.enabled ? RGB(30,30,30) : RGB(180,180,180));
        RECT textRc = itemRc;
        textRc.left += 12 + kIconSize;
        DrawTextW(hdc, item.text.c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        // TODO: Draw iconGlyph if present
    }
}

int VirtualNavList::ItemFromPoint(int y) const {
    return y / kItemHeight;
}

void VirtualNavList::UpdateScrollInfo() {
    RECT client{};
    GetClientRect(m_hwnd, &client);
    int visible = (client.bottom - client.top) / kItemHeight;
    m_visibleCount = visible > 0 ? visible : 1;
    int totalHeight = (int)m_items.size() * kItemHeight;
    m_maxScroll = std::max(0, (int)(totalHeight - (client.bottom - client.top)));
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
    si.nMin = 0;
    si.nMax = totalHeight > 0 ? totalHeight - 1 : 0;
    si.nPage = client.bottom - client.top;
    si.nPos = m_scrollOffset;
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
}

void VirtualNavList::ScrollTo(int offset) {
    int newOffset = std::max(0, std::min(offset, m_maxScroll));
    if (newOffset != m_scrollOffset) {
        m_scrollOffset = newOffset;
        UpdateScrollInfo();
        Invalidate();
    }
}

void VirtualNavList::EnsureVisible(size_t index) {
    int itemTop = (int)index * kItemHeight;
    int itemBottom = itemTop + kItemHeight;
    RECT client{};
    GetClientRect(m_hwnd, &client);
    int viewTop = m_scrollOffset;
    int viewBottom = m_scrollOffset + (client.bottom - client.top);
    if (itemTop < viewTop) {
        ScrollTo(itemTop);
    } else if (itemBottom > viewBottom) {
        ScrollTo(itemBottom - (client.bottom - client.top));
    }
}
