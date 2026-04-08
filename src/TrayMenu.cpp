#include "TrayMenu.h"
#include "App.h"
#include "AppResources.h"
#include "SpaceManager.h"
#include "Win32Helpers.h"
#include "core/ThemePlatform.h"
#include <shellapi.h>
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

    lstrcpynW(m_nid.szTip, tooltip.c_str(), static_cast<int>(std::size(m_nid.szTip)));
    m_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    m_nid.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;
}

bool TrayMenu::Create(HINSTANCE hInstance)
{
    // Create message-only window
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
        return false;

    // Add notify icon
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
    }
    lstrcpyW(m_nid.szTip, L"SimpleSpaces");

    if (!Shell_NotifyIconW(NIM_ADD, &m_nid))
    {
        Win32Helpers::LogError(L"Failed to create tray icon.");
        return false;
    }

    RefreshTooltipText();

    return true;
}

void TrayMenu::Destroy()
{
    if (m_hwnd)
    {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void TrayMenu::ShowContextMenu(POINT pt)
{
    RefreshTooltipText();

    HMENU menu = CreatePopupMenu();

    m_commandByMenuId.clear();
    m_menuVisuals.clear();
    UINT menuId = ID_TRAY_COMMAND_BASE;

    int trayMinWidthPx = 220;
    int trayRowHeightPx = 28;
    if (m_app && m_app->GetThemePlatform())
    {
        trayMinWidthPx = m_app->GetThemePlatform()->GetTrayMenuMinWidthPx();
        trayRowHeightPx = m_app->GetThemePlatform()->GetTrayMenuRowHeightPx();
    }

    auto appendSeparator = [&]() {
        AppendMenuW(menu, MF_OWNERDRAW | MF_DISABLED, menuId, nullptr);
        m_menuVisuals.emplace(menuId, Win32Helpers::PopupMenuItemVisual{L"", L"", L"", true, false, trayMinWidthPx, trayRowHeightPx});
        ++menuId;
    };

    auto appendItem = [&](const std::wstring& title, const std::wstring& commandId, const ThemeIconMapping& iconMapping) {
        const UINT flags = MF_OWNERDRAW | MF_STRING | MF_ENABLED;
        AppendMenuW(menu, flags, menuId, nullptr);
        m_commandByMenuId[menuId] = commandId;
        m_menuVisuals.emplace(menuId, Win32Helpers::PopupMenuItemVisual{title, iconMapping.glyph, iconMapping.assetPack.empty() ? L"" : (iconMapping.assetPack + L":" + iconMapping.assetName), false, true, trayMinWidthPx, trayRowHeightPx});
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

            ThemeIconMapping icon;
            const ThemePlatform* themePlatform = m_app->GetThemePlatform();
            if (themePlatform && !entry.iconKey.empty())
            {
                icon = themePlatform->ResolveIconMapping(entry.iconKey);
            }

            appendItem(entry.title, entry.commandId, icon);
        }
    }

    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, m_hwnd, nullptr);
    DestroyMenu(menu);

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
    if (msg == WM_MEASUREITEM)
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
    }

    if (msg == WM_DRAWITEM)
    {
        auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (draw)
        {
            const auto it = m_menuVisuals.find(draw->itemID);
            if (it != m_menuVisuals.end())
            {
                const ThemePalette palette = (m_app && m_app->GetThemePlatform())
                    ? m_app->GetThemePlatform()->BuildPalette()
                    : ThemePalette{};
                Win32Helpers::DrawThemedPopupMenuItem(draw, palette, it->second);
                return TRUE;
            }
        }
    }

    if (msg == WMAPP_TRAYICON)
    {
        if (lParam == WM_RBUTTONUP)
        {
            POINT pt{};
            GetCursorPos(&pt);
            ShowContextMenu(pt);
            return 0;
        }

        if (lParam == WM_LBUTTONDBLCLK)
        {
            if (m_app && m_app->GetSpaceManager())
            {
                m_app->GetSpaceManager()->ToggleAllSpacesHidden();
                RefreshTooltipText();
            }
            return 0;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
