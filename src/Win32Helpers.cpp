#include "Win32Helpers.h"

#include <commdlg.h>
#include <shlobj.h>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>

namespace Win32Helpers
{
    namespace
    {
        std::mutex g_logMutex;
        constexpr int kPromptEditId = 6001;
        constexpr int kPromptOkId = 6002;
        constexpr int kPromptCancelId = 6003;

        struct TextPromptState
        {
            std::wstring title;
            std::wstring label;
            std::wstring initialValue;
            std::wstring resultValue;
            bool accepted = false;
            HWND editHwnd = nullptr;
        };

        LRESULT CALLBACK TextPromptWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            TextPromptState* state = reinterpret_cast<TextPromptState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

            if (msg == WM_NCCREATE)
            {
                auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
                state = reinterpret_cast<TextPromptState*>(create->lpCreateParams);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
                return TRUE;
            }

            if (!state)
            {
                return DefWindowProcW(hwnd, msg, wParam, lParam);
            }

            switch (msg)
            {
            case WM_CREATE:
            {
                CreateWindowExW(0, L"STATIC", state->label.c_str(),
                                WS_CHILD | WS_VISIBLE,
                                12, 12, 320, 20,
                                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

                state->editHwnd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", state->initialValue.c_str(),
                                                  WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                                  12, 36, 320, 24,
                                                  hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPromptEditId)),
                                                  GetModuleHandleW(nullptr), nullptr);

                CreateWindowExW(0, L"BUTTON", L"OK",
                                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                176, 72, 74, 26,
                                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPromptOkId)),
                                GetModuleHandleW(nullptr), nullptr);

                CreateWindowExW(0, L"BUTTON", L"Cancel",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                258, 72, 74, 26,
                                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPromptCancelId)),
                                GetModuleHandleW(nullptr), nullptr);

                if (state->editHwnd)
                {
                    SendMessageW(state->editHwnd, EM_SETSEL, 0, -1);
                    SetFocus(state->editHwnd);
                }
                return 0;
            }
            case WM_COMMAND:
            {
                const int id = LOWORD(wParam);
                const int code = HIWORD(wParam);

                if (id == kPromptOkId && code == BN_CLICKED)
                {
                    if (state->editHwnd)
                    {
                        const int textLen = GetWindowTextLengthW(state->editHwnd);
                        std::wstring value(static_cast<size_t>(max(textLen, 0)), L'\0');
                        if (textLen > 0)
                        {
                            std::vector<wchar_t> buffer(static_cast<size_t>(textLen) + 1, L'\0');
                            GetWindowTextW(state->editHwnd, buffer.data(), textLen + 1);
                            value.assign(buffer.data());
                        }
                        state->resultValue = value;
                    }
                    state->accepted = true;
                    DestroyWindow(hwnd);
                    return 0;
                }

                if (id == kPromptCancelId && code == BN_CLICKED)
                {
                    state->accepted = false;
                    DestroyWindow(hwnd);
                    return 0;
                }
                break;
            }
            case WM_CLOSE:
                state->accepted = false;
                DestroyWindow(hwnd);
                return 0;
            default:
                break;
            }

            return DefWindowProcW(hwnd, msg, wParam, lParam);
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

        void WriteLogLine(const wchar_t* level, const std::wstring& message)
        {
            std::error_code ec;
            std::filesystem::create_directories(GetAppDataRoot(), ec);

            const auto logPath = GetDebugLogPath();
            std::wofstream log(logPath, std::ios::app);
            if (!log.is_open())
            {
                return;
            }

            SYSTEMTIME st{};
            GetLocalTime(&st);
            log << L"[" << st.wYear << L"-"
                << st.wMonth << L"-"
                << st.wDay << L" "
                << st.wHour << L":"
                << st.wMinute << L":"
                << st.wSecond << L"] "
                << level << L": "
                << message << L"\n";
        }
    }

    std::wstring GetAppDataPath()
    {
        wchar_t path[MAX_PATH]{};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path)))
        {
            return path;
        }
        return L"";
    }

    std::wstring GetLocalAppDataPath()
    {
        wchar_t path[MAX_PATH]{};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path)))
        {
            return path;
        }
        return L"";
    }

    std::filesystem::path GetAppDataRoot()
    {
        return std::filesystem::path(GetLocalAppDataPath()) / L"SimpleFences";
    }

    std::filesystem::path GetFencesRoot()
    {
        return GetAppDataRoot() / L"Fences";
    }

    std::filesystem::path GetConfigPath()
    {
        return GetAppDataRoot() / L"config.json";
    }

    std::filesystem::path GetDebugLogPath()
    {
        return GetAppDataRoot() / L"debug.log";
    }

    bool ReplaceFileAtomically(const std::filesystem::path& tempPath, const std::filesystem::path& targetPath)
    {
        if (MoveFileExW(
                tempPath.c_str(),
                targetPath.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            return true;
        }

        const DWORD error = GetLastError();
        LogError(
            L"Atomic replace failed from '" + tempPath.wstring() +
            L"' to '" + targetPath.wstring() +
            L"' error=" + std::to_wstring(error));
        return false;
    }

    void ShowUserWarning(HWND owner, const std::wstring& title, const std::wstring& message)
    {
        MessageBoxW(owner, message.c_str(), title.c_str(), MB_OK | MB_ICONWARNING);
    }

    bool PromptTextInput(HWND owner,
                         const std::wstring& title,
                         const std::wstring& label,
                         const std::wstring& initialValue,
                         std::wstring& resultValue)
    {
        static const wchar_t* kClassName = L"SimpleFences_TextPrompt";
        static bool registered = false;

        if (!registered)
        {
            WNDCLASSW wc{};
            wc.lpfnWndProc = TextPromptWndProc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = kClassName;
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

            if (!RegisterClassW(&wc))
            {
                const DWORD error = GetLastError();
                if (error != ERROR_CLASS_ALREADY_EXISTS)
                {
                    LogError(L"PromptTextInput class registration failed: " + std::to_wstring(error));
                    return false;
                }
            }

            registered = true;
        }

        TextPromptState state;
        state.title = title;
        state.label = label;
        state.initialValue = initialValue;

        const DWORD exStyle = owner ? WS_EX_DLGMODALFRAME : 0;
        HWND hwnd = CreateWindowExW(
            exStyle,
            kClassName,
            state.title.c_str(),
            WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            360,
            140,
            owner,
            nullptr,
            GetModuleHandleW(nullptr),
            &state);

        if (!hwnd)
        {
            LogError(L"PromptTextInput CreateWindowEx failed.");
            return false;
        }

        if (owner && IsWindow(owner))
        {
            EnableWindow(owner, FALSE);
        }

        MSG msg{};
        while (IsWindow(hwnd) && GetMessageW(&msg, nullptr, 0, 0))
        {
            if (!IsDialogMessageW(hwnd, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

        if (owner && IsWindow(owner))
        {
            EnableWindow(owner, TRUE);
            SetForegroundWindow(owner);
        }

        if (!state.accepted)
        {
            return false;
        }

        resultValue = state.resultValue;
        return true;
    }

    bool PromptOpenJsonFile(HWND owner, const std::wstring& title, std::wstring& selectedPath)
    {
        wchar_t buffer[MAX_PATH]{};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = owner;
        ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = buffer;
        ofn.nMaxFile = static_cast<DWORD>(std::size(buffer));
        ofn.lpstrTitle = title.c_str();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
        ofn.lpstrDefExt = L"json";

        if (!GetOpenFileNameW(&ofn))
        {
            return false;
        }

        selectedPath.assign(buffer);
        return true;
    }

    bool PromptSelectFolder(HWND owner, const std::wstring& title, std::wstring& selectedPath)
    {
        BROWSEINFOW info{};
        info.hwndOwner = owner;
        info.lpszTitle = title.c_str();
        info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;

        PIDLIST_ABSOLUTE itemIdList = SHBrowseForFolderW(&info);
        if (!itemIdList)
        {
            return false;
        }

        wchar_t buffer[MAX_PATH]{};
        const bool ok = SHGetPathFromIDListW(itemIdList, buffer) == TRUE;
        CoTaskMemFree(itemIdList);
        if (!ok)
        {
            return false;
        }

        selectedPath.assign(buffer);
        return !selectedPath.empty();
    }

    bool PromptSaveJsonFile(HWND owner, const std::wstring& title, std::wstring& selectedPath)
    {
        wchar_t buffer[MAX_PATH]{};
        if (!selectedPath.empty())
        {
            wcsncpy_s(buffer, selectedPath.c_str(), _TRUNCATE);
        }

        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = owner;
        ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = buffer;
        ofn.nMaxFile = static_cast<DWORD>(std::size(buffer));
        ofn.lpstrTitle = title.c_str();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER;
        ofn.lpstrDefExt = L"json";

        if (!GetSaveFileNameW(&ofn))
        {
            return false;
        }

        selectedPath.assign(buffer);
        return true;
    }

    void MeasureThemedPopupMenuItem(MEASUREITEMSTRUCT* measureInfo, const PopupMenuItemVisual& item)
    {
        if (!measureInfo)
        {
            return;
        }

        if (item.separator)
        {
            measureInfo->itemWidth = 180;
            measureInfo->itemHeight = 10;
            return;
        }

        measureInfo->itemWidth = static_cast<UINT>(max(220, 36 + (static_cast<int>(item.text.size()) * 8)));
        measureInfo->itemHeight = 28;
    }

    void DrawThemedPopupMenuItem(const DRAWITEMSTRUCT* drawInfo, const ThemePalette& palette, const PopupMenuItemVisual& item)
    {
        if (!drawInfo)
        {
            return;
        }

        HDC hdc = drawInfo->hDC;
        RECT rc = drawInfo->rcItem;

        if (item.separator)
        {
            HBRUSH bgBrush = CreateSolidBrush(palette.surfaceColor);
            FillRect(hdc, &rc, bgBrush);
            DeleteObject(bgBrush);

            HPEN pen = CreatePen(PS_SOLID, 1, palette.borderColor);
            HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, pen));
            const int y = (rc.top + rc.bottom) / 2;
            MoveToEx(hdc, rc.left + 10, y, nullptr);
            LineTo(hdc, rc.right - 10, y);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
            return;
        }

        const bool selected = (drawInfo->itemState & ODS_SELECTED) != 0;
        const bool disabled = !item.enabled || (drawInfo->itemState & ODS_DISABLED) != 0;
        const COLORREF background = selected
            ? BlendColor(palette.surfaceColor, palette.accentColor, 64)
            : palette.surfaceColor;

        HBRUSH bgBrush = CreateSolidBrush(background);
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);

        RECT textRc = rc;
        textRc.left += 12;
        textRc.right -= 12;

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, disabled ? palette.subtleTextColor : palette.textColor);
        DrawTextW(hdc, item.text.c_str(), -1, &textRc, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    }

    void LogInfo(const std::wstring& message)
    {
        std::lock_guard<std::mutex> guard(g_logMutex);
        WriteLogLine(L"INFO", message);
    }

    void LogError(const std::wstring& message)
    {
        std::lock_guard<std::mutex> guard(g_logMutex);
        WriteLogLine(L"ERROR", message);
    }

    POINT GetCursorPos()
    {
        POINT pt{};
        ::GetCursorPos(&pt);
        return pt;
    }

    void CenterWindowNearCursor(HWND hwnd)
    {
        POINT pt = GetCursorPos();
        SetWindowPos(hwnd, nullptr, pt.x, pt.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}
