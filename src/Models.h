#pragma once
#include <filesystem>
#include <string>
#include <vector>

struct FenceItem
{
    std::wstring name;
    std::wstring fullPath;
    std::wstring originalPath;  // Where the file came from before being added to fence
    bool isDirectory = false;
    int iconIndex = 0;          // Index into system image list for small icons
};

struct FenceModel
{
    std::wstring id;
    std::wstring title;
    int x = 100;
    int y = 100;
    int width = 320;
    int height = 240;
    std::wstring backingFolder;
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
    std::vector<std::pair<std::filesystem::path, std::wstring>> failedItems;

    bool AllSucceeded() const { return failedCount == 0; }
};
