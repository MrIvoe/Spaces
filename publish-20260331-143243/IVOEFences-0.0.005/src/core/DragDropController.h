#pragma once

#include <windows.h>
#include <string>
#include <memory>

/// Handles drag/drop of items between desktop and fences
class DragDropController {
public:
    DragDropController();
    ~DragDropController();

    /// Initialize drag/drop system
    bool Initialize();

    /// Handle drop event on fence
    bool HandleDropOnFence(HWND fenceHwnd, const std::wstring& fenceId, HDROP hDrop);

    /// Handle drop event on desktop
    bool HandleDropOnDesktop(HDROP hDrop);

    /// Check if drag is from fence or external
    bool IsDragFromFence(HWND sourceHwnd);

private:
    bool ExtractDroppedPaths(HDROP hDrop, std::vector<std::wstring>& outPaths);
};
