#pragma once

#include <windows.h>

class SwitchControl
{
public:
    struct Colors
    {
        COLORREF surface = RGB(255, 255, 255);
        COLORREF accent = RGB(70, 120, 220);
        COLORREF text = RGB(0, 0, 0);
        COLORREF window = RGB(255, 255, 255);
    };

    static bool Register(HINSTANCE hInstance);
    static HWND Create(HWND parent,
                       int x,
                       int y,
                       int width,
                       int height,
                       int controlId,
                       bool checked,
                       const Colors& colors);

    static void SetChecked(HWND hwnd, bool checked);
    static bool GetChecked(HWND hwnd);
    static void SetColors(HWND hwnd, const Colors& colors);

private:
    struct CreateParams
    {
        bool checked = false;
        Colors colors;
    };

    struct State
    {
        bool checked = false;
        bool hovered = false;
        bool pressed = false;
        bool focused = false;
        Colors colors;
    };

    static constexpr UINT kMsgSetColors = WM_USER + 1;

    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static void Toggle(HWND hwnd, State* state);
    static void Paint(HWND hwnd, HDC hdc, const RECT& clientRect, const State& state);
};