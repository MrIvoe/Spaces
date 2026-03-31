#include "ShellDesktop.h"

namespace {

HWND FindWorkerWWithDefView() {
    HWND worker = nullptr;
    while ((worker = FindWindowExW(nullptr, worker, L"WorkerW", nullptr)) != nullptr) {
        if (FindWindowExW(worker, nullptr, L"SHELLDLL_DefView", nullptr)) {
            return worker;
        }
    }

    return nullptr;
}

HWND FindNextWorkerW(HWND after) {
    return FindWindowExW(nullptr, after, L"WorkerW", nullptr);
}

} // namespace

ShellDesktop::HostInfo ShellDesktop::FindDesktopHostInfo() {
    HostInfo info{};

    HWND progman = FindWindowW(L"Progman", nullptr);
    if (progman) {
        SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);
    }

    HWND singleWorker = FindWorkerWWithDefView();
    if (singleWorker) {
        info.host = singleWorker;
        info.mode = HostMode::SingleWorkerW;

        HWND doubleWorker = FindNextWorkerW(singleWorker);
        if (doubleWorker) {
            info.host = doubleWorker;
            info.mode = HostMode::DoubleWorkerW;
        }

        return info;
    }

    info.host = progman;
    info.mode = HostMode::ProgmanFallback;
    return info;
}

HWND ShellDesktop::FindDesktopHost() {
    return FindDesktopHostInfo().host;
}
