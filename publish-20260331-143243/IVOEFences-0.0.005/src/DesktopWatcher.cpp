#include "DesktopWatcher.h"

DesktopWatcher::DesktopWatcher(std::filesystem::path desktopPath, ChangeCallback callback)
    : m_desktopPath(std::move(desktopPath)), m_callback(std::move(callback)) {
}

bool DesktopWatcher::Initialize() {
    // Phase 2 scaffold: watcher wiring exists but no file-system events are emitted yet.
    m_initialized = !m_desktopPath.empty();
    return m_initialized;
}

void DesktopWatcher::Shutdown() {
    m_initialized = false;
}