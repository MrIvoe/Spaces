#pragma once

#include <windows.h>
#include <string>
#include <vector>

enum class DropPolicy {
    Move = 0,
    Copy = 1,
    Prompt = 2,
};

enum class FenceType {
    Standard = 0,
    Portal = 1,
};

enum class DesktopItemSource {
    Desktop = 0,
    LegacyFenceFolder = 1,
    PortalFolder = 2,
};

struct DesktopItemRef {
    std::wstring itemId;
    std::wstring displayName;
    std::wstring sourcePath;
    DesktopItemSource source{DesktopItemSource::Desktop};
};

struct FenceData {
    int id{};
    std::wstring title{L"Fence"};
    RECT rect{40, 40, 400, 340};
    bool collapsed{false};
    FenceType type{FenceType::Standard};
    std::wstring portalFolder;
    std::wstring backingFolder;
    std::vector<DesktopItemRef> members;
};