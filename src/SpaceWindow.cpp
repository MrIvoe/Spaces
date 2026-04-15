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
#include <functional>
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
static constexpr UINT_PTR kEntranceAnimTimerId = 2;
static constexpr UINT kEntranceAnimFrameMs = 16;
static constexpr DWORD kEntranceAnimDurationMs = 220;
static constexpr int kExplorerHeaderHeight = 24;

static constexpr int kCmdSpaceSettings = 1007;
static constexpr int kCmdSetStackGroup = 1008;
static constexpr int kCmdSetParentContainer = 1009;
static constexpr int kCmdSetLayoutModeFree = 1010;
static constexpr int kCmdSetLayoutModeStacked = 1011;
static constexpr int kCmdSetLayoutModeContained = 1012;
static constexpr int kToolbarButtonNone = 0;
static constexpr int kToolbarButtonSettings = 1;
static constexpr int kToolbarButtonViewToggle = 2;
static constexpr int kCtlInheritThemePolicy = 5001;
static constexpr int kCtlTextOnly = 5002;
static constexpr int kCtlRollup = 5003;
static constexpr int kCtlTransparent = 5004;
static constexpr int kCtlLabelsOnHover = 5005;
static constexpr int kCtlSpacingPreset = 5006;
static constexpr int kCtlClose = 5007;
static constexpr int kCtlApplyAll = 5008;
static constexpr int kHotkeyCommandPalette = 7001;

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

    bool IsSettingsActionButtonId(int controlId)
    {
        return controlId == kCtlClose || controlId == kCtlApplyAll;
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

    void DrawSettingsActionButton(const DRAWITEMSTRUCT* drawInfo,
                                  COLORREF windowColor,
                                  COLORREF surfaceColor,
                                  COLORREF textColor,
                                  COLORREF accentColor,
                                  const std::wstring& buttonFamily)
    {
        if (!drawInfo || !drawInfo->hwndItem)
        {
            return;
        }

        const bool pressed = (drawInfo->itemState & ODS_SELECTED) != 0;
        const bool focused = (drawInfo->itemState & ODS_FOCUS) != 0;
        const bool hovered = (drawInfo->itemState & ODS_HOTLIGHT) != 0;
        const bool disabled = (drawInfo->itemState & ODS_DISABLED) != 0;

        COLORREF bg = BlendColor(surfaceColor, accentColor, 18);
        COLORREF border = BlendColor(surfaceColor, textColor, 44);
        COLORREF fg = textColor;

        if (buttonFamily == L"outlined")
        {
            bg = BlendColor(windowColor, surfaceColor, hovered ? 24 : 12);
            border = hovered ? accentColor : BlendColor(surfaceColor, textColor, 56);
        }
        else if (buttonFamily == L"high-contrast")
        {
            bg = hovered ? accentColor : BlendColor(surfaceColor, textColor, 105);
            border = hovered ? accentColor : BlendColor(surfaceColor, textColor, 120);
            fg = hovered ? RGB(255, 255, 255) : textColor;
        }
        else if (buttonFamily == L"compact")
        {
            bg = BlendColor(surfaceColor, windowColor, hovered ? 36 : 16);
        }

        if (pressed)
        {
            bg = BlendColor(bg, textColor, 24);
        }
        if (disabled)
        {
            bg = BlendColor(surfaceColor, windowColor, 60);
            border = BlendColor(surfaceColor, textColor, 24);
            fg = BlendColor(textColor, surfaceColor, 120);
        }

        HDC hdc = drawInfo->hDC;
        RECT rc = drawInfo->rcItem;
        InflateRect(&rc, -1, -1);

        const int radius = (buttonFamily == L"soft") ? 9 : 6;
        HPEN pen = CreatePen(PS_SOLID, 1, border);
        HBRUSH brush = CreateSolidBrush(bg);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, brush);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);

        wchar_t label[128]{};
        GetWindowTextW(drawInfo->hwndItem, label, static_cast<int>(std::size(label)));
        RECT textRc = rc;
        if (pressed)
        {
            OffsetRect(&textRc, 1, 1);
        }
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, fg);
        SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
        DrawTextW(hdc, label, -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        if (focused)
        {
            RECT focusRc = rc;
            InflateRect(&focusRc, -3, -3);
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

    std::wstring ToLower(const std::wstring& value)
    {
        std::wstring lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](wchar_t c) {
            return static_cast<wchar_t>(std::towlower(c));
        });
        return lowered;
    }

    int FuzzyScore(const std::wstring& queryRaw, const std::wstring& candidateRaw)
    {
        if (queryRaw.empty())
        {
            return 1;
        }

        const std::wstring query = ToLower(queryRaw);
        const std::wstring candidate = ToLower(candidateRaw);
        if (candidate.find(query) != std::wstring::npos)
        {
            return static_cast<int>(200 + query.size());
        }

        size_t qi = 0;
        int score = 0;
        for (size_t ci = 0; ci < candidate.size() && qi < query.size(); ++ci)
        {
            if (candidate[ci] == query[qi])
            {
                ++qi;
                score += 8;
                if (ci == 0 || candidate[ci - 1] == L' ' || candidate[ci - 1] == L'.' || candidate[ci - 1] == L'-')
                {
                    score += 4;
                }
            }
        }

        return (qi == query.size()) ? score : -1;
    }

    bool IsLoadingContentState(const std::wstring& stateRaw)
    {
        const std::wstring state = ToLower(stateRaw);
        return state.find(L"loading") != std::wstring::npos ||
               state.find(L"refresh") != std::wstring::npos ||
               state.find(L"pending") != std::wstring::npos ||
               state.find(L"sync") != std::wstring::npos ||
               state.find(L"install") != std::wstring::npos ||
               state.find(L"updat") != std::wstring::npos;
    }

    int EstimateBadgeWidth(const std::wstring& text)
    {
        const int perChar = 7;
        return (std::max)(52, static_cast<int>(text.size() * perChar) + 18);
    }

    void DrawTitleBadge(HDC hdc,
                        RECT rc,
                        const std::wstring& text,
                        COLORREF fill,
                        COLORREF border,
                        COLORREF fg)
    {
        HPEN pen = CreatePen(PS_SOLID, 1, border);
        HBRUSH brush = CreateSolidBrush(fill);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, brush);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 7, 7);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, fg);
        RECT textRc = rc;
        textRc.left += 8;
        textRc.right -= 8;
        DrawTextW(hdc, text.c_str(), -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    void ResolveGridLayout(int contentWidth, int preferredTileSize, int& outCols, int& outTileSize)
    {
        outTileSize = (std::max)(44, preferredTileSize);
        // Add a small bias so columns increase a bit earlier on resize.
        outCols = (std::max)(1, (contentWidth + (outTileSize / 3)) / outTileSize);
        if (outCols > 1)
        {
            outTileSize = (std::max)(44, contentWidth / outCols);
        }
    }

    bool MoveItemInVector(std::vector<SpaceItem>& items, int fromIndex, int toIndex)
    {
        if (fromIndex < 0 || toIndex < 0 ||
            fromIndex >= static_cast<int>(items.size()) ||
            toIndex >= static_cast<int>(items.size()) ||
            fromIndex == toIndex)
        {
            return false;
        }

        SpaceItem moved = items[static_cast<size_t>(fromIndex)];
        items.erase(items.begin() + fromIndex);
        items.insert(items.begin() + toIndex, std::move(moved));
        return true;
    }

    RECT GetToolbarButtonRect(const RECT& clientRc, int titleBarHeight, int buttonId)
    {
        RECT buttonRc{};
        const int buttonSize = (std::max)(20, titleBarHeight - 8);
        const int top = (titleBarHeight - buttonSize) / 2;
        const int rightPad = 8;
        const int gap = 6;

        if (buttonId == kToolbarButtonSettings)
        {
            buttonRc.right = clientRc.right - rightPad;
            buttonRc.left = buttonRc.right - buttonSize;
            buttonRc.top = top;
            buttonRc.bottom = top + buttonSize;
        }
        else if (buttonId == kToolbarButtonViewToggle)
        {
            buttonRc.right = clientRc.right - rightPad - buttonSize - gap;
            buttonRc.left = buttonRc.right - buttonSize;
            buttonRc.top = top;
            buttonRc.bottom = top + buttonSize;
        }

        return buttonRc;
    }

    int HitTestToolbarButton(const RECT& clientRc, int titleBarHeight, int x, int y)
    {
        if (y < 0 || y >= titleBarHeight)
        {
            return kToolbarButtonNone;
        }

        const RECT settingsRc = GetToolbarButtonRect(clientRc, titleBarHeight, kToolbarButtonSettings);
        if (x >= settingsRc.left && x < settingsRc.right && y >= settingsRc.top && y < settingsRc.bottom)
        {
            return kToolbarButtonSettings;
        }

        const RECT viewRc = GetToolbarButtonRect(clientRc, titleBarHeight, kToolbarButtonViewToggle);
        if (x >= viewRc.left && x < viewRc.right && y >= viewRc.top && y < viewRc.bottom)
        {
            return kToolbarButtonViewToggle;
        }

        return kToolbarButtonNone;
    }

    void DrawTitleToolbarButton(HDC hdc,
                                const RECT& buttonRc,
                                const std::wstring& glyph,
                                bool hovered,
                                bool active,
                                const ThemePalette& palette)
    {
        const COLORREF fill = active
            ? BlendColor(palette.accentColor, palette.spaceTitleBarColor, 72)
            : (hovered
                ? BlendColor(palette.spaceTitleBarColor, palette.spaceTitleTextColor, 36)
                : BlendColor(palette.spaceTitleBarColor, palette.surfaceColor, 22));
        const COLORREF border = active
            ? palette.accentColor
            : BlendColor(palette.spaceTitleBarColor, palette.spaceTitleTextColor, 60);
        const COLORREF text = active ? RGB(255, 255, 255) : palette.spaceTitleTextColor;

        HPEN pen = CreatePen(PS_SOLID, 1, border);
        HBRUSH brush = CreateSolidBrush(fill);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, brush);
        RoundRect(hdc, buttonRc.left, buttonRc.top, buttonRc.right, buttonRc.bottom, 6, 6);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, text);
        HFONT iconFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
        HGDIOBJ oldFont = SelectObject(hdc, iconFont ? iconFont : GetStockObject(DEFAULT_GUI_FONT));
        DrawTextW(hdc, glyph.c_str(), -1, const_cast<RECT*>(&buttonRc), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        if (iconFont)
        {
            DeleteObject(iconFont);
        }
    }

    int GetContentTop(int titleBarHeight)
    {
        // Keep content close to the title bar for a cleaner, classic fence look.
        return titleBarHeight + 8;
    }

    std::wstring BuildBreadcrumbText(const SpaceModel& model)
    {
        if (!model.backingFolder.empty())
        {
            const std::wstring& path = model.backingFolder;
            size_t end = path.size();
            while (end > 0 && (path[end - 1] == L'\\' || path[end - 1] == L'/'))
            {
                --end;
            }

            const size_t lastSep = path.find_last_of(L"\\/", end == 0 ? 0 : end - 1);
            std::wstring leaf = (lastSep == std::wstring::npos)
                ? path.substr(0, end)
                : path.substr(lastSep + 1, end - lastSep - 1);
            if (!leaf.empty())
            {
                return L"Spaces > " + leaf;
            }
        }

        if (!model.contentSource.empty())
        {
            return L"Spaces > " + model.contentSource;
        }

        return L"Spaces > " + model.title;
    }

    RECT GetDensitySliderRect(const RECT& clientRc, int titleBarHeight)
    {
        RECT sliderRc{};
        sliderRc.right = clientRc.right - 10;
        sliderRc.left = sliderRc.right - 130;
        sliderRc.top = titleBarHeight + 4;
        sliderRc.bottom = sliderRc.top + 14;
        return sliderRc;
    }

    int DensityStepFromPolicy(const EffectiveSpacePolicy& policy)
    {
        if (policy.iconTileSize <= 50)
        {
            return 0;
        }
        if (policy.iconTileSize >= 64)
        {
            return 2;
        }
        return 1;
    }

    int HitTestDensitySliderStep(const RECT& sliderRc, int x, int y)
    {
        if (x < sliderRc.left || x >= sliderRc.right || y < sliderRc.top || y >= sliderRc.bottom)
        {
            return -1;
        }

        const int width = (std::max)(1, static_cast<int>(sliderRc.right - sliderRc.left));
        const int localX = x - sliderRc.left;
        if (localX < width / 3)
        {
            return 0;
        }
        if (localX < (2 * width) / 3)
        {
            return 1;
        }
        return 2;
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
    m_introAnimActive = true;
    m_introAnimStartTick = GetTickCount();
    m_introAnimProgress = 0.0f;
    m_introAnimAlpha = 150;
    SetTimer(m_hwnd, kEntranceAnimTimerId, kEntranceAnimFrameMs, nullptr);

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

    RegisterHotKey(m_hwnd, kHotkeyCommandPalette, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'P');

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
        if (m_manager)
        {
            m_manager->EndLocalHoverPreview(m_model.id);
            m_manager->NotifySpaceDragState(m_model.id, false);
        }
        UnregisterHotKey(m_hwnd, kHotkeyCommandPalette);
        KillTimer(m_hwnd, kIdleStateTimerId);
        KillTimer(m_hwnd, kEntranceAnimTimerId);
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
        const bool visualChanged = ApplyIdleVisualState();
        InvalidateRect(m_hwnd, nullptr, visualChanged ? FALSE : FALSE);
    }
}

void SpaceWindow::SetItems(const std::vector<SpaceItem>& items)
{
    m_items = items;
    if (m_hwnd)
    {
        InvalidateRect(m_hwnd, nullptr, FALSE);
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

bool SpaceWindow::IsRolledUp() const
{
    return m_isRolledUp;
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
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;
    }

    switch (msg)
    {
    case WM_SETTINGCHANGE:
        if (m_manager && wParam == SPI_SETWORKAREA)
        {
            m_manager->HandleDesktopTopologyChanged(L"work_area_changed");
        }
        ApplyIdleVisualState();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;

    case WM_DISPLAYCHANGE:
        if (m_manager)
        {
            m_manager->HandleDesktopTopologyChanged(L"display_change");
        }
        ApplyIdleVisualState();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;

    case WM_THEMECHANGED:
        ApplyIdleVisualState();
        InvalidateRect(m_hwnd, nullptr, FALSE);
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
        m_titleBarHoverButton = kToolbarButtonNone;
        TraceDebug(L"Mouse leave", m_activeCorrelationId);
        if (ApplyIdleVisualState())
        {
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_TIMER:
        if (wParam == kIdleStateTimerId)
        {
            if (IsLoadingContentState(m_model.contentState))
            {
                m_loadingSpinnerFrame = (m_loadingSpinnerFrame + 1) % 4;
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }

            if (IsPopupInteractionActive())
            {
                return 0;
            }

            if (ApplyIdleVisualState())
            {
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            return 0;
        }
        if (wParam == kEntranceAnimTimerId)
        {
            const DWORD elapsed = GetTickCount() - m_introAnimStartTick;
            const float t = (kEntranceAnimDurationMs == 0)
                ? 1.0f
                : (std::min)(1.0f, static_cast<float>(elapsed) / static_cast<float>(kEntranceAnimDurationMs));
            // Smoothstep easing for subtle ease-in/out motion.
            m_introAnimProgress = t * t * (3.0f - (2.0f * t));
            m_introAnimAlpha = static_cast<BYTE>(150 + static_cast<int>(105.0f * m_introAnimProgress));

            ApplyIdleVisualState();
            InvalidateRect(m_hwnd, nullptr, FALSE);

            if (t >= 1.0f)
            {
                m_introAnimActive = false;
                m_introAnimProgress = 1.0f;
                m_introAnimAlpha = 255;
                KillTimer(m_hwnd, kEntranceAnimTimerId);
            }
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
                anchor.y = GetContentTop(titleBarHeight) + (m_selectedItem * kItemHeight) + (kItemHeight / 2);
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

    case WM_HOTKEY:
        if (wParam == kHotkeyCommandPalette)
        {
            ShowCommandPalette();
            return 0;
        }
        break;

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
        m_snapPreviewActive = false;
        if (m_manager)
        {
            m_manager->NotifySpaceDragState(m_model.id, false);
        }

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

    case WM_ENTERSIZEMOVE:
        m_snapPreviewActive = false;
        if (m_manager)
        {
            m_manager->NotifySpaceDragState(m_model.id, true);
        }
        return 0;

    case WM_MOVING:
    {
        auto* movingRect = reinterpret_cast<RECT*>(lParam);
        if (!movingRect)
        {
            return 0;
        }

        if (m_manager)
        {
            const int movingWidth = movingRect->right - movingRect->left;
            const int movingHeight = movingRect->bottom - movingRect->top;
            const auto preview = m_manager->QuerySnapPreview(m_model.id,
                                                             movingRect->left,
                                                             movingRect->top,
                                                             movingWidth,
                                                             movingHeight);
            m_snapPreviewActive = preview.active;
            m_snapPreviewVertical = preview.verticalSnap;
            m_snapTargetRect = preview.targetRect;

            if (preview.active)
            {
                movingRect->left = preview.snappedX;
                movingRect->top = preview.snappedY;
                movingRect->right = movingRect->left + movingWidth;
                movingRect->bottom = movingRect->top + movingHeight;
            }

            int clampedX = movingRect->left;
            int clampedY = movingRect->top;
            m_manager->ClampDragPositionToValidRegion(m_model.id,
                                                      movingWidth,
                                                      movingHeight,
                                                      clampedX,
                                                      clampedY);
            movingRect->left = clampedX;
            movingRect->top = clampedY;
            movingRect->right = movingRect->left + movingWidth;
            movingRect->bottom = movingRect->top + movingHeight;

            InvalidateRect(m_hwnd, nullptr, FALSE);
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
    const EffectiveSpacePolicy policy = ResolveEffectivePolicy();
    const bool idleTransparent = policy.transparentWhenNotHovered && !m_mouseInside && !m_isRolledUp;

    // Draw background
    HBRUSH bgBrush = CreateSolidBrush(palette.surfaceColor);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Draw title bar
    RECT titleRc = rc;
    titleRc.bottom = titleBarHeight;
    int titleOpacityPercent = 88;
    int idleOpacityPercent = 92;
    if (m_themePlatform)
    {
        titleOpacityPercent = m_themePlatform->GetSpaceTitleBarOpacityPercent();
        idleOpacityPercent = m_themePlatform->GetSpaceIdleOpacityPercent();
    }
    if (idleTransparent)
    {
        titleOpacityPercent = (std::min)(titleOpacityPercent, idleOpacityPercent);
    }
    const COLORREF translucentTitleColor = idleTransparent
        ? palette.surfaceColor
        : BlendColor(palette.surfaceColor,
                     palette.spaceTitleBarColor,
                     (titleOpacityPercent * 255) / 100);
    HBRUSH titleBrush = CreateSolidBrush(translucentTitleColor);
    FillRect(hdc, &titleRc, titleBrush);
    DeleteObject(titleBrush);

    // Apply fence shell style-specific styling
    std::wstring fenceStyle = L"window-frame";
    if (m_themePlatform)
    {
        ThemeResourceResolver* resolver = m_themePlatform->GetResourceResolver();
        if (resolver)
        {
            fenceStyle = resolver->GetSelectedFenceStyle();
        }
    }

    // Draw fence style-specific border/accent
    if (fenceStyle == L"card-container")
    {
        // For card-container style, add bottom border to title bar
        HPEN borderPen = CreatePen(PS_SOLID, 2, palette.borderColor);
        HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, borderPen));
        MoveToEx(hdc, titleRc.left, titleRc.bottom - 1, nullptr);
        LineTo(hdc, titleRc.right, titleRc.bottom - 1);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);
    }
    else if (fenceStyle == L"embedded")
    {
        // For embedded style, add subtle side accent bar
        RECT accentRc = titleRc;
        accentRc.right = accentRc.left + 3;
        HBRUSH accentBrush = CreateSolidBrush(palette.accentColor);
        FillRect(hdc, &accentRc, accentBrush);
        DeleteObject(accentBrush);
    }

    // Do not toggle WS_EX_LAYERED during paint; style thrash here causes visible flashing.

    // Draw title text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, palette.spaceTitleTextColor);
    RECT textRc = titleRc;
    textRc.left += 8;
    textRc.top += 4;
    std::wstring titleText = m_model.title;

    textRc.right -= 8;
    DrawTextW(hdc, titleText.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    const int introSlidePx = m_introAnimActive
        ? static_cast<int>((1.0f - m_introAnimProgress) * 10.0f)
        : 0;
    const int introColorAlpha = m_introAnimActive
        ? static_cast<int>(m_introAnimProgress * 255.0f)
        : 255;

    // Draw explorer header row (breadcrumbs + density slider)
    (void)introColorAlpha;

    // Draw items
    const bool textOnly = m_model.textOnlyMode;
    const int iconTileSize = policy.iconTileSize;
    const int contentLeft = 8;
    const int contentTop = GetContentTop(titleBarHeight) + introSlidePx;

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

            if (m_itemDragActive && m_itemDragTargetIndex == i && m_itemDragSourceIndex != i)
            {
                HPEN targetPen = CreatePen(PS_DASH, 1, palette.accentColor);
                HPEN oldTargetPen = reinterpret_cast<HPEN>(SelectObject(hdc, targetPen));
                HBRUSH oldTargetBrush = reinterpret_cast<HBRUSH>(SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)));
                Rectangle(hdc, itemRc.left + 2, itemRc.top + 2, itemRc.right - 2, itemRc.bottom - 2);
                SelectObject(hdc, oldTargetBrush);
                SelectObject(hdc, oldTargetPen);
                DeleteObject(targetPen);
            }
            itemY += kItemHeight;
        }
    }
    else
    {
        const int contentWidth = max(1, (rc.right - rc.left) - (contentLeft * 2));
        int cols = 1;
        int responsiveTileSize = iconTileSize;
        ResolveGridLayout(contentWidth, iconTileSize, cols, responsiveTileSize);

        for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
        {
            const auto& item = m_items[i];
            const int row = i / cols;
            const int col = i % cols;

            RECT tileRc{};
            tileRc.left = contentLeft + (col * responsiveTileSize);
            tileRc.top = contentTop + (row * responsiveTileSize);
            tileRc.right = tileRc.left + responsiveTileSize;
            tileRc.bottom = tileRc.top + responsiveTileSize;

            if (i == m_selectedItem)
            {
                HBRUSH hiBrush = CreateSolidBrush(palette.spaceItemHoverColor);
                FillRect(hdc, &tileRc, hiBrush);
                DeleteObject(hiBrush);

                HPEN ringPen = CreatePen(PS_SOLID, 2, palette.accentColor);
                HPEN oldRingPen = reinterpret_cast<HPEN>(SelectObject(hdc, ringPen));
                HBRUSH oldRingBrush = reinterpret_cast<HBRUSH>(SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)));
                Rectangle(hdc, tileRc.left + 1, tileRc.top + 1, tileRc.right - 1, tileRc.bottom - 1);
                SelectObject(hdc, oldRingBrush);
                SelectObject(hdc, oldRingPen);
                DeleteObject(ringPen);
            }

            if (item.iconIndex >= 0 && m_imageList != nullptr)
            {
                const int iconX = tileRc.left + ((responsiveTileSize - kIconGridSize) / 2);
                const int iconY = tileRc.top + ((responsiveTileSize - kIconGridSize) / 2) - (policy.labelsOnHover ? 6 : 0);
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

            if (m_itemDragActive && m_itemDragTargetIndex == i && m_itemDragSourceIndex != i)
            {
                HPEN targetPen = CreatePen(PS_DASH, 1, palette.accentColor);
                HPEN oldTargetPen = reinterpret_cast<HPEN>(SelectObject(hdc, targetPen));
                HBRUSH oldTargetBrush = reinterpret_cast<HBRUSH>(SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)));
                Rectangle(hdc, tileRc.left + 2, tileRc.top + 2, tileRc.right - 2, tileRc.bottom - 2);
                SelectObject(hdc, oldTargetBrush);
                SelectObject(hdc, oldTargetPen);
                DeleteObject(targetPen);
            }
        }
    }

    if (m_snapPreviewActive)
    {
        RECT guideRc{};
        GetClientRect(m_hwnd, &guideRc);
        HPEN guidePen = CreatePen(PS_DASH, 2, palette.accentColor);
        HPEN oldGuidePen = reinterpret_cast<HPEN>(SelectObject(hdc, guidePen));

        if (m_snapPreviewVertical)
        {
            MoveToEx(hdc, guideRc.left + 1, guideRc.top + 1, nullptr);
            LineTo(hdc, guideRc.left + 1, guideRc.bottom - 1);
            MoveToEx(hdc, guideRc.right - 2, guideRc.top + 1, nullptr);
            LineTo(hdc, guideRc.right - 2, guideRc.bottom - 1);
        }
        else
        {
            MoveToEx(hdc, guideRc.left + 1, guideRc.top + 1, nullptr);
            LineTo(hdc, guideRc.right - 1, guideRc.top + 1);
            MoveToEx(hdc, guideRc.left + 1, guideRc.bottom - 2, nullptr);
            LineTo(hdc, guideRc.right - 1, guideRc.bottom - 2);
        }

        SelectObject(hdc, oldGuidePen);
        DeleteObject(guidePen);
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
        return;
    }

    const int itemIndex = GetItemAtPosition(x, y);
    if (itemIndex >= 0)
    {
        m_itemDragPending = true;
        m_itemDragActive = false;
        m_itemDragSourceIndex = itemIndex;
        m_itemDragTargetIndex = itemIndex;
        m_dragStart.x = x;
        m_dragStart.y = y;
        SetCapture(m_hwnd);
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

    m_titleBarHoverButton = kToolbarButtonNone;

    if ((m_itemDragPending || m_itemDragActive) && (flags & MK_LBUTTON) == 0)
    {
        m_itemDragPending = false;
        m_itemDragActive = false;
        m_itemDragSourceIndex = -1;
        m_itemDragTargetIndex = -1;
        ReleaseCapture();
    }

    if (m_itemDragPending && !m_itemDragActive)
    {
        const int dx = std::abs(x - m_dragStart.x);
        const int dy = std::abs(y - m_dragStart.y);
        if (dx >= 4 || dy >= 4)
        {
            m_itemDragActive = true;
        }
    }

    if (m_itemDragActive)
    {
        int hoverIndex = GetItemAtPosition(x, y);
        if (hoverIndex < 0 && !m_items.empty())
        {
            hoverIndex = static_cast<int>(m_items.size()) - 1;
        }

        if (hoverIndex != m_itemDragTargetIndex)
        {
            m_itemDragTargetIndex = hoverIndex;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    }

    // Track item under cursor for highlight (when not dragging reorder)
    const int item = GetItemAtPosition(x, y);
    if (!m_itemDragActive && item != m_selectedItem)
    {
        m_selectedItem = item;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void SpaceWindow::OnLButtonUp()
{
    if (m_itemDragActive)
    {
        const bool moved = MoveItemInVector(m_items, m_itemDragSourceIndex, m_itemDragTargetIndex);
        if (moved)
        {
            m_selectedItem = m_itemDragTargetIndex;
            TraceDebug(L"Reordered item from " + std::to_wstring(m_itemDragSourceIndex) +
                       L" to " + std::to_wstring(m_itemDragTargetIndex));
        }
    }

    m_itemDragPending = false;
    m_itemDragActive = false;
    m_itemDragSourceIndex = -1;
    m_itemDragTargetIndex = -1;
    ReleaseCapture();
    InvalidateRect(m_hwnd, nullptr, FALSE);
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

    int menuWidthPx = 220;
    int menuRowHeightPx = 28;
    if (m_themePlatform)
    {
        menuWidthPx = m_themePlatform->GetTrayMenuMinWidthPx();
        menuRowHeightPx = m_themePlatform->GetTrayMenuRowHeightPx();

        ThemeResourceResolver* resolver = m_themePlatform->GetResourceResolver();
        if (resolver)
        {
            const std::wstring menuStyle = resolver->GetSelectedMenuStyle();
            if (menuStyle == L"compact")
            {
                menuWidthPx = (std::max)(180, menuWidthPx - 20);
                menuRowHeightPx = (std::max)(24, menuRowHeightPx - 4);
            }
            else if (menuStyle == L"hierarchical")
            {
                menuWidthPx = (std::max)(240, menuWidthPx + 20);
                menuRowHeightPx = (std::max)(30, menuRowHeightPx + 2);
            }
        }
    }

    auto appendSeparator = [&]() {
        AppendMenuW(menu, MF_OWNERDRAW | MF_DISABLED, decorationId, nullptr);
        Win32Helpers::PopupMenuItemVisual visual{L"", L"", L"", true, false};
        visual.preferredWidthPx = menuWidthPx;
        visual.preferredHeightPx = menuRowHeightPx;
        m_menuVisuals.emplace(decorationId, visual);
        ++decorationId;
    };

    auto appendItem = [&](const std::wstring& text, UINT commandId) {
        AppendMenuW(menu, MF_OWNERDRAW | MF_STRING, commandId, nullptr);
        Win32Helpers::PopupMenuItemVisual visual{text, L"", L"", false, true};
        visual.preferredWidthPx = menuWidthPx;
        visual.preferredHeightPx = menuRowHeightPx;
        m_menuVisuals.emplace(commandId, visual);
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
            Win32Helpers::PopupMenuItemVisual visual{contribution.title, L"", L"", false, true};
            visual.preferredWidthPx = menuWidthPx;
            visual.preferredHeightPx = menuRowHeightPx;
            m_menuVisuals.emplace(pluginCommandId, visual);
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
        appendItem(L"Set Stack Group...", kCmdSetStackGroup);
        appendItem(L"Set Parent Container...", kCmdSetParentContainer);
        appendItem(L"Layout: Free", kCmdSetLayoutModeFree);
        appendItem(L"Layout: Stacked", kCmdSetLayoutModeStacked);
        appendItem(L"Layout: Contained", kCmdSetLayoutModeContained);
        appendItem(L"Text Only Mode", 1004);
        appendItem(L"Roll Up When Not Hovered", 1005);
        appendItem(L"Transparent When Not Hovered", 1006);

        const UINT textState = m_model.textOnlyMode ? MF_CHECKED : MF_UNCHECKED;
        const UINT rollupState = effectivePolicy.rollupWhenNotHovered ? MF_CHECKED : MF_UNCHECKED;
        const UINT transparentState = effectivePolicy.transparentWhenNotHovered ? MF_CHECKED : MF_UNCHECKED;
        CheckMenuItem(menu, 1004, MF_BYCOMMAND | textState);
        CheckMenuItem(menu, 1005, MF_BYCOMMAND | rollupState);
        CheckMenuItem(menu, 1006, MF_BYCOMMAND | transparentState);
        CheckMenuItem(menu, kCmdSetLayoutModeFree, MF_BYCOMMAND | (m_model.layoutMode == L"free" ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(menu, kCmdSetLayoutModeStacked, MF_BYCOMMAND | (m_model.layoutMode == L"stacked" ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(menu, kCmdSetLayoutModeContained, MF_BYCOMMAND | (m_model.layoutMode == L"contained" ? MF_CHECKED : MF_UNCHECKED));

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

    case kCmdSetStackGroup:
        if (m_manager)
        {
            std::wstring groupValue;
            if (Win32Helpers::PromptTextInput(m_hwnd,
                                              L"Stack Group",
                                              L"Group ID (empty clears):",
                                              m_model.groupId,
                                              groupValue))
            {
                m_manager->SetSpaceGroupId(m_model.id, TrimWhitespace(groupValue));
            }
        }
        break;

    case kCmdSetParentContainer:
        if (m_manager)
        {
            std::wstring parentValue;
            if (Win32Helpers::PromptTextInput(m_hwnd,
                                              L"Parent Container",
                                              L"Parent fence ID (empty clears):",
                                              m_model.parentFenceId,
                                              parentValue))
            {
                m_manager->SetSpaceParentFence(m_model.id, TrimWhitespace(parentValue));
            }
        }
        break;

    case kCmdSetLayoutModeFree:
        if (m_manager)
        {
            m_manager->SetSpaceLayoutMode(m_model.id, L"free");
        }
        break;

    case kCmdSetLayoutModeStacked:
        if (m_manager)
        {
            m_manager->SetSpaceLayoutMode(m_model.id, L"stacked");
        }
        break;

    case kCmdSetLayoutModeContained:
        if (m_manager)
        {
            m_manager->SetSpaceLayoutMode(m_model.id, L"contained");
        }
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
        int itemY = GetContentTop(titleBarHeight);
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
    const int contentTop = GetContentTop(titleBarHeight);
    const int iconTileSize = policy.iconTileSize;
    const int contentWidth = max(1, (rc.right - rc.left) - (contentLeft * 2));
    int cols = 1;
    int responsiveTileSize = iconTileSize;
    ResolveGridLayout(contentWidth, iconTileSize, cols, responsiveTileSize);

    const int localX = x - contentLeft;
    const int localY = y - contentTop;
    if (localX < 0 || localY < 0)
    {
        return -1;
    }

    const int col = localX / responsiveTileSize;
    const int row = localY / responsiveTileSize;
    if (col < 0 || col >= cols)
    {
        return -1;
    }

    const int index = row * cols + col;
    if (index < 0 || index >= static_cast<int>(m_items.size()))
    {
        return -1;
    }

    const int insideTileX = localX % responsiveTileSize;
    const int insideTileY = localY % responsiveTileSize;
    if (insideTileX < 0 || insideTileY < 0 || insideTileX >= responsiveTileSize || insideTileY >= responsiveTileSize)
    {
        return -1;
    }

    return index;
}

bool SpaceWindow::ApplyIdleVisualState()
{
    if (!m_hwnd)
    {
        return false;
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
        if (m_manager)
        {
            m_manager->EndLocalHoverPreview(m_model.id);
        }

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

        if (m_manager)
        {
            RECT previewRc = rc;
            previewRc.bottom = previewRc.top + restoredHeight;
            m_manager->BeginLocalHoverPreview(m_model.id, previewRc);
        }

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
    const bool wantsLayered = shouldBeTransparent && idleOpacityPercent < 100;
    BYTE desiredAlpha = wantsLayered
        ? static_cast<BYTE>((idleOpacityPercent * 255) / 100)
        : static_cast<BYTE>(255);

    if (m_introAnimActive)
    {
        desiredAlpha = (std::min)(desiredAlpha, m_introAnimAlpha);
    }

    if (!m_alphaInitialized)
    {
        m_currentAlpha = desiredAlpha;
        m_targetAlpha = desiredAlpha;
        m_alphaInitialized = true;
    }
    else
    {
        m_targetAlpha = desiredAlpha;
    }

    int motionDurationMs = 220;
    if (m_themePlatform)
    {
        ThemeResourceResolver* resolver = m_themePlatform->GetResourceResolver();
        if (resolver)
        {
            motionDurationMs = resolver->GetMotionDurationMs(resolver->GetSelectedMotionPreset(), 220);
        }
    }

    if (motionDurationMs <= 0)
    {
        m_currentAlpha = m_targetAlpha;
    }
    else
    {
        const int steps = (std::max)(1, motionDurationMs / static_cast<int>(kIdleStateTimerMs));
        const int delta = static_cast<int>(m_targetAlpha) - static_cast<int>(m_currentAlpha);
        if (delta != 0)
        {
            const int stepAbs = (std::max)(1, (std::abs(delta) + steps - 1) / steps);
            if (delta > 0)
            {
                m_currentAlpha = static_cast<BYTE>((std::min)(255, static_cast<int>(m_currentAlpha) + stepAbs));
                if (m_currentAlpha > m_targetAlpha)
                {
                    m_currentAlpha = m_targetAlpha;
                }
            }
            else
            {
                m_currentAlpha = static_cast<BYTE>((std::max)(0, static_cast<int>(m_currentAlpha) - stepAbs));
                if (m_currentAlpha < m_targetAlpha)
                {
                    m_currentAlpha = m_targetAlpha;
                }
            }
        }
    }

    // Use layered mode only while translucency is active or a fade is in flight.
    const bool alphaInFlight = m_currentAlpha != m_targetAlpha;
    const bool needsLayeredNow = wantsLayered || alphaInFlight || m_currentAlpha < 255;
    if (needsLayeredNow)
    {
        if ((exStyle & WS_EX_LAYERED) == 0)
        {
            SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
            exStyle |= WS_EX_LAYERED;
        }
        SetLayeredWindowAttributes(m_hwnd, 0, m_currentAlpha, LWA_ALPHA);
    }
    else if ((exStyle & WS_EX_LAYERED) != 0)
    {
        // Switch back to fully opaque non-layered once transition completes.
        SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
        SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
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

    const bool alphaChanged = (m_currentAlpha != m_targetAlpha) || (m_targetAlpha != desiredAlpha);
    return hoverChanged || rollChanged || transparencyChanged || alphaChanged;
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

void SpaceWindow::ShowCommandPalette()
{
    struct PaletteCommand
    {
        std::wstring label;
        std::function<void()> execute;
    };

    std::vector<PaletteCommand> commands;
    commands.push_back(PaletteCommand{L"New Space", [this]() {
        if (m_manager)
        {
            RECT rc{};
            GetWindowRect(m_hwnd, &rc);
            m_manager->CreateSpaceAt(rc.left + 28, rc.top + 36);
        }
    }});
    commands.push_back(PaletteCommand{L"Rename Space", [this]() {
        if (!m_manager)
        {
            return;
        }

        std::wstring newTitle;
        if (Win32Helpers::PromptTextInput(m_hwnd, L"Rename Space", L"Space title:", m_model.title, newTitle))
        {
            newTitle = TrimWhitespace(newTitle);
            if (!newTitle.empty() && newTitle.size() <= 128)
            {
                m_manager->RenameSpace(m_model.id, newTitle);
            }
        }
    }});
    commands.push_back(PaletteCommand{L"Space Settings", [this]() { ShowSettingsPanel(); }});
    commands.push_back(PaletteCommand{L"Toggle Text Only Mode", [this]() {
        if (m_manager)
        {
            m_manager->SetSpaceTextOnlyMode(m_model.id, !m_model.textOnlyMode);
        }
    }});
    commands.push_back(PaletteCommand{L"Toggle Roll Up", [this]() {
        if (m_manager)
        {
            m_manager->SetSpaceRollupWhenNotHovered(m_model.id, !m_model.rollupWhenNotHovered, NewCorrelationId(L"palette_rollup"));
        }
    }});
    commands.push_back(PaletteCommand{L"Toggle Transparency", [this]() {
        if (m_manager)
        {
            m_manager->SetSpaceTransparentWhenNotHovered(m_model.id, !m_model.transparentWhenNotHovered, NewCorrelationId(L"palette_transparency"));
        }
    }});
    commands.push_back(PaletteCommand{L"Density Compact", [this]() {
        if (m_manager)
        {
            m_manager->SetSpaceThemePolicyInheritance(m_model.id, false);
            m_manager->SetSpaceIconSpacingPreset(m_model.id, L"compact");
        }
    }});
    commands.push_back(PaletteCommand{L"Density Comfortable", [this]() {
        if (m_manager)
        {
            m_manager->SetSpaceThemePolicyInheritance(m_model.id, false);
            m_manager->SetSpaceIconSpacingPreset(m_model.id, L"comfortable");
        }
    }});
    commands.push_back(PaletteCommand{L"Density Spacious", [this]() {
        if (m_manager)
        {
            m_manager->SetSpaceThemePolicyInheritance(m_model.id, false);
            m_manager->SetSpaceIconSpacingPreset(m_model.id, L"spacious");
        }
    }});
    commands.push_back(PaletteCommand{L"Refresh Space", [this]() {
        if (m_manager)
        {
            m_manager->RefreshSpace(m_model.id);
        }
    }});

    std::wstring query;
    if (!Win32Helpers::PromptTextInput(
            m_hwnd,
            L"Command Palette",
            L"Type command (fuzzy):",
            L"",
            query))
    {
        return;
    }

    query = TrimWhitespace(query);

    int bestIndex = -1;
    int bestScore = -1;
    for (int i = 0; i < static_cast<int>(commands.size()); ++i)
    {
        const int score = FuzzyScore(query, commands[static_cast<size_t>(i)].label);
        if (score > bestScore)
        {
            bestScore = score;
            bestIndex = i;
        }
    }

    if (bestIndex < 0)
    {
        Win32Helpers::ShowUserWarning(m_hwnd, L"Command Palette", L"No matching command found.");
        return;
    }

    TraceDebug(L"Command palette executed: " + commands[static_cast<size_t>(bestIndex)].label,
               NewCorrelationId(L"palette"));
    commands[static_cast<size_t>(bestIndex)].execute();
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
                        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                        240, 220, 90, 28, hwnd,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCtlClose)),
                        GetModuleHandleW(nullptr), nullptr);

        CreateWindowExW(0, L"BUTTON", L"Apply to all spaces",
                        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
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
            if (SendMessageW(GetDlgItem(hwnd, kCtlInheritThemePolicy), BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                SendMessageW(GetDlgItem(hwnd, kCtlInheritThemePolicy), BM_SETCHECK, BST_UNCHECKED, 0);
                m_manager->SetSpaceThemePolicyInheritance(m_model.id, false);
            }
            const bool enabled = (SendMessageW(GetDlgItem(hwnd, kCtlRollup), BM_GETCHECK, 0, 0) == BST_CHECKED);
            const std::wstring cid = NewCorrelationId(L"rollup_setting");
            TraceDebug(L"Settings panel roll-up toggle", cid);
            m_manager->SetSpaceRollupWhenNotHovered(m_model.id, enabled, cid);
            return 0;
        }

        if (id == kCtlTransparent && code == BN_CLICKED)
        {
            if (SendMessageW(GetDlgItem(hwnd, kCtlInheritThemePolicy), BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                SendMessageW(GetDlgItem(hwnd, kCtlInheritThemePolicy), BM_SETCHECK, BST_UNCHECKED, 0);
                m_manager->SetSpaceThemePolicyInheritance(m_model.id, false);
            }
            const bool enabled = (SendMessageW(GetDlgItem(hwnd, kCtlTransparent), BM_GETCHECK, 0, 0) == BST_CHECKED);
            const std::wstring cid = NewCorrelationId(L"transparency_setting");
            TraceDebug(L"Settings panel transparency toggle", cid);
            m_manager->SetSpaceTransparentWhenNotHovered(m_model.id, enabled, cid);
            return 0;
        }

        if (id == kCtlLabelsOnHover && code == BN_CLICKED)
        {
            if (SendMessageW(GetDlgItem(hwnd, kCtlInheritThemePolicy), BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                SendMessageW(GetDlgItem(hwnd, kCtlInheritThemePolicy), BM_SETCHECK, BST_UNCHECKED, 0);
                m_manager->SetSpaceThemePolicyInheritance(m_model.id, false);
            }
            const bool enabled = (SendMessageW(GetDlgItem(hwnd, kCtlLabelsOnHover), BM_GETCHECK, 0, 0) == BST_CHECKED);
            m_manager->SetSpaceLabelsOnHover(m_model.id, enabled);
            return 0;
        }

        if (id == kCtlSpacingPreset && code == CBN_SELCHANGE)
        {
            if (SendMessageW(GetDlgItem(hwnd, kCtlInheritThemePolicy), BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                SendMessageW(GetDlgItem(hwnd, kCtlInheritThemePolicy), BM_SETCHECK, BST_UNCHECKED, 0);
                m_manager->SetSpaceThemePolicyInheritance(m_model.id, false);
            }
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

        if (drawInfo && drawInfo->CtlType == ODT_BUTTON &&
            IsSettingsActionButtonId(static_cast<int>(drawInfo->CtlID)))
        {
            const ThemePalette palette = m_themePlatform ? m_themePlatform->BuildPalette() : ThemePalette{};
            std::wstring buttonFamily = L"compact";
            if (m_themePlatform)
            {
                ThemeResourceResolver* resolver = m_themePlatform->GetResourceResolver();
                if (resolver)
                {
                    buttonFamily = resolver->GetSelectedButtonFamily();
                }
            }

            DrawSettingsActionButton(drawInfo,
                                     palette.windowColor,
                                     palette.surfaceColor,
                                     palette.textColor,
                                     palette.accentColor,
                                     buttonFamily);
            return TRUE;
        }
        break;
    }
    case WM_ERASEBKGND:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const ThemePalette palette = m_themePlatform ? m_themePlatform->BuildPalette() : ThemePalette{};
        HBRUSH bg = CreateSolidBrush(palette.windowColor);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        HPEN dividerPen = CreatePen(PS_SOLID, 1, BlendColor(palette.windowColor, palette.textColor, 26));
        HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, dividerPen));
        MoveToEx(hdc, 16, 206, nullptr);
        LineTo(hdc, rc.right - 16, 206);
        SelectObject(hdc, oldPen);
        DeleteObject(dividerPen);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        const ThemePalette palette = m_themePlatform ? m_themePlatform->BuildPalette() : ThemePalette{};
        SetTextColor(hdc, palette.textColor);
        SetBkColor(hdc, palette.windowColor);
        static HBRUSH sPanelBrush = nullptr;
        if (sPanelBrush)
        {
            DeleteObject(sPanelBrush);
            sPanelBrush = nullptr;
        }
        sPanelBrush = CreateSolidBrush(palette.windowColor);
        return reinterpret_cast<INT_PTR>(sPanelBrush);
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
