#pragma once
#include <filesystem>
#include <string>
#include <vector>

struct SpaceItem
{
    std::wstring name;
    std::wstring fullPath;
    std::wstring originalPath;  // Where the file came from before being added to space
    bool isDirectory = false;
    int iconIndex = 0;          // Index into system image list for small icons
};

struct SpaceModel
{
    std::wstring id;
    std::wstring title;
    int x = 100;
    int y = 100;
    int width = 320;
    int height = 240;
    std::wstring backingFolder;
    std::wstring contentType = L"file_collection";
    std::wstring contentPluginId = L"core.file_collection";
    std::wstring contentSource;
    std::wstring contentState = L"ready";
    std::wstring contentStateDetail;
    std::wstring appearanceProfileId;
    std::wstring widgetLayoutId;

    // Per-space presentation controls.
    bool textOnlyMode = false;                  // false => icon-first desktop style
    bool rollupWhenNotHovered = false;
    bool transparentWhenNotHovered = false;
    bool labelsOnHover = true;
    std::wstring iconSpacingPreset = L"comfortable"; // compact | comfortable | spacious
    bool inheritThemePolicy = true;
};

struct FileMoveResult
{
    std::vector<std::filesystem::path> moved;
    std::vector<std::pair<std::filesystem::path, std::wstring>> failed;

    bool HasFailures() const { return !failed.empty(); }
};

struct RestoreResult
{
    int restoredCount = 0;
    int failedCount = 0;
    std::vector<std::filesystem::path> restoredItems;
    std::vector<std::pair<std::filesystem::path, std::wstring>> failedItems;

    bool AllSucceeded() const { return failedCount == 0; }
};
