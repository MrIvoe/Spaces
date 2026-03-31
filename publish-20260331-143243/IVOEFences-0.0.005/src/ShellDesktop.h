#pragma once
#include <windows.h>

class ShellDesktop {
public:
    enum class HostMode {
        SingleWorkerW,
        DoubleWorkerW,
        ProgmanFallback
    };

    struct HostInfo {
        HWND host{};
        HostMode mode{HostMode::ProgmanFallback};
    };

    static HostInfo FindDesktopHostInfo();
    static HWND FindDesktopHost();
};
