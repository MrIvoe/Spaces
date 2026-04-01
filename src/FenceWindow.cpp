#include "FenceWindow.h"
#include "FenceManager.h"
#include "Win32Helpers.h"
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>
#include <algorithm>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Shell32.lib")

static constexpr const wchar_t* kFenceWindowClass = L"SimpleFences_FenceWindow";
static constexpr int kTitleBarHeight = 28;
static constexpr int kBorderSize = 1;

FenceWindow::FenceWindow(FenceManager* manager, const FenceModel& model)
    : m_manager(manager), m_model(model)
{
}

FenceWindow::~FenceWindow()
{
    Destroy();
}

bool FenceWindow::Create(HWND parent)
{
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = FenceWindow::WndProcStatic;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kFenceWindowClass;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
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
        kFenceWindowClass,
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

    DragAcceptFiles(m_hwnd, TRUE);
    InitializeImageList();

    return true;
}

void FenceWindow::Show()
{
    if (m_hwnd)
    {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
    }
}

void FenceWindow::Destroy()
{
    if (m_hwnd)
    {
        DragAcceptFiles(m_hwnd, FALSE);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void FenceWindow::UpdateModel(const FenceModel& model)
{
    m_model = model;
    if (m_hwnd)
    {
        SetWindowTextW(m_hwnd, m_model.title.c_str());
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void FenceWindow::SetItems(const std::vector<FenceItem>& items)
{
    m_items = items;
    if (m_hwnd)
    {
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

HWND FenceWindow::GetHwnd() const
{
    return m_hwnd;
}

const std::wstring& FenceWindow::GetFenceId() const
{
    return m_model.id;
}

const FenceModel& FenceWindow::GetModel() const
{
    return m_model;
}

LRESULT CALLBACK FenceWindow::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    FenceWindow* pThis = nullptr;

    if (msg == WM_NCCREATE)
    {
        auto pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<FenceWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    }
    else
    {
        pThis = reinterpret_cast<FenceWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis)
        return pThis->WndProc(hwnd, msg, wParam, lParam);

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT FenceWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
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
        m_selectedItem = -1;
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;

    case WM_RBUTTONUP:
        OnContextMenu(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

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
        int width = (int)(short)LOWORD(lParam);
        int height = (int)(short)HIWORD(lParam);
        OnSize(width, height);
        return 0;
    }

    case WM_EXITSIZEMOVE:
    {
        if (m_manager)
        {
            RECT rc{};
            GetWindowRect(m_hwnd, &rc);
            m_manager->UpdateFenceGeometry(m_model.id, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
        }
        return 0;
    }

    case WM_DESTROY:
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void FenceWindow::OnPaint()
{
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rc{};
    GetClientRect(m_hwnd, &rc);

    // Draw background
    HBRUSH bgBrush = CreateSolidBrush(RGB(45, 45, 45));
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Draw title bar
    RECT titleRc = rc;
    titleRc.bottom = kTitleBarHeight;
    HBRUSH titleBrush = CreateSolidBrush(RGB(65, 65, 65));
    FillRect(hdc, &titleRc, titleBrush);
    DeleteObject(titleBrush);

    // Draw title text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(240, 240, 240));
    RECT textRc = titleRc;
    textRc.left += 8;
    textRc.top += 4;
    DrawTextW(hdc, m_model.title.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Draw items
    int itemY = kTitleBarHeight + 8;
    SetTextColor(hdc, RGB(200, 200, 200));
    static constexpr int kItemHeight = 24;  // 16px icon + 4px padding top/bottom
    static constexpr int kIconSize = 16;
    
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
    {
        const auto& item = m_items[i];
        RECT itemRc = rc;
        itemRc.left += 8;
        itemRc.top = itemY;
        itemRc.bottom = itemY + kItemHeight;
        itemRc.right -= 8;

        // Highlight selected item
        if (i == m_selectedItem)
        {
            HBRUSH hiBrush = CreateSolidBrush(RGB(85, 85, 85));
            FillRect(hdc, &itemRc, hiBrush);
            DeleteObject(hiBrush);
            SetTextColor(hdc, RGB(240, 240, 240));
        }
        else
        {
            SetTextColor(hdc, RGB(200, 200, 200));
        }

        // Draw icon if available
        if (item.iconIndex >= 0)
        {
            // Get system image list for drawing
            SHFILEINFOW sfi{};
            HIMAGELIST hImageList = reinterpret_cast<HIMAGELIST>(
                SHGetFileInfoW(L".", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                              SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)
            );
            
            if (hImageList)
            {
                int iconX = itemRc.left + 2;
                int iconY = itemRc.top + (kItemHeight - kIconSize) / 2;
                ImageList_Draw(hImageList, item.iconIndex, hdc, iconX, iconY, ILD_TRANSPARENT);
            }
        }

        // Draw text beside the icon
        std::wstring displayText = item.name;
        if (item.isDirectory)
            displayText += L" [folder]";

        RECT itemTextRc = itemRc;
        itemTextRc.left += kIconSize + 6;  // Icon + spacing
        DrawTextW(hdc, displayText.c_str(), -1, &itemTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        itemY += kItemHeight;
    }

    EndPaint(m_hwnd, &ps);
}

void FenceWindow::OnLButtonDown(int x, int y)
{
    if (y < kTitleBarHeight)
    {
        m_dragging = true;
        m_dragStart = { x, y };
        GetWindowRect(m_hwnd, &m_windowStart);
        SetCapture(m_hwnd);
    }
}

void FenceWindow::OnMouseMove(int x, int y, WPARAM flags)
{
    if (m_dragging && (flags & MK_LBUTTON))
    {
        int dx = x - m_dragStart.x;
        int dy = y - m_dragStart.y;

        int newX = m_windowStart.left + dx;
        int newY = m_windowStart.top + dy;

        SetWindowPos(m_hwnd, nullptr, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
    else
    {
        // Track item under cursor for highlight
        int item = GetItemAtPosition(x, y);
        if (item != m_selectedItem)
        {
            m_selectedItem = item;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    }
}

void FenceWindow::OnLButtonUp()
{
    if (m_dragging)
    {
        m_dragging = false;
        ReleaseCapture();

        if (m_manager)
        {
            RECT rc{};
            GetWindowRect(m_hwnd, &rc);
            m_manager->UpdateFenceGeometry(m_model.id, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
        }
    }
}

void FenceWindow::OnContextMenu(int x, int y)
{
    int itemIndex = GetItemAtPosition(x, y);
    HMENU menu = CreatePopupMenu();

    if (itemIndex >= 0)
    {
        // Item context menu
        AppendMenuW(menu, MF_STRING, 2001, L"Open");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 2002, L"Delete Item");
    }
    else
    {
        // Fence context menu
        AppendMenuW(menu, MF_STRING, 1001, L"New Fence");
        AppendMenuW(menu, MF_STRING, 1002, L"Rename Fence");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 1003, L"Delete Fence");
    }

    POINT pt = { x, y };
    ClientToScreen(m_hwnd, &pt);

    int cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, m_hwnd, nullptr);
    DestroyMenu(menu);

    switch (cmd)
    {
    case 1001: // New Fence
        if (m_manager)
            m_manager->CreateFenceAt(pt.x, pt.y);
        break;

    case 1002: // Rename
        // TODO: Implement rename dialog
        break;

    case 1003: // Delete Fence
        if (m_manager)
            m_manager->DeleteFence(m_model.id);
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

void FenceWindow::OnDropFiles(HDROP hDrop)
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
        m_manager->HandleDrop(m_model.id, paths);
}

void FenceWindow::OnMove(int x, int y)
{
    // Update model position
    m_model.x = x;
    m_model.y = y;
}

void FenceWindow::OnSize(int width, int height)
{
    // Update model size
    m_model.width = width;
    m_model.height = height;
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

int FenceWindow::GetItemAtPosition(int x, int y) const
{
    (void)x;

    // Check if click is in title bar
    if (y < kTitleBarHeight)
        return -1;

    // Calculate which item was clicked
    int itemY = kTitleBarHeight + 8;
    int itemIndex = 0;
    static constexpr int kItemHeight = 24;

    for (size_t i = 0; i < m_items.size(); ++i)
    {
        if (y >= itemY && y < itemY + kItemHeight)
            return itemIndex;
        itemY += kItemHeight;
        ++itemIndex;
    }

    return -1;
}

void FenceWindow::ExecuteItem(int itemIndex)
{
    if (itemIndex < 0 || itemIndex >= static_cast<int>(m_items.size()))
        return;

    const auto& item = m_items[itemIndex];
    
    // Use ShellExecute to open the file/folder
    ShellExecuteW(m_hwnd, L"open", item.fullPath.c_str(), nullptr, nullptr, SW_SHOW);
}

void FenceWindow::OnLButtonDblClk(int x, int y)
{
    (void)x;

    int itemIndex = GetItemAtPosition(x, y);
    if (itemIndex >= 0)
    {
        ExecuteItem(itemIndex);
    }
}

bool FenceWindow::InitializeImageList()
{
    // Get system small image list handle
    // We don't own this handle - it's managed by Windows, so we don't destroy it
    try
    {
        SHFILEINFOW sfi{};
        m_imageList = reinterpret_cast<HIMAGELIST>(
            SHGetFileInfoW(L".", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), 
                          SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)
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
