#pragma once

#include <windows.h>
#include <string>
#include <vector>

enum class DropPolicy {
    Move = 0,
    Copy = 1,
    Prompt = 2,
};

enum class SpaceType {
    Standard = 0,
    Portal = 1,
};

enum class DesktopItemSource {
    Desktop = 0,
    LegacySpaceFolder = 1,
    PortalFolder = 2,
};

struct DesktopItemRef {
    std::wstring itemId;
    std::wstring displayName;
    std::wstring sourcePath;
    DesktopItemSource source{DesktopItemSource::Desktop};
    bool isFolder{false};
    bool exists{true};
    int iconIndex{-1};
};

struct SpaceData {
    int id{};
    std::wstring title{L"Space"};
    RECT rect{40, 40, 400, 340};
    bool collapsed{false};
    SpaceType type{SpaceType::Standard};
    std::wstring portalFolder;
    std::wstring backingFolder;
    std::vector<DesktopItemRef> members;
};