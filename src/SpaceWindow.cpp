#include "SpaceWindow.h"
#include "SpaceManager.h"
#include "AppResources.h"
#include "Win32Helpers.h"
#include "core/ThemePlatform.h"
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>
#include <algorithm>
#include <atomic>
#include <cwctype>
#include <sstream>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Shell32.lib")

static constexpr const wchar_t* kSpaceWindowClass = L"SimpleSpaces_SpaceWindow";
static constexpr const wchar_t* kSpaceSettingsClass = L"SimpleSpaces_SpaceSettingsWindow";
static constexpr int kTitleBarHeight = 28;
static constexpr int kBorderSize = 1;
static constexpr int kIconGridSize = 32;
static constexpr UINT_PTR kIdleStateTimerId = 1;
static constexpr UINT kIdleStateTimerMs = 100;

static constexpr int kCmdSpaceSettings = 1007;
static constexpr int kCtlInheritThemePolicy = 5001;
static constexpr int kCtlTextOnly = 5002;
static constexpr int kCtlRollup = 5003;
static constexpr int kCtlTransparent = 5004;
static constexpr int kCtlLabelsOnHover = 5005;
static constexpr int kCtlSpacingPreset = 5006;
static constexpr int kCtlClose = 5007;
static constexpr int kCtlApplyAll = 5008;

namespace
{
    std::atomic<unsigned long long> gSpaceEventSequence{1};

    bool IsPopupInteractionActive()
    {
        GUITHREADINFO info{};
        info.cbSize = sizeof(info);
        if (!GetGUIThreadInfo(0, &info))
        {
            return false;
        }

        const DWORD popupFlags = GUI_INMENUMODE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE;
        return (info.flags & popupFlags) != 0;
    }

    COLORREF BlendColor(COLORREF from, COLORREF to, int alpha)
    {
        alpha = (alpha < 0) ? 0 : ((alpha > 255) ? 255 : alpha);
        const int inv = 255 - alpha;
        const BYTE red = static_cast<BYTE>(((GetRValue(from) * inv) + (GetRValue(to) * alpha)) / 255);
        const BYTE green = static_cast<BYTE>(((GetGValue(from) * inv) + (GetGValue(to) * alpha)) / 255);
        const BYTE blue = static_cast<BYTE>(((GetBValue(from) * inv) + (GetBValue(to) * alpha)) / 255);
        return RGB(red, green, blue);
    }

    bool IsSettingsToggleControlId(int controlId)
    {
        switch (controlId)
        {
        case kCtlInheritThemePolicy:
        case kCtlTextOnly:
        case kCtlRollup:
        case kCtlTransparent:
        case kCtlLabelsOnHover:
            return true;
        default:
            return false;
        }
    }

    void DrawSettingsToggleControl(const DRAWITEMSTRUCT* drawInfo, COLORREF backgroundColor, COLORREF accentColor)
    {
        if (!drawInfo || !drawInfo->hwndItem)
        {
            return;
        }

        HDC hdc = drawInfo->hDC;
        RECT rc = drawInfo->rcItem;
        const bool checked = (SendMessageW(drawInfo->hwndItem, BM_GETCHECK, 0, 0) == BST_CHECKED);
        const bool focused = (drawInfo->itemState & ODS_FOCUS) != 0;
        const bool pressed = (drawInfo->itemState & ODS_SELECTED) != 0;
        const bool disabled = (drawInfo->itemState & ODS_DISABLED) != 0;

        HBRUSH bgBrush = CreateSolidBrush(backgroundColor);
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);

        wchar_t label[256]{};
        GetWindowTextW(drawInfo->hwndItem, label, static_cast<int>(std::size(label)));

        RECT textRc = rc;
        textRc.right -= 74;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, disabled ? RGB(145, 145, 145) : RGB(30, 30, 30));
        DrawTextW(hdc, label, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT track{};
        const int trackWidth = 50;
        const int trackHeight = 28;
        track.right = rc.right - 10;
        track.left = track.right - trackWidth;
        track.top = rc.top + ((rc.bottom - rc.top - trackHeight) / 2);
        track.bottom = track.top + trackHeight;

        COLORREF trackColor = checked ? accentColor : RGB(210, 214, 220);
        if (pressed)
        {
            trackColor = checked ? RGB(80, 95, 230) : RGB(196, 202, 210);
        }
        if (disabled)
        {
            trackColor = RGB(224, 224, 224);
        }

        HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(180, 186, 196));
        HBRUSH trackBrush = CreateSolidBrush(trackColor);
        HGDIOBJ oldPen = SelectObject(hdc, borderPen);
        HGDIOBJ oldBrush = SelectObject(hdc, trackBrush);
        RoundRect(hdc, track.left, track.top, track.right, track.bottom, trackHeight, trackHeight);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(trackBrush);
        DeleteObject(borderPen);

        RECT knob{};
        const int knobSize = 22;
        knob.left = checked ? (track.right - knobSize - 3) : (track.left + 3);
        if (pressed)
        {
            knob.left += checked ? -1 : 1;
        }
        knob.top = track.top + 3;
        knob.right = knob.left + knobSize;
        knob.bottom = knob.top + knobSize;

        HBRUSH knobBrush = CreateSolidBrush(disabled ? RGB(245, 245, 245) : RGB(255, 255, 255));
        HPEN knobPen = CreatePen(PS_SOLID, 1, RGB(188, 188, 188));
        oldPen = SelectObject(hdc, knobPen);
        oldBrush = SelectObject(hdc, knobBrush);
        Ellipse(hdc, knob.left, knob.top, knob.right, knob.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(knobBrush);
        DeleteObject(knobPen);

        if (focused)
        {
            RECT focusRc = rc;
            focusRc.left = textRc.right + 4;
            InflateRect(&focusRc, -2, -2);
            DrawFocusRect(hdc, &focusRc);
        }
    }

    std::wstring BoolText(bool value)
    {
        return value ? L"true" : L"false";
    }

    std::wstring CorrelationPrefix(const std::wstring& correlationId)
    {
        return correlationId.empty() ? L"" : (L"[cid=" + correlationId + L"] ");
    }

    int TileSizeFromPreset(const std::wstring& preset)
    {
        if (preset == L"compact")
        {
            return 48;
        }
        if (preset == L"spacious")
        {
            return 68;
        }
        return 56;
    }

    std::wstring TrimWhitespace(const std::wstring& input)
    {
        size_t start = 0;
        while (start < input.size() && iswspace(input[start]))
        {
            ++start;
        }

        size_t end = input.size();
        while (end > start && iswspace(input[end - 1]))
        {
            --end;
        }

        return input.substr(start, end - start);
    }
}

SpaceWindow::SpaceWindow(SpaceManager* manager, const SpaceModel& model, const ThemePlatform* themePlatform)
    : m_manager(manager), m_model(model), m_themePlatform(themePlatform)
{
}

SpaceWindow::~SpaceWindow()
{
    Destroy();
}

bool SpaceWindow::Create(HWND parent)
{
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = SpaceWindow::WndProcStatic;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kSpaceWindowClass;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_SPACES_APP));
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.style = CS_VREDRAW | CS_HREDRAW;

        if (!RegisterClassW(&wc))
            return false;

        registered = true;
    }

    int width = m_model.width;
    int height = m_model.height;

    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kSpaceWindowClass,
        m_model.title.c_str(),
        WS_POPUP | WS_THICKFRAME | WS_VISIBLE,
        m_model.x,
        m_model.y,
        width,
        height,
        parent,
        nullptr,
        GetModuleHandleW(nullptr),
        this);

    if (!m_hwnd)
        return false;

    const HINSTANCE hInstance = GetModuleHandleW(nullptr);
    const HICON bigIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_SPACES_APP));
    const HICON smallIcon = static_cast<HICON>(LoadImageW(
        hInstance,
        MAKEINTRESOURCEW(IDI_SPACES_APP),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR | LR_SHARED));
    if (bigIcon)
    {
        SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
    }
    if (smallIcon)
    {
        SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
    }

    m_expandedHeight = height;
    SetTimer(m_hwnd, kIdleStateTimerId, kIdleStateTimerMs, nullptr);

    // Seed hover state from real cursor position to prevent immediate roll-up
    // when a space is created under the mouse.
    POINT cursor{};
    RECT winRect{};
    if (GetCursorPos(&cursor) && GetWindowRect(m_hwnd, &winRect))
    {
        m_mouseInside = PtInRect(&winRect, cursor) != FALSE;
    }

    DragAcceptFiles(m_hwnd, TRUE);
    InitializeImageList();
    const std::wstring createCid = NewCorrelationId(L"create");
    TraceDebug(L"Create window: pos=(" + std::to_wstring(m_model.x) +
               L"," + std::to_wstring(m_model.y) + L") size=(" +
               std::to_wstring(width) + L"x" + std::to_wstring(height) +
               L") hover=" + BoolText(m_mouseInside),
               createCid);
    ApplyIdleVisualState();

    return true;
}

void SpaceWindow::Show()
{
    if (m_hwnd)
    {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
    }
}

void SpaceWindow::Destroy()
{
    if (m_hwnd)
    {
        const std::wstring destroyCid = NewCorrelationId(L"destroy");
        TraceDebug(L"Destroy window", destroyCid);
        KillTimer(m_hwnd, kIdleStateTimerId);
        DragAcceptFiles(m_hwnd, FALSE);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void SpaceWindow::UpdateModel(const SpaceModel& model)
{
    const bool previouslyRolledUp = m_isRolledUp;
    m_model = model;
    if (!previouslyRolledUp && m_hwnd)
    {
        RECT rc{};
        GetWindowRect(m_hwnd, &rc);
        m_expandedHeight = rc.bottom - rc.top;
    }
    if (m_hwnd)
    {
        SetWindowTextW(m_hwnd, m_model.title.c_str());
        ApplyIdleVisualState();
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void SpaceWindow::SetItems(const std::vector<SpaceItem>& items)
{
    m_items = items;
    if (m_hwnd)
    {
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

HWND SpaceWindow::GetHwnd() const
{
    return m_hwnd;
}

const std::wstring& SpaceWindow::GetSpaceId() const
{
    return m_model.id;
}

const SpaceModel& SpaceWindow::GetModel() const
{
    return m_model;
}

LRESULT CALLBACK SpaceWindow::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    SpaceWindow* pThis = nullptr;

    if (msg == WM_NCCREATE)
    {
        auto pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<SpaceWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    }
    else
    {
        pThis = reinterpret_cast<SpaceWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis)
        return pThis->WndProc(hwnd, msg, wParam, lParam);

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT SpaceWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == ThemePlatform::GetThemeChangedMessageId())
    {
        ApplyIdleVisualState();
        InvalidateRect(m_hwnd, nullptr, TRUE);
        UpdateWindow(m_hwnd);
        return 0;
    }

    switch (msg)
    {
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        ApplyIdleVisualState();
        InvalidateRect(m_hwnd, nullptr, TRUE);
        UpdateWindow(m_hwnd);
        return 0;

    case WM_PAINT:
        OnPaint();
        return 0;

    case WM_LBUTTONDOWN:
        OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSEMOVE:
        OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_LBUTTONUP:
        OnLButtonUp();
        return 0;

    case WM_LBUTTONDBLCLK:
        OnLButtonDblClk(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSELEAVE:
        m_activeCorrelationId = NewCorrelationId(L"hover");
        m_mouseInside = false;
        m_selectedItem = -1;
        TraceDebug(L"Mouse leave", m_activeCorrelationId);
        ApplyIdleVisualState();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;

    case WM_TIMER:
        if (wParam == kIdleStateTimerId)
        {
            if (IsPopupInteractionActive())
            {
                return 0;
            }

            ApplyIdleVisualState();
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return 0;
        }
        break;

    case WM_CONTEXTMENU:
    {
        POINT screenPt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        const int titleBarHeight = m_themePlatform ? m_themePlatform->GetFenceTitleBarHeightPx() : kTitleBarHeight;
        if (screenPt.x == -1 && screenPt.y == -1)
        {
            RECT clientRc{};
            GetClientRect(m_hwnd, &clientRc);

            POINT anchor{};
            if (m_selectedItem >= 0)
            {
                static constexpr int kItemHeight = 24;
                anchor.x = 20;
                anchor.y = titleBarHeight + 8 + (m_selectedItem * kItemHeight) + (kItemHeight / 2);
            }
            else
            {
                anchor.x = (clientRc.right - clientRc.left) / 2;
                anchor.y = titleBarHeight / 2;
            }

            ClientToScreen(m_hwnd, &anchor);
            screenPt = anchor;
        }

        POINT clientPt = screenPt;
        ScreenToClient(m_hwnd, &clientPt);
        OnContextMenu(clientPt.x, clientPt.y);
        return 0;
    }

    case WM_MEASUREITEM:
    {
        auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (measure)
        {
            const auto it = m_menuVisuals.find(measure->itemID);
            if (it != m_menuVisuals.end())
            {
                Win32Helpers::MeasureThemedPopupMenuItem(measure, it->second);
                return TRUE;
            }
        }
        break;
    }

    case WM_DRAWITEM:
    {
        auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (draw)
        {
            const auto it = m_menuVisuals.find(draw->itemID);
            if (it != m_menuVisuals.end())
            {
                const ThemePalette palette = m_themePlatform ? m_themePlatform->BuildPalette() : ThemePalette{};
                Win32Helpers::DrawThemedPopupMenuItem(draw, palette, it->second);
                return TRUE;
            }
        }
        break;
    }

    case WM_DROPFILES:
        OnDropFiles(reinterpret_cast<HDROP>(wParam));
        return 0;

    case WM_MOVE:
    {
        int x = (int)(short)LOWORD(lParam);
        int y = (int)(short)HIWORD(lParam);
        OnMove(x, y);
        return 0;
    }

    case WM_SIZE:
    {
        if (wParam == SIZE_MINIMIZED)
        {
            // Keep spaces on desktop even when shell tries to minimize windows
            // (e.g. Win+D / Show Desktop action).
            ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
            return 0;
        }

        int width = (int)(short)LOWORD(lParam);
        int height = (int)(short)HIWORD(lParam);
        OnSize(width, height);
        return 0;
    }

    case WM_SYSCOMMAND:
    {
        const UINT command = static_cast<UINT>(wParam & 0xFFF0);
        if (command == SC_MINIMIZE)
        {
            // Keep spaces active overlays even when shell attempts global minimize
            // (Win+D / Show Desktop).
            ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
            return 0;
        }
        break;
    }

    case WM_WINDOWPOSCHANGING:
    {
        auto* pos = reinterpret_cast<WINDOWPOS*>(lParam);
        if (pos)
        {
            // Ignore shell hide requests so spaces stay visible as desktop overlays.
            pos->flags &= ~SWP_HIDEWINDOW;
        }
        break;
    }

    case WM_EXITSIZEMOVE:
    {
        if (m_manager)
        {
            RECT rc{};
            GetWindowRect(m_hwnd, &rc);
            if (m_geometryCorrelationId.empty())
            {
                m_geometryCorrelationId = NewCorrelationId(L"resize");
            }

            m_manager->UpdateSpaceGeometry(m_model.id,
                                           rc.left,
                                           rc.top,
                                           rc.right - rc.left,
                                           rc.bottom - rc.top,
                                           m_geometryCorrelationId);
            TraceDebug(L"Geometry committed on exit-size-move", m_geometryCorrelationId);
            m_geometryCorrelationId.clear();
        }
        return 0;
    }

    case WM_DESTROY:
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void SpaceWindow::OnPaint()
{
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rc{};
    GetClientRect(m_hwnd, &rc);

    const ThemePalette palette = m_themePlatform ? m_themePlatform->BuildPalette() : ThemePalette{};
    const int titleBarHeight = m_themePlatform ? m_themePlatform->GetFenceTitleBarHeightPx() : kTitleBarHeight;

    // Draw background
    HBRUSH bgBrush = CreateSolidBrush(palette.surfaceColor);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Draw title bar
    RECT titleRc = rc;
    titleRc.bottom = titleBarHeight;
    int titleOpacityPercent = 88;
    if (m_themePlatform)
    {
        titleOpacityPercent = m_themePlatform->GetSpaceTitleBarOpacityPercent();
    }
    const COLORREF translucentTitleColor = BlendColor(
        palette.surfaceColor,
        palette.spaceTitleBarColor,
        (titleOpacityPercent * 255) / 100);
    HBRUSH titleBrush = CreateSolidBrush(translucentTitleColor);
    FillRect(hdc, &titleRc, titleBrush);
    DeleteObject(titleBrush);

    // Apply title bar opacity: at 100%, window is fully solid; below 100%, apply blending
    if (titleOpacityPercent < 100)
    {
        LONG_PTR exStyle = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
        if ((exStyle & WS_EX_LAYERED) == 0)
        {
            SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        }
    }
    else
    {
        // At 100% opacity, fully remove layered window style for true solidity
        LONG_PTR exStyle = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
        if ((exStyle & WS_EX_LAYERED) != 0)
        {
            SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
            SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
    }

    // Draw title text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, palette.spaceTitleTextColor);
    RECT textRc = titleRc;
    textRc.left += 8;
    textRc.top += 4;
    std::wstring titleText = m_model.title;
    if (!m_model.contentState.empty() && m_model.contentState != L"ready")
    {
        titleText += L" [" + m_model.contentState + L"]";
    }
    DrawTextW(hdc, titleText.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Draw items
    const bool textOnly = m_model.textOnlyMode;
    const EffectiveSpacePolicy policy = ResolveEffectivePolicy();
    const int iconTileSize = policy.iconTileSize;
    const int contentLeft = 8;
    const int contentTop = titleBarHeight + 8;

    if (textOnly)
    {
        int itemY = contentTop;
        static constexpr int kItemHeight = 24;
        static constexpr int kIconSize = 16;

        for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
        {
            const auto& item = m_items[i];
            RECT itemRc = rc;
            itemRc.left += contentLeft;
            itemRc.top = itemY;
            itemRc.bottom = itemY + kItemHeight;
            itemRc.right -= contentLeft;

            if (i == m_selectedItem)
            {
                HBRUSH hiBrush = CreateSolidBrush(palette.spaceItemHoverColor);
                FillRect(hdc, &itemRc, hiBrush);
                DeleteObject(hiBrush);
                SetTextColor(hdc, palette.spaceTitleTextColor);
            }
            else
            {
                SetTextColor(hdc, palette.spaceItemTextColor);
            }

            RECT itemTextRc = itemRc;
            itemTextRc.left += 4;
            DrawTextW(hdc, item.name.c_str(), -1, &itemTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            itemY += kItemHeight;
        }
    }
    else
    {
        const int contentWidth = max(1, (rc.right - rc.left) - (contentLeft * 2));
        const int cols = max(1, contentWidth / iconTileSize);

        for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
        {
            const auto& item = m_items[i];
            const int row = i / cols;
            const int col = i % cols;

            RECT tileRc{};
            tileRc.left = contentLeft + (col * iconTileSize);
            tileRc.top = contentTop + (row * iconTileSize);
            tileRc.right = tileRc.left + iconTileSize;
            tileRc.bottom = tileRc.top + iconTileSize;

            if (i == m_selectedItem)
            {
                HBRUSH hiBrush = CreateSolidBrush(palette.spaceItemHoverColor);
                FillRect(hdc, &tileRc, hiBrush);
                DeleteObject(hiBrush);

                HPEN ringPen = CreatePen(PS_SOLID, 2, palette.accentColor);
                HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, ringPen));
                HBRUSH oldBrush = reinterpret_cast<HBRUSH>(SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)));
                Rectangle(hdc, tileRc.left + 1, tileRc.top + 1, tileRc.right - 1, tileRc.bottom - 1);
                SelectObject(hdc, oldBrush);
                SelectObject(hdc, oldPen);
                DeleteObject(ringPen);
            }

            if (item.iconIndex >= 0 && m_imageList != nullptr)
            {
                const int iconX = tileRc.left + ((iconTileSize - kIconGridSize) / 2);
                const int iconY = tileRc.top + ((iconTileSize - kIconGridSize) / 2) - (policy.labelsOnHover ? 6 : 0);
                ImageList_DrawEx(m_imageList, item.iconIndex, hdc, iconX, iconY, kIconGridSize, kIconGridSize, CLR_NONE, CLR_NONE, ILD_TRANSPARENT);
            }

            if (policy.labelsOnHover && i == m_selectedItem)
            {
                RECT labelRc = tileRc;
                labelRc.top = tileRc.bottom - 18;
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, palette.spaceTitleTextColor);
                DrawTextW(hdc, item.name.c_str(), -1, &labelRc, DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
        }
    }

    EndPaint(m_hwnd, &ps);
}

void SpaceWindow::OnLButtonDown(int x, int y)
{
    const int titleBarHeight = m_themePlatform ? m_themePlatform->GetFenceTitleBarHeightPx() : kTitleBarHeight;
    if (y < titleBarHeight)
    {
        (void)x;
        ReleaseCapture();
        SendMessageW(m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }
}

void SpaceWindow::OnMouseMove(int x, int y, WPARAM flags)
{
    (void)flags;

    if (!m_mouseInside)
    {
        m_activeCorrelationId = NewCorrelationId(L"hover");
        m_mouseInside = true;
        TraceDebug(L"Mouse enter", m_activeCorrelationId);
        ApplyIdleVisualState();
    }

    TRACKMOUSEEVENT tme{};
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_hwnd;
    TrackMouseEvent(&tme);

    // Track item under cursor for highlight
    const int item = GetItemAtPosition(x, y);
    if (item != m_selectedItem)
    {
        m_selectedItem = item;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void SpaceWindow::OnLButtonUp()
{
    // Native drag move path (WM_NCLBUTTONDOWN + HTCAPTION) handles release for us.
}

void SpaceWindow::OnContextMenu(int x, int y)
{
    int itemIndex = GetItemAtPosition(x, y);
    HMENU menu = CreatePopupMenu();
    m_menuVisuals.clear();
    m_menuCommands.clear();

    CommandContext commandContext;
    commandContext.space.id = m_model.id;
    commandContext.space.title = m_model.title;
    commandContext.space.backingFolderPath = m_model.backingFolder;
    commandContext.space.contentType = m_model.contentType;
    commandContext.space.contentPluginId = m_model.contentPluginId;
    commandContext.space.contentSource = m_model.contentSource;
    commandContext.space.contentState = m_model.contentState;
    commandContext.space.contentStateDetail = m_model.contentStateDetail;
    commandContext.invocationSource = itemIndex >= 0 ? L"item_context" : L"space_context";
    if (itemIndex >= 0 && itemIndex < static_cast<int>(m_items.size()))
    {
        const SpaceItem& item = m_items[itemIndex];
        SpaceItemMetadata itemMeta;
        itemMeta.name = item.name;
        itemMeta.fullPath = item.fullPath;
        itemMeta.originalPath = item.originalPath;
        itemMeta.isDirectory = item.isDirectory;
        commandContext.item = itemMeta;
    }

    UINT decorationId = 3000;
    UINT pluginCommandId = 4000;
    auto appendSeparator = [&]() {
        AppendMenuW(menu, MF_OWNERDRAW | MF_DISABLED, decorationId, nullptr);
        m_menuVisuals.emplace(decorationId, Win32Helpers::PopupMenuItemVisual{L"", L"", L"", true, false});
        ++decorationId;
    };

    auto appendItem = [&](const std::wstring& text, UINT commandId) {
        AppendMenuW(menu, MF_OWNERDRAW | MF_STRING, commandId, nullptr);
        m_menuVisuals.emplace(commandId, Win32Helpers::PopupMenuItemVisual{text, L"", L"", false, true});
    };

    auto appendPluginItems = [&](MenuSurface surface) {
        if (!m_manager)
        {
            return;
        }

        const auto contributions = m_manager->GetMenuContributions(surface);
        for (const auto& contribution : contributions)
        {
            if (contribution.separatorBefore)
            {
                appendSeparator();
            }

            AppendMenuW(menu, MF_OWNERDRAW | MF_STRING, pluginCommandId, nullptr);
            m_menuVisuals.emplace(pluginCommandId, Win32Helpers::PopupMenuItemVisual{contribution.title, L"", L"", false, true});
            m_menuCommands.emplace(pluginCommandId, contribution.commandId);
            ++pluginCommandId;
        }
    };

    if (itemIndex >= 0)
    {
        // Item context menu
        appendItem(L"Open", 2001);
        appendSeparator();
        appendItem(L"Delete Item", 2002);
        appendPluginItems(MenuSurface::ItemContext);
    }
    else
    {
        // Space context menu
        const EffectiveSpacePolicy effectivePolicy = ResolveEffectivePolicy();
        appendItem(L"New Space", 1001);
        appendItem(L"Rename Space", 1002);
        appendItem(L"Space Settings...", kCmdSpaceSettings);
        appendItem(L"Text Only Mode", 1004);
        appendItem(L"Roll Up When Not Hovered", 1005);
        appendItem(L"Transparent When Not Hovered", 1006);

        const UINT textState = m_model.textOnlyMode ? MF_CHECKED : MF_UNCHECKED;
        const UINT rollupState = effectivePolicy.rollupWhenNotHovered ? MF_CHECKED : MF_UNCHECKED;
        const UINT transparentState = effectivePolicy.transparentWhenNotHovered ? MF_CHECKED : MF_UNCHECKED;
        CheckMenuItem(menu, 1004, MF_BYCOMMAND | textState);
        CheckMenuItem(menu, 1005, MF_BYCOMMAND | rollupState);
        CheckMenuItem(menu, 1006, MF_BYCOMMAND | transparentState);

        appendSeparator();
        appendPluginItems(MenuSurface::SpaceContext);
        appendSeparator();
        appendItem(L"Delete Space", 1003);
    }

    POINT pt = { x, y };
    ClientToScreen(m_hwnd, &pt);

    int cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, m_hwnd, nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);

    const auto pluginCommand = m_menuCommands.find(static_cast<UINT>(cmd));
    if (pluginCommand != m_menuCommands.end())
    {
        commandContext.commandId = pluginCommand->second;
        if (m_manager)
        {
            m_manager->ExecuteCommand(pluginCommand->second, commandContext);
        }
        return;
    }

    switch (cmd)
    {
    case 1001: // New Space
        if (m_manager)
            m_manager->CreateSpaceAt(pt.x, pt.y);
        break;

    case 1002: // Rename
        if (m_manager)
        {
            std::wstring newTitle;
            if (Win32Helpers::PromptTextInput(m_hwnd,
                                              L"Rename Space",
                                              L"Space title:",
                                              m_model.title,
                                              newTitle))
            {
                newTitle = TrimWhitespace(newTitle);
                if (!newTitle.empty() && newTitle.size() <= 128)
                {
                    m_manager->RenameSpace(m_model.id, newTitle);
                }
                else
                {
                    Win32Helpers::ShowUserWarning(m_hwnd, L"Rename Space", L"Space title must be 1 to 128 characters.");
                }
            }
        }
        break;

    case kCmdSpaceSettings:
        ShowSettingsPanel();
        break;

    case 1004: // Text only mode
        if (m_manager)
            m_manager->SetSpaceTextOnlyMode(m_model.id, !m_model.textOnlyMode);
        break;

    case 1005: // Roll up when not hovered
        if (m_manager)
        {
            const std::wstring cid = NewCorrelationId(L"rollup_setting");
            TraceDebug(L"Toggle roll-up policy", cid);
            m_manager->SetSpaceRollupWhenNotHovered(m_model.id, !m_model.rollupWhenNotHovered, cid);
        }
        break;

    case 1006: // Transparent when not hovered
        if (m_manager)
        {
            const std::wstring cid = NewCorrelationId(L"transparency_setting");
            TraceDebug(L"Toggle transparency policy", cid);
            m_manager->SetSpaceTransparentWhenNotHovered(m_model.id, !m_model.transparentWhenNotHovered, cid);
        }
        break;

    case 1003: // Delete Space
        if (m_manager)
        {
            const std::wstring cid = NewCorrelationId(L"delete");
            TraceDebug(L"Delete space requested", cid);
            m_manager->DeleteSpace(m_model.id, cid);
        }
        break;

    case 2001: // Open item
        if (itemIndex >= 0)
            ExecuteItem(itemIndex);
        break;

    case 2002: // Delete item
        if (itemIndex >= 0 && itemIndex < static_cast<int>(m_items.size()))
        {
            if (m_manager)
                m_manager->DeleteItem(m_model.id, m_items[itemIndex]);
        }
        break;
    }
}

void SpaceWindow::OnDropFiles(HDROP hDrop)
{
    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::wstring> paths;
    paths.reserve(count);

    for (UINT i = 0; i < count; ++i)
    {
        const UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
        if (len == 0)
        {
            continue;
        }

        std::wstring path;
        path.resize(len + 1);
        DragQueryFileW(hDrop, i, path.data(), len + 1);
        path.resize(len);
        paths.push_back(path);
    }

    DragFinish(hDrop);

    if (m_manager)
    {
        const std::wstring cid = NewCorrelationId(L"drop");
        TraceDebug(L"Drop received count=" + std::to_wstring(paths.size()), cid);
        m_manager->HandleDrop(m_model.id, paths, cid);
    }
}

void SpaceWindow::OnMove(int x, int y)
{
    if (m_geometryCorrelationId.empty())
    {
        m_geometryCorrelationId = NewCorrelationId(L"resize");
    }

    if (m_model.x != x || m_model.y != y)
    {
        TraceDebug(L"Move to (" + std::to_wstring(x) + L"," + std::to_wstring(y) + L")", m_geometryCorrelationId);
    }

    // Update model position
    m_model.x = x;
    m_model.y = y;
}

void SpaceWindow::OnSize(int width, int height)
{
    if (m_internalIdleResize)
    {
        InvalidateRect(m_hwnd, nullptr, TRUE);
        return;
    }

    if (m_geometryCorrelationId.empty())
    {
        m_geometryCorrelationId = NewCorrelationId(L"resize");
    }

    if (m_model.width != width || m_model.height != height)
    {
        TraceDebug(L"Resize to (" + std::to_wstring(width) + L"x" + std::to_wstring(height) + L")", m_geometryCorrelationId);
    }

    // Update model size
    m_model.width = width;
    m_model.height = height;
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

int SpaceWindow::GetItemAtPosition(int x, int y) const
{
    const int titleBarHeight = m_themePlatform ? m_themePlatform->GetFenceTitleBarHeightPx() : kTitleBarHeight;
    // Check if click is in title bar
    if (y < titleBarHeight)
        return -1;

    if (m_model.textOnlyMode)
    {
        int itemY = titleBarHeight + 8;
        static constexpr int kItemHeight = 24;
        for (size_t i = 0; i < m_items.size(); ++i)
        {
            if (y >= itemY && y < itemY + kItemHeight)
            {
                return static_cast<int>(i);
            }
            itemY += kItemHeight;
        }
        return -1;
    }

    const EffectiveSpacePolicy policy = ResolveEffectivePolicy();
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    const int contentLeft = 8;
    const int contentTop = titleBarHeight + 8;
    const int iconTileSize = policy.iconTileSize;
    const int contentWidth = max(1, (rc.right - rc.left) - (contentLeft * 2));
    const int cols = max(1, contentWidth / iconTileSize);

    const int localX = x - contentLeft;
    const int localY = y - contentTop;
    if (localX < 0 || localY < 0)
    {
        return -1;
    }

    const int col = localX / iconTileSize;
    const int row = localY / iconTileSize;
    if (col < 0 || col >= cols)
    {
        return -1;
    }

    const int index = row * cols + col;
    if (index < 0 || index >= static_cast<int>(m_items.size()))
    {
        return -1;
    }

    const int insideTileX = localX % iconTileSize;
    const int insideTileY = localY % iconTileSize;
    if (insideTileX < 0 || insideTileY < 0 || insideTileX >= iconTileSize || insideTileY >= iconTileSize)
    {
        return -1;
    }

    return index;
}

void SpaceWindow::ApplyIdleVisualState()
{
    if (!m_hwnd)
    {
        return;
    }

    const bool previousHover = m_mouseInside;
    const bool previousRolledUp = m_isRolledUp;

    // Recompute hover state from current cursor position. This avoids a stuck
    // rolled state when a mouse move/leave event is missed by the window loop.
    POINT cursor{};
    RECT windowRect{};
    if (GetCursorPos(&cursor) && GetWindowRect(m_hwnd, &windowRect))
    {
        m_mouseInside = (PtInRect(&windowRect, cursor) != FALSE);
    }

    const EffectiveSpacePolicy policy = ResolveEffectivePolicy();
    const bool shouldRollup = policy.rollupWhenNotHovered && !m_mouseInside;

    // Compute a safe rolled total height that still leaves the full custom
    // title bar client area visible after non-client frame metrics are applied.
    RECT desiredClient{};
    desiredClient.left = 0;
    desiredClient.top = 0;
    desiredClient.right = 1;
    const int titleBarHeight = m_themePlatform ? m_themePlatform->GetFenceTitleBarHeightPx() : kTitleBarHeight;
    desiredClient.bottom = titleBarHeight;

    const LONG_PTR style = GetWindowLongPtrW(m_hwnd, GWL_STYLE);
    const LONG_PTR exStyleForRect = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&desiredClient, static_cast<DWORD>(style), FALSE, static_cast<DWORD>(exStyleForRect));
    const int rolledHeight = max((desiredClient.bottom - desiredClient.top), titleBarHeight + 16);

    if (shouldRollup && !m_isRolledUp)
    {
        if (m_activeCorrelationId.empty())
        {
            m_activeCorrelationId = NewCorrelationId(L"rollup");
        }

        RECT rc{};
        GetWindowRect(m_hwnd, &rc);
        const int currentWidth = max(1, rc.right - rc.left);
        const int currentHeight = rc.bottom - rc.top;
        if (currentHeight > rolledHeight)
        {
            m_expandedHeight = currentHeight;
        }

        m_internalIdleResize = true;
        SetWindowPos(m_hwnd, nullptr, 0, 0, currentWidth, rolledHeight,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        m_internalIdleResize = false;
        m_isRolledUp = true;
    }
    else if (!shouldRollup && m_isRolledUp)
    {
        if (m_activeCorrelationId.empty())
        {
            m_activeCorrelationId = NewCorrelationId(L"rollup");
        }

        RECT rc{};
        GetWindowRect(m_hwnd, &rc);
        const int currentWidth = max(1, rc.right - rc.left);
        const int restoredHeight = max(m_expandedHeight, rolledHeight + 40);

        m_internalIdleResize = true;
        SetWindowPos(m_hwnd, nullptr, 0, 0, currentWidth, restoredHeight,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        m_internalIdleResize = false;
        m_isRolledUp = false;
    }

    // Keep rolled-up spaces opaque so the title bar remains visible and usable.
    const bool shouldBeTransparent = policy.transparentWhenNotHovered && !m_mouseInside && !m_isRolledUp;
    int idleOpacityPercent = 84;
    if (m_themePlatform)
    {
        idleOpacityPercent = m_themePlatform->GetSpaceIdleOpacityPercent();
    }
    
    LONG_PTR exStyle = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
    if (shouldBeTransparent && idleOpacityPercent < 100)
    {
        const BYTE idleAlpha = static_cast<BYTE>((idleOpacityPercent * 255) / 100);
        if ((exStyle & WS_EX_LAYERED) == 0)
        {
            SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        }
        SetLayeredWindowAttributes(m_hwnd, 0, idleAlpha, LWA_ALPHA);
    }
    else
    {
        // When not transparent or idle opacity is 100%, fully remove layered window style
        if ((exStyle & WS_EX_LAYERED) != 0)
        {
            SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
            SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
    }

    ++m_idleStateEvalCount;
    const bool hoverChanged = (previousHover != m_mouseInside);
    const bool rollChanged = (previousRolledUp != m_isRolledUp);
    const bool transparencyChanged = !m_hasLastLoggedTransparent || (m_lastLoggedTransparent != shouldBeTransparent);
    if (hoverChanged || rollChanged || transparencyChanged || ((m_idleStateEvalCount % 50) == 0))
    {
        std::wstringstream trace;
        trace << L"Idle eval#" << m_idleStateEvalCount
              << L" hover=" << BoolText(m_mouseInside)
              << L" rollPolicy=" << BoolText(policy.rollupWhenNotHovered)
              << L" shouldRollup=" << BoolText(shouldRollup)
              << L" rolled=" << BoolText(m_isRolledUp)
              << L" transparentPolicy=" << BoolText(policy.transparentWhenNotHovered)
              << L" transparent=" << BoolText(shouldBeTransparent)
              << L" rolledHeight=" << rolledHeight
              << L" expandedHeight=" << m_expandedHeight;
          TraceDebug(trace.str(), m_activeCorrelationId);
    }

    m_lastLoggedTransparent = shouldBeTransparent;
    m_hasLastLoggedTransparent = true;
}

std::wstring SpaceWindow::NewCorrelationId(const wchar_t* action) const
{
    const unsigned long long seq = gSpaceEventSequence.fetch_add(1, std::memory_order_relaxed);
    const std::wstring actionText = action ? action : L"event";
    return actionText + L"-" + std::to_wstring(seq);
}

void SpaceWindow::TraceDebug(const std::wstring& message, const std::wstring& correlationId) const
{
    Win32Helpers::LogInfo(L"[SpaceWindow][" + m_model.id + L"] " + CorrelationPrefix(correlationId) + message);
}

EffectiveSpacePolicy SpaceWindow::ResolveEffectivePolicy() const
{
    EffectiveSpacePolicy policy;

    std::wstring spacingPreset = m_model.iconSpacingPreset;
    if (spacingPreset.empty())
    {
        spacingPreset = L"comfortable";
    }

    policy.rollupWhenNotHovered = m_model.rollupWhenNotHovered;
    policy.transparentWhenNotHovered = m_model.transparentWhenNotHovered;
    policy.labelsOnHover = m_model.labelsOnHover;

    if (m_model.inheritThemePolicy && m_themePlatform)
    {
        const SpacePolicyDefaults defaults = m_themePlatform->ResolveSpacePolicyDefaults();
        policy.rollupWhenNotHovered = defaults.rollupWhenNotHovered;
        policy.transparentWhenNotHovered = defaults.transparentWhenNotHovered;
        policy.labelsOnHover = defaults.labelsOnHover;
        spacingPreset = defaults.iconSpacingPreset;
    }

    policy.iconTileSize = TileSizeFromPreset(spacingPreset);
    return policy;
}

void SpaceWindow::ShowSettingsPanel()
{
    if (m_settingsPanel && IsWindow(m_settingsPanel))
    {
        ShowWindow(m_settingsPanel, SW_SHOWNORMAL);
        SetForegroundWindow(m_settingsPanel);
        return;
    }

    static bool registered = false;
    if (!registered)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = SpaceWindow::SettingsPanelWndProcStatic;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kSpaceSettingsClass;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_SPACES_APP));
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassW(&wc);
        registered = true;
    }

    POINT pt{};
    GetCursorPos(&pt);
    m_settingsPanel = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        kSpaceSettingsClass,
        L"Space Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        pt.x - 120,
        pt.y - 60,
        360,
        300,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        this);

    if (m_settingsPanel)
    {
        ShowWindow(m_settingsPanel, SW_SHOWNORMAL);
        UpdateWindow(m_settingsPanel);
    }
}

LRESULT CALLBACK SpaceWindow::SettingsPanelWndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    SpaceWindow* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<SpaceWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<SpaceWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self)
    {
        return self->SettingsPanelWndProc(hwnd, msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT SpaceWindow::SettingsPanelWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        const EffectiveSpacePolicy effectivePolicy = ResolveEffectivePolicy();

        CreateWindowExW(0, L"BUTTON", L"Use theme profile defaults",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX | BS_OWNERDRAW,
                        16, 16, 220, 24, hwnd,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCtlInheritThemePolicy)),
                        GetModuleHandleW(nullptr), nullptr);

        CreateWindowExW(0, L"BUTTON", L"Text only mode",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX | BS_OWNERDRAW,
                        16, 44, 220, 24, hwnd,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCtlTextOnly)),
                        GetModuleHandleW(nullptr), nullptr);

        CreateWindowExW(0, L"BUTTON", L"Roll up when not hovered",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX | BS_OWNERDRAW,
                        16, 72, 220, 24, hwnd,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCtlRollup)),
                        GetModuleHandleW(nullptr), nullptr);

        CreateWindowExW(0, L"BUTTON", L"Transparent when not hovered",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX | BS_OWNERDRAW,
                        16, 100, 240, 24, hwnd,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCtlTransparent)),
                        GetModuleHandleW(nullptr), nullptr);

        CreateWindowExW(0, L"BUTTON", L"Show labels on hover",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX | BS_OWNERDRAW,
                        16, 128, 220, 24, hwnd,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCtlLabelsOnHover)),
                        GetModuleHandleW(nullptr), nullptr);

        CreateWindowExW(0, L"STATIC", L"Icon spacing:",
                        WS_CHILD | WS_VISIBLE,
                        16, 160, 100, 20, hwnd,
                        nullptr, GetModuleHandleW(nullptr), nullptr);

        HWND combo = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                     WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                     120, 156, 180, 300, hwnd,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCtlSpacingPreset)),
                                     GetModuleHandleW(nullptr), nullptr);
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Compact"));
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Comfortable"));
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Spacious"));

        CreateWindowExW(0, L"BUTTON", L"Close",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        240, 220, 90, 28, hwnd,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCtlClose)),
                        GetModuleHandleW(nullptr), nullptr);

        CreateWindowExW(0, L"BUTTON", L"Apply to all spaces",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        16, 220, 150, 28, hwnd,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCtlApplyAll)),
                        GetModuleHandleW(nullptr), nullptr);

        SendMessageW(GetDlgItem(hwnd, kCtlInheritThemePolicy), BM_SETCHECK, m_model.inheritThemePolicy ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, kCtlTextOnly), BM_SETCHECK, m_model.textOnlyMode ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, kCtlRollup), BM_SETCHECK, effectivePolicy.rollupWhenNotHovered ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, kCtlTransparent), BM_SETCHECK, effectivePolicy.transparentWhenNotHovered ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, kCtlLabelsOnHover), BM_SETCHECK, effectivePolicy.labelsOnHover ? BST_CHECKED : BST_UNCHECKED, 0);

        int spacingSel = 1;
        if (effectivePolicy.iconTileSize <= 48) spacingSel = 0;
        else if (effectivePolicy.iconTileSize >= 68) spacingSel = 2;
        SendMessageW(combo, CB_SETCURSEL, spacingSel, 0);

        const BOOL customEnabled = m_model.inheritThemePolicy ? FALSE : TRUE;
        EnableWindow(GetDlgItem(hwnd, kCtlRollup), customEnabled);
        EnableWindow(GetDlgItem(hwnd, kCtlTransparent), customEnabled);
        EnableWindow(GetDlgItem(hwnd, kCtlLabelsOnHover), customEnabled);
        EnableWindow(GetDlgItem(hwnd, kCtlSpacingPreset), customEnabled);
        return 0;
    }
    case WM_COMMAND:
    {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);

        if (!m_manager)
        {
            break;
        }

        if (id == kCtlClose && code == BN_CLICKED)
        {
            DestroyWindow(hwnd);
            return 0;
        }

        if (id == kCtlApplyAll && code == BN_CLICKED)
        {
            m_manager->ApplySpaceSettingsToAll(m_model.id);
            MessageBoxW(hwnd, L"Current space settings were applied to all spaces.", L"Space Settings", MB_OK | MB_ICONINFORMATION);
            return 0;
        }

        if (id == kCtlInheritThemePolicy && code == BN_CLICKED)
        {
            const bool enabled = (SendMessageW(GetDlgItem(hwnd, kCtlInheritThemePolicy), BM_GETCHECK, 0, 0) == BST_CHECKED);
            m_manager->SetSpaceThemePolicyInheritance(m_model.id, enabled);
            const BOOL customEnabled = enabled ? FALSE : TRUE;
            EnableWindow(GetDlgItem(hwnd, kCtlRollup), customEnabled);
            EnableWindow(GetDlgItem(hwnd, kCtlTransparent), customEnabled);
            EnableWindow(GetDlgItem(hwnd, kCtlLabelsOnHover), customEnabled);
            EnableWindow(GetDlgItem(hwnd, kCtlSpacingPreset), customEnabled);
            return 0;
        }

        if (id == kCtlTextOnly && code == BN_CLICKED)
        {
            const bool enabled = (SendMessageW(GetDlgItem(hwnd, kCtlTextOnly), BM_GETCHECK, 0, 0) == BST_CHECKED);
            m_manager->SetSpaceTextOnlyMode(m_model.id, enabled);
            return 0;
        }

        if (id == kCtlRollup && code == BN_CLICKED)
        {
            const bool enabled = (SendMessageW(GetDlgItem(hwnd, kCtlRollup), BM_GETCHECK, 0, 0) == BST_CHECKED);
            const std::wstring cid = NewCorrelationId(L"rollup_setting");
            TraceDebug(L"Settings panel roll-up toggle", cid);
            m_manager->SetSpaceRollupWhenNotHovered(m_model.id, enabled, cid);
            return 0;
        }

        if (id == kCtlTransparent && code == BN_CLICKED)
        {
            const bool enabled = (SendMessageW(GetDlgItem(hwnd, kCtlTransparent), BM_GETCHECK, 0, 0) == BST_CHECKED);
            const std::wstring cid = NewCorrelationId(L"transparency_setting");
            TraceDebug(L"Settings panel transparency toggle", cid);
            m_manager->SetSpaceTransparentWhenNotHovered(m_model.id, enabled, cid);
            return 0;
        }

        if (id == kCtlLabelsOnHover && code == BN_CLICKED)
        {
            const bool enabled = (SendMessageW(GetDlgItem(hwnd, kCtlLabelsOnHover), BM_GETCHECK, 0, 0) == BST_CHECKED);
            m_manager->SetSpaceLabelsOnHover(m_model.id, enabled);
            return 0;
        }

        if (id == kCtlSpacingPreset && code == CBN_SELCHANGE)
        {
            const LRESULT sel = SendMessageW(GetDlgItem(hwnd, kCtlSpacingPreset), CB_GETCURSEL, 0, 0);
            std::wstring preset = L"comfortable";
            if (sel == 0) preset = L"compact";
            else if (sel == 2) preset = L"spacious";
            m_manager->SetSpaceIconSpacingPreset(m_model.id, preset);
            return 0;
        }
        break;
    }
    case WM_DRAWITEM:
    {
        auto* drawInfo = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (drawInfo && drawInfo->CtlType == ODT_BUTTON &&
            IsSettingsToggleControlId(static_cast<int>(drawInfo->CtlID)))
        {
            DrawSettingsToggleControl(drawInfo, RGB(248, 248, 248), RGB(114, 99, 255));
            return TRUE;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        m_settingsPanel = nullptr;
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void SpaceWindow::ExecuteItem(int itemIndex)
{
    if (itemIndex < 0 || itemIndex >= static_cast<int>(m_items.size()))
        return;

    const auto& item = m_items[itemIndex];
    
    // Use ShellExecute to open the file/folder
    ShellExecuteW(m_hwnd, L"open", item.fullPath.c_str(), nullptr, nullptr, SW_SHOW);
}

void SpaceWindow::OnLButtonDblClk(int x, int y)
{
    int itemIndex = GetItemAtPosition(x, y);
    if (itemIndex >= 0)
    {
        ExecuteItem(itemIndex);
    }
}

bool SpaceWindow::InitializeImageList()
{
    // Get system small image list handle
    // We don't own this handle - it's managed by Windows, so we don't destroy it
    try
    {
        SHFILEINFOW sfi{};
        m_imageList = reinterpret_cast<HIMAGELIST>(
            SHGetFileInfoW(L".", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), 
                          SHGFI_SYSICONINDEX | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES)
        );
        return m_imageList != nullptr;
    }
    catch (const std::exception&)
    {
        Win32Helpers::LogError(L"InitializeImageList failed due to unexpected exception.");
        m_imageList = nullptr;
        return false;
    }
}
