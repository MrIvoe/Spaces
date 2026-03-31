#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <thread>

#include <windows.h>

class DesktopWatcher {
public:
    using ChangeCallback = std::function<void()>;

    DesktopWatcher(std::filesystem::path desktopPath, ChangeCallback callback);
    ~DesktopWatcher();

    bool Initialize();
    void Shutdown();

private:
    void WatchLoop();

private:
    std::filesystem::path m_desktopPath;
    ChangeCallback m_callback;
    std::atomic<bool> m_running{false};
    HANDLE m_directoryHandle{INVALID_HANDLE_VALUE};
    std::thread m_worker;
};