#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "FenceModels.h"

class FenceLayoutEngine;
class FenceSelectionModel;

class FenceWindow {
public:
    struct CreateParams {
        HINSTANCE instance{};
        HWND parent{};
        int id{};
        RECT rect{};
        std::wstring title{L"Fence"};
        bool collapsed{};
        std::function<void()> onChanged;
        std::function<void(int, const std::vector<std::wstring>&)> onFilesDropped;
        std::function<void(int)> onOpenBackingFolder;
        std::function<void(int)> onDeleteFence;
        std::function<bool(int)> canDeleteFence;
        std::wstring backingFolder;
    };

    FenceWindow();
    ~FenceWindow();

    static bool RegisterClass(HINSTANCE instance);
    bool Create(const CreateParams& params);

    HWND GetHwnd() const { return m_hwnd; }
    int GetId() const { return m_id; }
    const std::wstring& GetTitle() const { return m_title; }
    RECT GetExpandedRect() const { return m_expandedRect; }
    bool IsCollapsed() const { return m_collapsed; }
    const std::wstring& GetBackingFolder() const { return m_backingFolder; }
    void SetBackingFolder(const std::wstring& path);
    void SetItemLabels(std::vector<std::wstring> labels);
    FenceData ToFenceData() const;

    void SetCollapsed(bool collapsed);
    void BeginRename();

private:
    enum class DragMode {
        None,
        Move,
        Resize
    };

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void Paint();
    void StartTrackingMouse();
    void CommitRename();
    void UpdateWindowBounds();
    void OnMouseMove(int x, int y, WPARAM keys);
    void OnKeyDown(WPARAM key);
    void ShowTitleBarContextMenu(POINT screenPt);
    void RebuildLayoutCache();
    int HitTestItem(POINT clientPt) const;

private:
    HWND m_hwnd{};
    HWND m_edit{};
    HINSTANCE m_instance{};
    HWND m_parent{};
    int m_id{};
    std::wstring m_title{L"Fence"};
    RECT m_expandedRect{40, 40, 400, 340};
    bool m_collapsed{false};
    bool m_trackingMouse{false};
    DragMode m_dragMode{DragMode::None};
    POINT m_dragStartScreen{};
    RECT m_dragStartRect{};
    std::function<void()> m_onChanged;
    std::function<void(int, const std::vector<std::wstring>&)> m_onFilesDropped;
    std::function<void(int)> m_onOpenBackingFolder;
    std::function<void(int)> m_onDeleteFence;
    std::function<bool(int)> m_canDeleteFence;
    std::wstring m_backingFolder;
    std::vector<std::wstring> m_itemLabels;
    std::unique_ptr<FenceLayoutEngine> m_layoutEngine;
    std::unique_ptr<FenceSelectionModel> m_selectionModel;
    std::vector<RECT> m_itemDrawRects;
};
