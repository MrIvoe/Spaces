#include "ui/SwitchControl.h"

#include <algorithm>

namespace
{
    constexpr const wchar_t* kSwitchControlClass = L"SimpleSpaces_SwitchControl";

    COLORREF BlendColor(COLORREF from, COLORREF to, int alpha)
    {
        alpha = (alpha < 0) ? 0 : ((alpha > 255) ? 255 : alpha);
        const int inv = 255 - alpha;
        const BYTE red = static_cast<BYTE>(((GetRValue(from) * inv) + (GetRValue(to) * alpha)) / 255);
        const BYTE green = static_cast<BYTE>(((GetGValue(from) * inv) + (GetGValue(to) * alpha)) / 255);
        const BYTE blue = static_cast<BYTE>(((GetBValue(from) * inv) + (GetBValue(to) * alpha)) / 255);
        return RGB(red, green, blue);
    }

    bool IsCyberTheme(COLORREF windowColor)
    {
        return GetRValue(windowColor) == 9 && GetGValue(windowColor) == 11 && GetBValue(windowColor) == 17;
    }
}

bool SwitchControl::Register(HINSTANCE hInstance)
{
    WNDCLASSW wc{};
    wc.lpfnWndProc = SwitchControl::WndProcStatic;
    wc.hInstance = hInstance;
    wc.lpszClassName = kSwitchControlClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
    wc.hbrBackground = nullptr;

    if (!RegisterClassW(&wc))
    {
        return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }

    return true;
}

HWND SwitchControl::Create(HWND parent,
                           int x,
                           int y,
                           int width,
                           int height,
                           int controlId,
                           bool checked,
                           const Colors& colors)
{
    CreateParams params;
    params.checked = checked;
    params.colors = colors;

    return CreateWindowExW(
        0,
        kSwitchControlClass,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        x,
        y,
        width,
        height,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
        GetModuleHandleW(nullptr),
        &params);
}

void SwitchControl::SetChecked(HWND hwnd, bool checked)
{
    if (hwnd)
    {
        SendMessageW(hwnd, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

bool SwitchControl::GetChecked(HWND hwnd)
{
    return hwnd && SendMessageW(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void SwitchControl::SetColors(HWND hwnd, const Colors& colors)
{
    if (hwnd)
    {
        SendMessageW(hwnd, kMsgSetColors, 0, reinterpret_cast<LPARAM>(&colors));
    }
}

LRESULT CALLBACK SwitchControl::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    State* state = reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_NCCREATE)
    {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* params = reinterpret_cast<CreateParams*>(create->lpCreateParams);
        auto* newState = new State();
        if (params)
        {
            newState->checked = params->checked;
            newState->colors = params->colors;
        }
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(newState));
        return TRUE;
    }

    if (!state)
    {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg)
    {
    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        delete state;
        return 0;

    case WM_ENABLE:
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_SETFOCUS:
        state->focused = true;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_KILLFOCUS:
        state->focused = false;
        state->pressed = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_MOUSEMOVE:
    {
        if (!state->hovered)
        {
            state->hovered = true;
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        state->hovered = false;
        state->pressed = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN:
        if (IsWindowEnabled(hwnd))
        {
            state->pressed = true;
            SetCapture(hwnd);
            SetFocus(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (state->pressed)
        {
            state->pressed = false;
            ReleaseCapture();

            POINT pt{ static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)) };
            RECT client{};
            GetClientRect(hwnd, &client);
            if (PtInRect(&client, pt) && IsWindowEnabled(hwnd))
            {
                Toggle(hwnd, state);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_KEYDOWN:
        if ((wParam == VK_SPACE || wParam == VK_RETURN) && IsWindowEnabled(hwnd))
        {
            Toggle(hwnd, state);
            return 0;
        }
        break;

    case BM_GETCHECK:
        return state->checked ? BST_CHECKED : BST_UNCHECKED;

    case BM_SETCHECK:
    {
        const bool checked = (wParam == BST_CHECKED);
        if (state->checked != checked)
        {
            state->checked = checked;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case kMsgSetColors:
        if (lParam)
        {
            state->colors = *reinterpret_cast<const Colors*>(lParam);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;

    case WM_GETDLGCODE:
        return DLGC_BUTTON;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);
        Paint(hwnd, hdc, client, *state);
        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void SwitchControl::Toggle(HWND hwnd, State* state)
{
    state->checked = !state->checked;
    InvalidateRect(hwnd, nullptr, FALSE);

    const int controlId = GetDlgCtrlID(hwnd);
    SendMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(controlId, BN_CLICKED), reinterpret_cast<LPARAM>(hwnd));
}

void SwitchControl::Paint(HWND hwnd, HDC hdc, const RECT& clientRect, const State& state)
{
    const bool enabled = IsWindowEnabled(hwnd) != FALSE;
    const bool cyber = IsCyberTheme(state.colors.window);

    HBRUSH bgBrush = CreateSolidBrush(state.colors.surface);
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    const int width = clientRect.right - clientRect.left;
    const int height = clientRect.bottom - clientRect.top;
    const int trackWidth = (std::max)(40, (std::min)(54, width - 4));
    const int trackHeight = (std::max)(20, (std::min)(30, height - 6));

    RECT track{};
    track.left = clientRect.left + ((width - trackWidth) / 2);
    track.top = clientRect.top + ((height - trackHeight) / 2);
    track.right = track.left + trackWidth;
    track.bottom = track.top + trackHeight;

    COLORREF offTrack = BlendColor(state.colors.surface, state.colors.text, 44);
    COLORREF onTrack = BlendColor(state.colors.accent, RGB(255, 255, 255), state.pressed ? 12 : 0);
    COLORREF trackColor = state.checked ? onTrack : offTrack;
    if (!enabled)
    {
        trackColor = BlendColor(state.colors.surface, trackColor, 132);
    }
    else if (state.hovered && !state.checked)
    {
        trackColor = BlendColor(trackColor, state.colors.accent, 24);
    }

    const COLORREF trackBorder = cyber
        ? BlendColor(state.colors.accent, state.colors.window, state.checked ? 28 : 90)
        : BlendColor(trackColor, state.colors.text, 30);
    HPEN trackPen = CreatePen(PS_SOLID, 1, trackBorder);
    HBRUSH trackBrush = CreateSolidBrush(trackColor);
    HGDIOBJ oldPen = SelectObject(hdc, trackPen);
    HGDIOBJ oldBrush = SelectObject(hdc, trackBrush);
    if (cyber)
    {
        Rectangle(hdc, track.left, track.top, track.right, track.bottom);
    }
    else
    {
        RoundRect(hdc, track.left, track.top, track.right, track.bottom, trackHeight, trackHeight);
    }
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(trackBrush);
    DeleteObject(trackPen);

    if (cyber)
    {
        HPEN glowPen = CreatePen(PS_SOLID, 1, state.checked ? state.colors.accent : BlendColor(state.colors.accent, state.colors.window, 80));
        HGDIOBJ oldGlowPen = SelectObject(hdc, glowPen);
        MoveToEx(hdc, track.left + 1, track.top + 1, nullptr);
        LineTo(hdc, track.right - 1, track.top + 1);
        SelectObject(hdc, oldGlowPen);
        DeleteObject(glowPen);
    }

    const int thumbSize = trackHeight - 6;
    int thumbLeft = state.checked ? (track.right - thumbSize - 3) : (track.left + 3);
    if (state.pressed)
    {
        thumbLeft += state.checked ? -1 : 1;
    }

    RECT thumb{};
    thumb.left = thumbLeft;
    thumb.top = track.top + 3;
    thumb.right = thumb.left + thumbSize;
    thumb.bottom = thumb.top + thumbSize;

    COLORREF thumbColor = enabled
        ? (cyber && state.checked ? state.colors.accent : RGB(255, 255, 255))
        : BlendColor(state.colors.window, state.colors.surface, 96);
    HPEN thumbPen = CreatePen(PS_SOLID, 1, cyber
        ? BlendColor(thumbColor, state.colors.window, 110)
        : BlendColor(thumbColor, RGB(0, 0, 0), 30));
    HBRUSH thumbBrush = CreateSolidBrush(thumbColor);
    oldPen = SelectObject(hdc, thumbPen);
    oldBrush = SelectObject(hdc, thumbBrush);
    if (cyber)
    {
        Rectangle(hdc, thumb.left, thumb.top, thumb.right, thumb.bottom);
    }
    else
    {
        Ellipse(hdc, thumb.left, thumb.top, thumb.right, thumb.bottom);
    }
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(thumbBrush);
    DeleteObject(thumbPen);

    if (state.focused)
    {
        RECT focusRect = track;
        InflateRect(&focusRect, 2, 2);
        if (cyber)
        {
            HPEN focusPen = CreatePen(PS_SOLID, 1, state.colors.accent);
            HGDIOBJ oldFocusPen = SelectObject(hdc, focusPen);
            HGDIOBJ oldFocusBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(hdc, focusRect.left, focusRect.top, focusRect.right, focusRect.bottom);
            SelectObject(hdc, oldFocusBrush);
            SelectObject(hdc, oldFocusPen);
            DeleteObject(focusPen);
        }
        else
        {
            DrawFocusRect(hdc, &focusRect);
        }
    }
}