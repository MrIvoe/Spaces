#pragma once

#include "core/ThemePlatform.h"

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace Win32Helpers
{
    struct PopupMenuItemVisual
    {
        std::wstring text;
        bool separator = false;
        bool enabled = true;
    };

    std::wstring GetAppDataPath();
    std::wstring GetLocalAppDataPath();
    std::filesystem::path GetAppDataRoot();
    std::filesystem::path GetFencesRoot();
    std::filesystem::path GetConfigPath();
    std::filesystem::path GetDebugLogPath();
    bool ReplaceFileAtomically(const std::filesystem::path& tempPath, const std::filesystem::path& targetPath);
    void ShowUserWarning(HWND owner, const std::wstring& title, const std::wstring& message);
    bool PromptTextInput(HWND owner,
                         const std::wstring& title,
                         const std::wstring& label,
                         const std::wstring& initialValue,
                         std::wstring& resultValue);
    bool PromptSelectFolder(HWND owner, const std::wstring& title, std::wstring& selectedPath);
    bool PromptOpenJsonFile(HWND owner, const std::wstring& title, std::wstring& selectedPath);
    bool PromptOpenThemeImportFile(HWND owner, const std::wstring& title, std::wstring& selectedPath);
    bool PromptSaveJsonFile(HWND owner, const std::wstring& title, std::wstring& selectedPath);
    void MeasureThemedPopupMenuItem(MEASUREITEMSTRUCT* measureInfo, const PopupMenuItemVisual& item);
    void DrawThemedPopupMenuItem(const DRAWITEMSTRUCT* drawInfo, const ThemePalette& palette, const PopupMenuItemVisual& item);

    void LogInfo(const std::wstring& message);
    void LogError(const std::wstring& message);

    // Lightweight in-process telemetry counters for observability.
    void IncrementTelemetryCounter(const std::wstring& counterName);
    uint64_t GetTelemetryCounterValue(const std::wstring& counterName);
    void ResetTelemetryCounters();

    POINT GetCursorPos();
    void CenterWindowNearCursor(HWND hwnd);
}

