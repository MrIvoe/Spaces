#pragma once

#include <windows.h>
#include <string>
#include <memory>

/// Handles drag/drop of items between desktop and spaces
class DragDropController {
public:
    DragDropController();
    ~DragDropController();

    /// Initialize drag/drop system
    bool Initialize();

    /// Handle drop event on space
    bool HandleDropOnSpace(HWND spaceHwnd, const std::wstring& spaceId, HDROP hDrop);

    /// Handle drop event on desktop
    bool HandleDropOnDesktop(HDROP hDrop);

    /// Check if drag is from space or external
    bool IsDragFromSpace(HWND sourceHwnd);

private:
    bool ExtractDroppedPaths(HDROP hDrop, std::vector<std::wstring>& outPaths);
};
