#include "DesktopWatcher.h"

#include <array>
#include <chrono>

DesktopWatcher::DesktopWatcher(std::filesystem::path desktopPath, ChangeCallback callback)
    : m_desktopPath(std::move(desktopPath)), m_callback(std::move(callback)) {
}

DesktopWatcher::~DesktopWatcher() {
    Shutdown();
}

bool DesktopWatcher::Initialize() {
    if (m_desktopPath.empty() || m_running.load()) {
        return false;
    }

    m_directoryHandle = CreateFileW(
        m_desktopPath.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);

    if (m_directoryHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    m_running.store(true);
    m_worker = std::thread([this]() {
        WatchLoop();
    });
    return true;
}

void DesktopWatcher::Shutdown() {
    m_running.store(false);

    if (m_directoryHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_directoryHandle);
        m_directoryHandle = INVALID_HANDLE_VALUE;
    }

    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void DesktopWatcher::WatchLoop() {
    std::array<BYTE, 4096> buffer{};
    auto lastCallback = std::chrono::steady_clock::now() - std::chrono::milliseconds(500);

    while (m_running.load()) {
        DWORD bytesReturned = 0;
        const BOOL changed = ReadDirectoryChangesW(
            m_directoryHandle,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME |
                FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_SIZE |
                FILE_NOTIFY_CHANGE_LAST_WRITE |
                FILE_NOTIFY_CHANGE_CREATION,
            &bytesReturned,
            nullptr,
            nullptr);

        if (!changed || !m_running.load()) {
            break;
        }

        if (bytesReturned > 0 && m_callback) {
            const auto now = std::chrono::steady_clock::now();
            if (now - lastCallback >= std::chrono::milliseconds(250)) {
                lastCallback = now;
                m_callback();
            }
        }
    }
}