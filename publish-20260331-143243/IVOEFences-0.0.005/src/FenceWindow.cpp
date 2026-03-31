#include "FenceWindow.h"
#include "FenceLayoutEngine.h"
#include "FenceSelectionModel.h"
#include <algorithm>
#include <utility>
#include <shellapi.h>
#include <windowsx.h>

namespace {
constexpr wchar_t kFenceClassName[] = L"IVOEFenceWindow";
constexpr BYTE kFenceAlpha = 178;
constexpr int kTitleBarHeight = 30;
constexpr int kCollapsedHeight = 30;
constexpr int kMinWidth = 180;
constexpr int kMinHeight = 120;
constexpr int kResizeGrip = 12;
constexpr int kBodyPadding = 10;
constexpr int kItemLineHeight = 18;
constexpr UINT_PTR kCollapseTimer = 1;
constexpr UINT kCollapseDelayMs = 900;
constexpr COLORREF kSelectionFillColor = RGB(56, 92, 136);
constexpr COLORREF kSelectionFrameColor = RGB(96, 146, 206);
constexpr COLORREF kFocusFrameColor = RGB(120, 120, 120);
constexpr UINT ID_FENCE_RENAME = 3000;
constexpr UINT ID_FENCE_OPEN_BACKING_FOLDER = 3001;
constexpr UINT ID_FENCE_DELETE = 3002;
}

FenceWindow::FenceWindow()
        : m_layoutEngine(std::make_unique<FenceLayoutEngine>()),
            m_selectionModel(std::make_unique<FenceSelectionModel>()) {
}
FenceWindow::~FenceWindow() = default;

bool FenceWindow::RegisterClass(HINSTANCE instance) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = FenceWindow::WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kFenceClassName;

    if (RegisterClassW(&wc)) {
        return true;
    }

    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool FenceWindow::Create(const CreateParams& params) {
    m_instance = params.instance;
    m_parent = params.parent;
    m_id = params.id;
    m_title = params.title;
    m_expandedRect = params.rect;
    m_collapsed = params.collapsed;
    m_onChanged = params.onChanged;
    m_onFilesDropped = params.onFilesDropped;
    m_onOpenBackingFolder = params.onOpenBackingFolder;
    m_onDeleteFence = params.onDeleteFence;
    m_canDeleteFence = params.canDeleteFence;
    m_backingFolder = params.backingFolder;

    POINT pt{ params.rect.left, params.rect.top };
    ScreenToClient(m_parent, &pt);

    int width = std::max(kMinWidth, static_cast<int>(params.rect.right - params.rect.left));
    int height = std::max(kMinHeight, static_cast<int>(params.rect.bottom - params.rect.top));

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        kFenceClassName,
        L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        pt.x, pt.y, width, height,
        m_parent,
        nullptr,
        m_instance,
        this);

    if (!m_hwnd) {
        return false;
    }

    SetLayeredWindowAttributes(m_hwnd, 0, kFenceAlpha, LWA_ALPHA);
    DragAcceptFiles(m_hwnd, TRUE);
    UpdateWindowBounds();
    return true;
}

FenceData FenceWindow::ToFenceData() const {
    FenceData data;
    data.id = m_id;
    data.title = m_title;
    data.rect = m_expandedRect;
    data.collapsed = m_collapsed;
    data.backingFolder = m_backingFolder;
    return data;
}

void FenceWindow::SetBackingFolder(const std::wstring& path) {
    m_backingFolder = path;
    if (m_onChanged) {
        m_onChanged();
    }
}

void FenceWindow::SetItemLabels(std::vector<std::wstring> labels) {
    m_itemLabels = std::move(labels);
    if (m_selectionModel) {
        m_selectionModel->Reset(static_cast<int>(m_itemLabels.size()));
    }
    RebuildLayoutCache();
    if (m_hwnd) {
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void FenceWindow::SetCollapsed(bool collapsed) {
    if (m_collapsed == collapsed || !m_hwnd) {
        return;
    }

    if (!collapsed) {
        m_collapsed = false;
    } else {
        RECT wr{};
        GetWindowRect(m_hwnd, &wr);
        m_expandedRect = wr;
        m_collapsed = true;
    }

    UpdateWindowBounds();
    InvalidateRect(m_hwnd, nullptr, TRUE);
    if (m_onChanged) {
        m_onChanged();
    }
}

void FenceWindow::BeginRename() {
    if (m_edit || !m_hwnd) {
        return;
    }

    RECT rc{};
    GetClientRect(m_hwnd, &rc);

    m_edit = CreateWindowExW(
        0, L"EDIT", m_title.c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        8, 4, std::max(80, (int)rc.right - 16), kTitleBarHeight - 8,
        m_hwnd, nullptr, m_instance, nullptr);

    if (m_edit) {
        SetFocus(m_edit);
        SendMessageW(m_edit, EM_SETSEL, 0, -1);
    }
}

void FenceWindow::CommitRename() {
    if (!m_edit) return;

    wchar_t buf[256]{};
    GetWindowTextW(m_edit, buf, 256);
    m_title = buf[0] ? buf : L"Fence";

    DestroyWindow(m_edit);
    m_edit = nullptr;
    InvalidateRect(m_hwnd, nullptr, TRUE);
    if (m_onChanged) {
        m_onChanged();
    }
}

void FenceWindow::StartTrackingMouse() {
    if (m_trackingMouse || !m_hwnd) return;

    TRACKMOUSEEVENT tme{};
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_hwnd;
    if (TrackMouseEvent(&tme)) {
        m_trackingMouse = true;
    }
}

void FenceWindow::UpdateWindowBounds() {
    if (!m_hwnd) return;

    int width = std::max(kMinWidth, (int)(m_expandedRect.right - m_expandedRect.left));
    int height = m_collapsed
        ? kCollapsedHeight
        : std::max(kMinHeight, (int)(m_expandedRect.bottom - m_expandedRect.top));

    POINT pt{ m_expandedRect.left, m_expandedRect.top };
    ScreenToClient(m_parent, &pt);

    SetWindowPos(
        m_hwnd,
        nullptr,
        pt.x, pt.y,
        width,
        height,
        SWP_NOZORDER | SWP_NOACTIVATE);

    RebuildLayoutCache();
}

void FenceWindow::RebuildLayoutCache() {
    m_itemDrawRects.clear();
    if (!m_hwnd || !m_layoutEngine || m_collapsed) {
        return;
    }

    RECT client{};
    GetClientRect(m_hwnd, &client);

    FenceLayoutEngine::LayoutConfig config;
    config.topPadding = kTitleBarHeight + kBodyPadding;
    config.leftPadding = kBodyPadding;
    config.rightPadding = kBodyPadding;
    config.bottomPadding = kBodyPadding;
    config.lineHeight = kItemLineHeight;

    const std::vector<FenceLayoutEngine::ItemSlot> slots =
        m_layoutEngine->BuildListLayout(client, m_itemLabels.size(), config);

    m_itemDrawRects.reserve(slots.size());
    for (const FenceLayoutEngine::ItemSlot& slot : slots) {
        m_itemDrawRects.push_back(slot.bounds);
    }
}

void FenceWindow::OnMouseMove(int, int y, WPARAM keys) {
    StartTrackingMouse();
    KillTimer(m_hwnd, kCollapseTimer);

    if (m_collapsed && y <= kCollapsedHeight) {
        SetCollapsed(false);
    }

    if ((keys & MK_LBUTTON) && m_dragMode != DragMode::None) {
        POINT screen{};
        GetCursorPos(&screen);

        int dx = screen.x - m_dragStartScreen.x;
        int dy = screen.y - m_dragStartScreen.y;

        RECT rc = m_dragStartRect;
        if (m_dragMode == DragMode::Move) {
            OffsetRect(&rc, dx, dy);
        } else if (m_dragMode == DragMode::Resize) {
            rc.right = std::max(rc.left + kMinWidth, rc.right + dx);
            rc.bottom = std::max(rc.top + kMinHeight, rc.bottom + dy);
        }

        m_expandedRect = rc;
        UpdateWindowBounds();
    }
}

int FenceWindow::HitTestItem(POINT clientPt) const {
    for (size_t i = 0; i < m_itemDrawRects.size(); ++i) {
        if (PtInRect(&m_itemDrawRects[i], clientPt)) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

void FenceWindow::OnKeyDown(WPARAM key) {
    if (!m_selectionModel) {
        return;
    }

    const int itemCount = std::min(
        static_cast<int>(m_itemLabels.size()),
        static_cast<int>(m_itemDrawRects.size()));
    if (itemCount <= 0) {
        return;
    }

    int current = m_selectionModel->GetPrimarySelection();
    int target = current;

    switch (key) {
    case VK_UP:
        target = (current < 0) ? (itemCount - 1) : std::max(0, current - 1);
        break;
    case VK_DOWN:
        target = (current < 0) ? 0 : std::min(itemCount - 1, current + 1);
        break;
    case VK_HOME:
        target = 0;
        break;
    case VK_END:
        target = itemCount - 1;
        break;
    default:
        return;
    }

    m_selectionModel->SelectSingle(target);
}

void FenceWindow::ShowTitleBarContextMenu(POINT screenPt) {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    AppendMenuW(menu, MF_STRING, ID_FENCE_RENAME, L"Rename Fence");
    AppendMenuW(menu, MF_STRING, ID_FENCE_OPEN_BACKING_FOLDER, L"Open Backing Folder");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_FENCE_DELETE, L"Delete Fence");

    bool canDelete = true;
    if (m_canDeleteFence) {
        canDelete = m_canDeleteFence(m_id);
    }
    if (!canDelete) {
        EnableMenuItem(menu, ID_FENCE_DELETE, MF_BYCOMMAND | MF_GRAYED);
    }

    SetForegroundWindow(m_hwnd);

    UINT cmd = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON,
        screenPt.x,
        screenPt.y,
        0,
        m_hwnd,
        nullptr);

    DestroyMenu(menu);

    if (cmd == ID_FENCE_RENAME) {
        BeginRename();
    } else if (cmd == ID_FENCE_OPEN_BACKING_FOLDER && m_onOpenBackingFolder) {
        m_onOpenBackingFolder(m_id);
    } else if (cmd == ID_FENCE_DELETE && m_onDeleteFence && canDelete) {
        m_onDeleteFence(m_id);
    }
}

void FenceWindow::Paint() {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rc{};
    GetClientRect(m_hwnd, &rc);

    HBRUSH bg = CreateSolidBrush(RGB(28, 28, 28));
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    RECT titleRc = rc;
    titleRc.bottom = std::min(rc.bottom, (LONG)kTitleBarHeight);

    HBRUSH titleBrush = CreateSolidBrush(RGB(38, 38, 38));
    FillRect(hdc, &titleRc, titleBrush);
    DeleteObject(titleBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(235, 235, 235));

    RECT textRc = titleRc;
    textRc.left += 10;
    textRc.right -= 10;
    DrawTextW(hdc, m_title.c_str(), -1, &textRc,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (!m_collapsed) {
        RECT bodyRc = rc;
        bodyRc.top = kTitleBarHeight + kBodyPadding;
        bodyRc.left += kBodyPadding;
        bodyRc.right -= kBodyPadding;
        bodyRc.bottom -= kBodyPadding;

        SetTextColor(hdc, RGB(205, 205, 205));
        if (m_itemLabels.empty()) {
            RECT emptyRc = bodyRc;
            DrawTextW(hdc, L"No items yet", -1, &emptyRc,
                      DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            const bool hasFocus = (GetFocus() == m_hwnd);
            const size_t count = std::min(m_itemLabels.size(), m_itemDrawRects.size());
            for (size_t i = 0; i < count; ++i) {
                const std::wstring& label = m_itemLabels[i];
                RECT itemRc = m_itemDrawRects[i];
                if (itemRc.bottom > bodyRc.bottom) {
                    break;
                }

                if (m_selectionModel && m_selectionModel->IsSelected(static_cast<int>(i))) {
                    HBRUSH selectedBrush = CreateSolidBrush(kSelectionFillColor);
                    FillRect(hdc, &itemRc, selectedBrush);
                    DeleteObject(selectedBrush);

                    HPEN framePen = CreatePen(PS_SOLID, 1, kSelectionFrameColor);
                    HGDIOBJ oldPen = SelectObject(hdc, framePen);
                    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
                    Rectangle(hdc, itemRc.left, itemRc.top, itemRc.right, itemRc.bottom);
                    SelectObject(hdc, oldBrush);
                    SelectObject(hdc, oldPen);
                    DeleteObject(framePen);

                    if (hasFocus) {
                        RECT focusRect = itemRc;
                        InflateRect(&focusRect, -2, -2);
                        DrawFocusRect(hdc, &focusRect);
                    }
                }

                DrawTextW(hdc, label.c_str(), -1, &itemRc,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            if (hasFocus) {
                HPEN focusPen = CreatePen(PS_SOLID, 1, kFocusFrameColor);
                HGDIOBJ oldPen = SelectObject(hdc, focusPen);
                HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
                Rectangle(hdc, bodyRc.left, bodyRc.top, bodyRc.right, bodyRc.bottom);
                SelectObject(hdc, oldBrush);
                SelectObject(hdc, oldPen);
                DeleteObject(focusPen);
            }
        }

        RECT grip{
            rc.right - kResizeGrip,
            rc.bottom - kResizeGrip,
            rc.right,
            rc.bottom
        };
        DrawFrameControl(hdc, &grip, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
    }

    EndPaint(m_hwnd, &ps);
}

LRESULT CALLBACK FenceWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FenceWindow* self = reinterpret_cast<FenceWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (!self && msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<FenceWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->m_hwnd = hwnd;
    }

    if (!self) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return self->HandleMessage(msg, wParam, lParam);
}

LRESULT FenceWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;

    case WM_MOUSEMOVE:
        OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_MOUSELEAVE:
        m_trackingMouse = false;
        if (!m_edit && m_dragMode == DragMode::None) {
            SetTimer(m_hwnd, kCollapseTimer, kCollapseDelayMs, nullptr);
        }
        return 0;

    case WM_TIMER:
        if (wParam == kCollapseTimer) {
            KillTimer(m_hwnd, kCollapseTimer);
            if (!m_edit && m_dragMode == DragMode::None) {
                SetCollapsed(true);
            }
            return 0;
        }
        break;

    case WM_LBUTTONDOWN: {
        SetFocus(m_hwnd);

        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT client{};
        GetClientRect(m_hwnd, &client);
        GetCursorPos(&m_dragStartScreen);

        RECT wr{};
        GetWindowRect(m_hwnd, &wr);
        if (!m_collapsed) {
            m_expandedRect = wr;
        }
        m_dragStartRect = m_expandedRect;

        if (!m_collapsed &&
            pt.x >= client.right - kResizeGrip &&
            pt.y >= client.bottom - kResizeGrip) {
            m_dragMode = DragMode::Resize;
        } else if (pt.y <= kTitleBarHeight) {
            m_dragMode = DragMode::Move;
        } else {
            m_dragMode = DragMode::None;

            if (m_selectionModel) {
                const int hit = HitTestItem(pt);
                if (hit >= 0) {
                    m_selectionModel->SelectSingle(hit);
                } else {
                    m_selectionModel->Clear();
                }
            }
            return 0;
        }

        if (m_dragMode != DragMode::None) {
            SetCapture(m_hwnd);
        }
        return 0;
    }

    case WM_LBUTTONUP:
        if (GetCapture() == m_hwnd) {
            ReleaseCapture();
        }
        if (m_dragMode != DragMode::None && m_onChanged) {
            m_onChanged();
        }
        m_dragMode = DragMode::None;
        return 0;

    case WM_LBUTTONDBLCLK: {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (pt.y <= kTitleBarHeight) {
            BeginRename();
        }
        return 0;
    }

    case WM_RBUTTONUP: {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (pt.y <= kTitleBarHeight) {
            POINT screenPt = pt;
            ClientToScreen(m_hwnd, &screenPt);
            ShowTitleBarContextMenu(screenPt);
            return 0;
        }
        break;
    }

    case WM_COMMAND:
        if ((HWND)lParam == m_edit && HIWORD(wParam) == EN_KILLFOCUS) {
            CommitRename();
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (!m_edit) {
            OnKeyDown(wParam);
            InvalidateRect(m_hwnd, nullptr, TRUE);
            return 0;
        }
        break;

    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(m_hwnd, nullptr, TRUE);
        return 0;

    case WM_PAINT:
        Paint();
        return 0;

    case WM_DROPFILES:
    {
        HDROP drop = (HDROP)wParam;
        UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        std::vector<std::wstring> paths;
        paths.reserve(count);

        wchar_t path[MAX_PATH]{};
        for (UINT i = 0; i < count; ++i) {
            if (DragQueryFileW(drop, i, path, MAX_PATH) > 0) {
                paths.push_back(path);
            }
        }

        DragFinish(drop);

        if (!paths.empty() && m_onFilesDropped) {
            m_onFilesDropped(m_id, paths);
        }
        return 0;
    }

    case WM_NCDESTROY:
    {
        HWND hwndSelf = m_hwnd;
        SetWindowLongPtrW(hwndSelf, GWLP_USERDATA, 0);
        m_hwnd = nullptr;
        return DefWindowProcW(hwndSelf, msg, wParam, lParam);
    }
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}
