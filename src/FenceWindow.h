#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "Models.h"

class FenceManager;

class FenceWindow
{
public:
    FenceWindow(FenceManager* manager, const FenceModel& model);
    ~FenceWindow();

    bool Create(HWND parent = nullptr);
    void Show();
    void Destroy();
    void UpdateModel(const FenceModel& model);
    void SetItems(const std::vector<FenceItem>& items);

    HWND GetHwnd() const;
    const std::wstring& GetFenceId() const;
    const FenceModel& GetModel() const;

private:
    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnPaint();
    void OnLButtonDown(int x, int y);
    void OnMouseMove(int x, int y, WPARAM flags);
    void OnLButtonUp();
    void OnLButtonDblClk(int x, int y);
    void OnContextMenu(int x, int y);
    void OnDropFiles(HDROP hDrop);
    void OnMove(int x, int y);
    void OnSize(int width, int height);

    int GetItemAtPosition(int x, int y) const;
    void ExecuteItem(int itemIndex);
    bool InitializeImageList();

private:
    FenceManager* m_manager;
    FenceModel m_model;
    std::vector<FenceItem> m_items;
    HWND m_hwnd = nullptr;
    HIMAGELIST m_imageList = nullptr;  // System image list for file icons
    bool m_dragging = false;
    POINT m_dragStart{};
    RECT m_windowStart{};
    int m_selectedItem = -1;
};
