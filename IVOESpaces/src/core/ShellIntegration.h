#pragma once

#include <windows.h>
#include <string>
#include <memory>

/// Discovers and manages Windows shell desktop integration
class ShellIntegration {
public:
    ShellIntegration();
    ~ShellIntegration();

    /// Initialize shell integration - locate desktop windows
    bool Initialize();

    /// Get the Progman window (main desktop window)
    HWND GetProgman() const { return m_hProgman; }

    /// Get the WorkerW window (where we should parent space windows)
    HWND GetWorkerW() const { return m_hWorkerW; }

    /// Get SHELLDLL_DefView (shell desktop view)
    HWND GetShellDefView() const { return m_hShellDefView; }

    /// Get SysListView32 (system list view for desktop items)
    HWND GetDesktopListView() const { return m_hDesktopListView; }

    /// Watch for Explorer restart and rebind windows
    bool WatchForExplorerRestart();

    /// Refresh window references (call if Explorer crashes/restarts)
    bool RefreshWindowReferences();

private:
    bool DiscoverDesktopWindows();
    bool SendSpawnWorkerWMessage();
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

    HWND m_hProgman;
    HWND m_hWorkerW;
    HWND m_hShellDefView;
    HWND m_hDesktopListView;
};
