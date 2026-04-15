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

COLORREF BlendColor(COLORREF from, COLORREF to, int alpha) {
    alpha = (alpha < 0) ? 0 : ((alpha > 255) ? 255 : alpha);
    const int inv = 255 - alpha;
    const BYTE red = static_cast<BYTE>(((GetRValue(from) * inv) + (GetRValue(to) * alpha)) / 255);
    const BYTE green = static_cast<BYTE>(((GetGValue(from) * inv) + (GetGValue(to) * alpha)) / 255);
    const BYTE blue = static_cast<BYTE>(((GetBValue(from) * inv) + (GetBValue(to) * alpha)) / 255);
    return RGB(red, green, blue);
}

bool IsCyberTheme(COLORREF windowColor) {
    return GetRValue(windowColor) == 9 && GetGValue(windowColor) == 11 && GetBValue(windowColor) == 17;
}
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

void VirtualNavList::SetFonts(HFONT textFont, HFONT iconFont) {
    m_textFont = textFont;
    m_iconFont = iconFont;
    Invalidate();
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
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        if (!m_trackingMouse) {
            TrackMouseEvent(&tme);
            m_trackingMouse = true;
        }

        const int y = GET_Y_LPARAM(lParam);
        const int idx = ItemFromPoint(y + m_scrollOffset);
        const int hover = (idx >= 0 && idx < static_cast<int>(m_items.size())) ? idx : -1;
        if (hover != m_hoverIndex) {
            m_hoverIndex = hover;
            Invalidate();
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        m_trackingMouse = false;
        if (m_hoverIndex != -1) {
            m_hoverIndex = -1;
            Invalidate();
        }
        return 0;
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

    ThemePalette palette;
    if (m_themePlatform) {
        palette = m_themePlatform->BuildPalette();
    }

    const bool cyber = IsCyberTheme(palette.windowColor);
    const bool collapsed = (client.right - client.left) <= 96;

    HBRUSH bg = CreateSolidBrush(palette.navColor);
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
        const bool hovered = (m_hoverIndex == (first + i));

        RECT rowRc = itemRc;
        InflateRect(&rowRc, -4, -3);

        if (item.selected || hovered) {
            const COLORREF rowFill = item.selected
                ? BlendColor(palette.navColor, palette.accentColor, cyber ? 42 : 76)
                : BlendColor(palette.navColor, palette.accentColor, cyber ? 16 : 28);
            HBRUSH itemBg = CreateSolidBrush(rowFill);
            FillRect(hdc, &rowRc, itemBg);
            DeleteObject(itemBg);
        }

        if (item.selected) {
            RECT beamRc = rowRc;
            beamRc.right = beamRc.left + (cyber ? 3 : 4);
            HBRUSH beamBrush = CreateSolidBrush(palette.accentColor);
            FillRect(hdc, &beamRc, beamBrush);
            DeleteObject(beamBrush);
        }

        SetBkMode(hdc, TRANSPARENT);

        RECT iconRc = rowRc;
        iconRc.left += collapsed ? ((rowRc.right - rowRc.left - kIconSize) / 2) : 14;
        iconRc.top += ((rowRc.bottom - rowRc.top - kIconSize) / 2);
        iconRc.right = iconRc.left + kIconSize;
        iconRc.bottom = iconRc.top + kIconSize;

        if (!collapsed) {
            const COLORREF chipFill = item.selected
                ? (cyber ? BlendColor(palette.accentColor, RGB(0, 0, 0), 60)
                         : BlendColor(palette.accentColor, RGB(255, 255, 255), 28))
                : BlendColor(palette.surfaceColor, palette.textColor, hovered ? 30 : 18);
            const COLORREF chipBorder = BlendColor(chipFill, cyber ? palette.accentColor : palette.textColor, cyber ? 90 : 24);
            HPEN chipPen = CreatePen(PS_SOLID, 1, chipBorder);
            HBRUSH chipBrush = CreateSolidBrush(chipFill);
            HGDIOBJ oldPen = SelectObject(hdc, chipPen);
            HGDIOBJ oldBrush = SelectObject(hdc, chipBrush);
            if (cyber) {
                Rectangle(hdc, iconRc.left - 4, iconRc.top - 4, iconRc.right + 4, iconRc.bottom + 4);
            } else {
                RoundRect(hdc, iconRc.left - 4, iconRc.top - 4, iconRc.right + 4, iconRc.bottom + 4, 8, 8);
            }
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(chipBrush);
            DeleteObject(chipPen);
        }

        const COLORREF glyphColor = item.selected
            ? (cyber ? palette.accentColor : RGB(255, 255, 255))
            : BlendColor(palette.textColor, palette.accentColor, hovered ? 42 : 24);
        const COLORREF textColor = item.enabled
            ? (item.selected ? (cyber ? palette.accentColor : RGB(255, 255, 255)) : palette.textColor)
            : BlendColor(palette.textColor, palette.surfaceColor, 120);

        HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, m_iconFont ? m_iconFont : (m_textFont ? m_textFont : GetStockObject(DEFAULT_GUI_FONT))));
        SetTextColor(hdc, glyphColor);
        DrawTextW(hdc, item.iconGlyph.c_str(), -1, &iconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);

        if (!collapsed) {
            RECT textRc = rowRc;
            textRc.left = iconRc.right + 18;
            oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, m_textFont ? m_textFont : GetStockObject(DEFAULT_GUI_FONT)));
            SetTextColor(hdc, textColor);
            DrawTextW(hdc, item.text.c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
            SelectObject(hdc, oldFont);
        }

        if (GetFocus() == m_hwnd && item.selected) {
            RECT focusRc = rowRc;
            InflateRect(&focusRc, -1, -1);
            HPEN focusPen = CreatePen(PS_SOLID, 1, BlendColor(palette.accentColor, RGB(255, 255, 255), cyber ? 20 : 40));
            HGDIOBJ oldFocusPen = SelectObject(hdc, focusPen);
            HGDIOBJ oldFocusBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
            if (cyber) {
                Rectangle(hdc, focusRc.left, focusRc.top, focusRc.right, focusRc.bottom);
            } else {
                RoundRect(hdc, focusRc.left, focusRc.top, focusRc.right, focusRc.bottom, 8, 8);
            }
            SelectObject(hdc, oldFocusBrush);
            SelectObject(hdc, oldFocusPen);
            DeleteObject(focusPen);
        }
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
