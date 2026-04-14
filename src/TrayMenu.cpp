#include "TrayMenu.h"
#include "App.h"
#include "AppResources.h"
#include "SpaceManager.h"
#include "Win32Helpers.h"
#include <shellapi.h>
#include <strsafe.h>
#include <vector>
#include <string>

#pragma comment(lib, "Shell32.lib")

static constexpr UINT WMAPP_TRAYICON = (WM_APP + 1);
static constexpr UINT ID_TRAY_COMMAND_BASE = 1001;

TrayMenu::TrayMenu(App* app) : m_app(app)
{
}

TrayMenu::~TrayMenu()
{
    Destroy();
}

void TrayMenu::RefreshTooltipText()
{
    if (!m_hwnd)
    {
        return;
    }

    std::wstring tooltip = L"SimpleSpaces";
    if (m_app && m_app->GetSpaceManager() && m_app->GetSpaceManager()->AreAllSpacesHidden())
    {
        tooltip = L"SimpleSpaces (hidden)";
    }

    StringCchCopyW(m_nid.szTip, ARRAYSIZE(m_nid.szTip), tooltip.c_str());
    m_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    m_nid.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;
}

bool TrayMenu::Create(HINSTANCE hInstance)
{
    if (m_hwnd)
    {
        return true;
    }

    m_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSW wc{};
    wc.lpfnWndProc = TrayMenu::WndProcStatic;
    wc.lpszClassName = L"SimpleSpaces_TrayWindow";
    wc.hInstance = hInstance;

    if (!RegisterClassW(&wc))
    {
        const DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS)
        {
            Win32Helpers::LogError(L"Tray class registration failed with error: " + std::to_wstring(error));
            return false;
        }
    }

    m_hwnd = CreateWindowW(
        wc.lpszClassName,
        L"",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        nullptr,
        hInstance,
        this);

    if (!m_hwnd)
    {
        Win32Helpers::LogError(L"Tray window creation failed: " + std::to_wstring(GetLastError()));
        return false;
    }

    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;
    m_nid.uCallbackMessage = WMAPP_TRAYICON;
    m_nid.hIcon = static_cast<HICON>(LoadImageW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDI_SPACES_APP),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR | LR_SHARED));
    if (!m_nid.hIcon)
    {
        m_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        if (!m_nid.hIcon)
        {
            Win32Helpers::LogError(L"Tray icon load failed; no fallback icon available.");
        }
    }
    StringCchCopyW(m_nid.szTip, ARRAYSIZE(m_nid.szTip), L"SimpleSpaces");

    if (!Shell_NotifyIconW(NIM_ADD, &m_nid))
    {
        const DWORD error = GetLastError();
        Win32Helpers::LogError(L"Shell_NotifyIconW(NIM_ADD) failed with error: " + std::to_wstring(error) +
                               L". Tray icon will not appear. Check Windows version compatibility and taskbar state.");
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
        ZeroMemory(&m_nid, sizeof(m_nid));
        return false;
    }

    Win32Helpers::LogInfo(L"Tray icon registered successfully (NIM_ADD). uID=" + std::to_wstring(m_nid.uID));

    m_nid.uVersion = NOTIFYICON_VERSION_4;
    if (Shell_NotifyIconW(NIM_SETVERSION, &m_nid))
    {
        m_notifyMode = TrayNotifyMode::Version4;
        Win32Helpers::LogInfo(L"Tray using NOTIFYICON_VERSION_4 (modern notify mode)");
    }
    else
    {
        m_notifyMode = TrayNotifyMode::Legacy;
        Win32Helpers::LogInfo(L"Tray using Legacy notify mode (NOTIFYICON_VERSION_4 not supported; likely older Windows)");
    }

    RefreshTooltipText();

    // Validate tray icon is actually registered before returning success
    if (!IsTrayIconValid())
    {
        Win32Helpers::LogError(L"Tray icon registered but immediate validation check failed. Icon may not appear.");
        // Don't fail here—icon might be registered but Shell_NotifyIconGetRect not available
    }

    return true;
}

void TrayMenu::Destroy()
{
    if (!m_hwnd)
    {
        return;
    }

    Shell_NotifyIconW(NIM_DELETE, &m_nid);
    DestroyWindow(m_hwnd);
    m_hwnd = nullptr;
    ZeroMemory(&m_nid, sizeof(m_nid));
}

bool TrayMenu::IsTrayIconValid() const
{
    // Tray icon is valid if:
    // 1. Message window exists
    // 2. Icon was successfully registered (m_nid has valid data)
    // 3. Icon is still registered with shell

    if (!m_hwnd)
    {
        return false;
    }

    if (m_nid.uID == 0 || m_nid.hWnd != m_hwnd)
    {
        return false;
    }

    // Optional: Query shell to confirm icon is still registered
    // This is a defensive check for scenarios where shell lost track of the icon
    // (e.g., Explorer restart incomplete recovery)
    NOTIFYICONIDENTIFIER nii{};
    nii.cbSize = sizeof(nii);
    nii.hWnd = m_hwnd;
    nii.uID = m_nid.uID;

    // If Shell_NotifyIconGetRect succeeds, the shell knows about our icon
    RECT dummyRect{};
    const HRESULT hr = Shell_NotifyIconGetRect(&nii, &dummyRect);
    return hr == S_OK;
}

std::wstring TrayMenu::GetDiagnosticStatus() const
{
    std::wstring status;

    status += L"[Tray Diagnostic Status]\n";
    status += L"  Window handle: ";
    status += (m_hwnd ? L"valid" : L"null");
    status += L"\n";
    
    status += L"  Notify mode: ";
    status += (m_notifyMode == TrayNotifyMode::Version4 ? L"Version4" : L"Legacy");
    status += L"\n";
    
    status += L"  Icon ID: ";
    status += std::to_wstring(m_nid.uID);
    status += L"\n";
    
    status += L"  Icon valid: ";
    status += std::wstring(IsTrayIconValid() ? L"yes" : L"no");
    status += L"\n";
    
    status += L"  Taskbar message registered: ";
    status += std::wstring(m_taskbarCreatedMessage != 0 ? L"yes" : L"no");
    status += L"\n";

    if (m_hwnd && IsWindow(m_hwnd))
    {
        status += L"  Window state: created and valid\n";
    }
    else if (m_hwnd)
    {
        status += L"  Window state: handle exists but IsWindow() failed\n";
    }
    else
    {
        status += L"  Window state: no window handle\n";
    }

    return status;
}

void TrayMenu::ShowContextMenu(POINT pt)
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
    {
        Win32Helpers::LogError(L"CreatePopupMenu failed for tray menu.");
        return;
    }

    m_commandByMenuId.clear();
    m_menuVisualByMenuId.clear();
    UINT menuId = ID_TRAY_COMMAND_BASE;
    const ThemePlatform* themePlatform = m_app ? m_app->GetThemePlatform() : nullptr;
    const ThemePalette palette = themePlatform ? themePlatform->BuildPalette() : ThemePalette{};
    int minWidthPx = themePlatform ? themePlatform->GetTrayMenuMinWidthPx() : 220;
    int rowHeightPx = themePlatform ? themePlatform->GetTrayMenuRowHeightPx() : 28;

    if (themePlatform)
    {
        ThemeResourceResolver* resolver = themePlatform->GetResourceResolver();
        if (resolver)
        {
            const std::wstring menuStyle = resolver->GetSelectedMenuStyle();
            if (menuStyle == L"compact")
            {
                minWidthPx = (std::max)(180, minWidthPx - 20);
                rowHeightPx = (std::max)(24, rowHeightPx - 4);
            }
            else if (menuStyle == L"hierarchical")
            {
                minWidthPx = (std::max)(240, minWidthPx + 20);
                rowHeightPx = (std::max)(30, rowHeightPx + 2);
            }
        }
    }

    UINT animationFlags = TPM_VERPOSANIMATION;
    if (themePlatform)
    {
        ThemeResourceResolver* resolver = themePlatform->GetResourceResolver();
        if (resolver)
        {
            const int motionMs = resolver->GetMotionDurationMs(resolver->GetSelectedMotionPreset(), 220);
            if (motionMs <= 0)
            {
                animationFlags = TPM_NOANIMATION;
            }
            else if (motionMs <= 220)
            {
                animationFlags = TPM_HORPOSANIMATION;
            }
            else
            {
                animationFlags = TPM_VERPOSANIMATION;
            }
        }
    }

    auto appendSeparator = [&]() {
        if (!AppendMenuW(menu, MF_SEPARATOR, 0, nullptr))
        {
            Win32Helpers::LogError(L"AppendMenuW failed for tray separator.");
        }
    };

    auto appendItem = [&](const std::wstring& title,
                          const std::wstring& commandId,
                          const std::wstring& iconKey) {
        MENUITEMINFOW itemInfo{};
        itemInfo.cbSize = sizeof(itemInfo);
        itemInfo.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_DATA;
        itemInfo.fType = MFT_OWNERDRAW;
        itemInfo.fState = MFS_ENABLED;
        itemInfo.wID = menuId;
        itemInfo.dwItemData = static_cast<ULONG_PTR>(menuId);

        if (!InsertMenuItemW(menu, GetMenuItemCount(menu), TRUE, &itemInfo))
        {
            Win32Helpers::LogError(L"AppendMenuW failed for tray item: " + title);
            return;
        }

        Win32Helpers::PopupMenuItemVisual visual;
        visual.text = title;
        visual.enabled = true;
        visual.preferredWidthPx = minWidthPx;
        visual.preferredHeightPx = rowHeightPx;

        if (themePlatform)
        {
            const ThemeIconMapping iconMapping = themePlatform->ResolveIconMapping(iconKey, L"");
            visual.iconGlyph = iconMapping.glyph;
            if (!iconMapping.assetPack.empty() && !iconMapping.assetName.empty())
            {
                visual.iconAsset = iconMapping.assetPack + L":" + iconMapping.assetName;
            }
        }

        m_commandByMenuId[menuId] = commandId;
        m_menuVisualByMenuId[menuId] = visual;
        ++menuId;
    };

    if (m_app)
    {
        const auto entries = m_app->GetTrayMenuEntries();
        for (const auto& entry : entries)
        {
            if (entry.separatorBefore)
            {
                appendSeparator();
            }

            appendItem(entry.title, entry.commandId, entry.iconKey);
        }
    }

    MENUINFO menuInfo{};
    menuInfo.cbSize = sizeof(menuInfo);
    menuInfo.fMask = MIM_BACKGROUND;
    HBRUSH menuBrush = CreateSolidBrush(palette.surfaceColor);
    menuInfo.hbrBack = menuBrush;
    SetMenuInfo(menu, &menuInfo);

    const UINT popupFlags = TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_BOTTOMALIGN | animationFlags;

    SetForegroundWindow(m_hwnd);
    const int cmd = TrackPopupMenuEx(menu,
                                     popupFlags,
                                     pt.x,
                                     pt.y,
                                     m_hwnd,
                                     nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
    DeleteObject(menuBrush);
    m_menuVisualByMenuId.clear();

    if (cmd != 0 && m_app)
    {
        const auto it = m_commandByMenuId.find(static_cast<UINT>(cmd));
        if (it != m_commandByMenuId.end())
        {
            const bool handled = m_app->ExecuteCommand(it->second);
            if (!handled)
            {
                Win32Helpers::LogError(L"Tray command was not handled: " + it->second);
            }
        }
    }

    m_commandByMenuId.clear();
}

LRESULT CALLBACK TrayMenu::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    TrayMenu* pThis = nullptr;

    if (msg == WM_NCCREATE)
    {
        auto pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<TrayMenu*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    }
    else
    {
        pThis = reinterpret_cast<TrayMenu*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis)
        return pThis->WndProc(hwnd, msg, wParam, lParam);

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT TrayMenu::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (m_taskbarCreatedMessage != 0 && msg == m_taskbarCreatedMessage)
    {
        // Taskbar was recreated (Explorer restarted). Re-register the tray icon.
        // This is critical for tray resilience: if Explorer crashes, we must recover.
        Win32Helpers::LogInfo(L"TaskbarCreated message received. Re-registering tray icon...");
        
        if (m_hwnd)
        {
            if (!Shell_NotifyIconW(NIM_ADD, &m_nid))
            {
                Win32Helpers::LogError(L"Failed to re-register tray icon after Explorer restart (NIM_ADD failed)");
            }
            else
            {
                Win32Helpers::LogInfo(L"Tray icon re-registered successfully after Explorer restart");
            }

            m_nid.uVersion = NOTIFYICON_VERSION_4;
            if (Shell_NotifyIconW(NIM_SETVERSION, &m_nid))
            {
                m_notifyMode = TrayNotifyMode::Version4;
                Win32Helpers::LogInfo(L"Tray mode restored to Version4 after Explorer restart");
            }
            else
            {
                m_notifyMode = TrayNotifyMode::Legacy;
                Win32Helpers::LogInfo(L"Tray mode restored to Legacy after Explorer restart");
            }
            RefreshTooltipText();
        }
        else
        {
            Win32Helpers::LogError(L"Received TaskbarCreated but tray window invalid. Cannot recover.");
        }
        return 0;
    }

    if (msg == WMAPP_TRAYICON)
    {
        switch (LOWORD(lParam))
        {
        case WM_CONTEXTMENU:
        case WM_RBUTTONUP:
        {
            const UINT trayEvent = LOWORD(lParam);
            const bool shouldOpen =
                (m_notifyMode == TrayNotifyMode::Version4 && trayEvent == WM_CONTEXTMENU) ||
                (m_notifyMode == TrayNotifyMode::Legacy && trayEvent == WM_RBUTTONUP);
            if (!shouldOpen)
            {
                // Diagnostic: Unexpected tray event for current mode (might be noise from dual-mode or deferred event)
                return 0;
            }

            // Diagnostic: Log context menu request
            Win32Helpers::LogInfo(L"Tray context menu requested (event=" + std::to_wstring(trayEvent) + L")");

            POINT pt{};
            bool haveAnchor = false;
            if (trayEvent == WM_CONTEXTMENU)
            {
                const short x = static_cast<short>(LOWORD(wParam));
                const short y = static_cast<short>(HIWORD(wParam));
                if (!(x == -1 && y == -1))
                {
                    pt.x = x;
                    pt.y = y;
                    haveAnchor = true;
                }
            }

            if (!haveAnchor)
            {
                NOTIFYICONIDENTIFIER nii{};
                nii.cbSize = sizeof(nii);
                nii.hWnd = m_hwnd;
                nii.uID = m_nid.uID;

                RECT iconRect{};
                if (Shell_NotifyIconGetRect(&nii, &iconRect) == S_OK)
                {
                    pt.x = iconRect.left;
                    pt.y = iconRect.top;
                    haveAnchor = true;
                }
            }

            if (!haveAnchor)
            {
                GetCursorPos(&pt);
            }

            ShowContextMenu(pt);
            return 0;
        }
        case WM_LBUTTONDBLCLK:
        {
            // Diagnostic: Log double-click toggle request
            Win32Helpers::LogInfo(L"Tray double-click: toggling spaces visibility");
            
            if (m_app && m_app->GetSpaceManager())
            {
                m_app->GetSpaceManager()->ToggleAllSpacesHidden();
                RefreshTooltipText();
            }
            else
            {
                Win32Helpers::LogError(L"Tray double-click: SpaceManager not available, toggle failed");
            }
            return 0;
        }
        default:
            break;
        }
    }

    if (msg == WM_MEASUREITEM)
    {
        auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (measure && measure->CtlType == ODT_MENU)
        {
            const auto it = m_menuVisualByMenuId.find(measure->itemID);
            if (it != m_menuVisualByMenuId.end())
            {
                Win32Helpers::MeasureThemedPopupMenuItem(measure, it->second);
                return TRUE;
            }
        }
    }

    if (msg == WM_DRAWITEM)
    {
        auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (draw && draw->CtlType == ODT_MENU)
        {
            const auto it = m_menuVisualByMenuId.find(draw->itemID);
            if (it != m_menuVisualByMenuId.end())
            {
                const ThemePalette palette = (m_app && m_app->GetThemePlatform())
                    ? m_app->GetThemePlatform()->BuildPalette()
                    : ThemePalette{};
                Win32Helpers::DrawThemedPopupMenuItem(draw, palette, it->second);
                return TRUE;
            }
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
