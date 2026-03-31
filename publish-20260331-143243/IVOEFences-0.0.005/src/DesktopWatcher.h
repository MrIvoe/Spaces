#pragma once

#include <filesystem>
#include <functional>

class DesktopWatcher {
public:
    using ChangeCallback = std::function<void()>;

    DesktopWatcher(std::filesystem::path desktopPath, ChangeCallback callback);

    bool Initialize();
    void Shutdown();

private:
    std::filesystem::path m_desktopPath;
    ChangeCallback m_callback;
    bool m_initialized{false};
};