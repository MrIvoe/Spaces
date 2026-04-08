#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <windows.h>
#include <vector>

struct SpaceModel;
class SpaceWindow;

/// Manages all space creation, deletion, and lifecycle
class SpaceManager {
public:
    SpaceManager();
    ~SpaceManager();

    /// Create a new space
    bool CreateSpace(const std::wstring& title, const RECT& rect, const std::wstring& viewProviderId, std::wstring& outSpaceId);

    /// Delete a space
    bool DeleteSpace(const std::wstring& spaceId);

    /// Get space by ID
    SpaceModel* GetSpace(const std::wstring& spaceId);

    /// Find space at screen coordinates
    SpaceModel* FindSpaceAtPoint(int screenX, int screenY);

    /// Get space window HWND
    HWND GetSpaceHwnd(const std::wstring& spaceId);

    /// Restore spaces from configuration
    bool RestoreSpaces(const std::vector<SpaceModel>& savedSpaces);

    /// Get all spaces
    const std::unordered_map<std::wstring, std::unique_ptr<SpaceModel>>& GetSpaces() const { return m_spaces; }

    /// Save all space states
    bool SaveAllSpaces();

    /// Set space rolled-up state
    bool SetSpaceRolledUp(const std::wstring& spaceId, bool rolledUp);

    /// Set space opacity
    bool SetSpaceOpacity(const std::wstring& spaceId, float opacity);

private:
    std::wstring GenerateSpaceId();

    std::unordered_map<std::wstring, std::unique_ptr<SpaceModel>> m_spaces;
    std::unordered_map<std::wstring, HWND> m_spaceHwnds;
    uint32_t m_spaceCounter;
};
