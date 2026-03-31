#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <windows.h>
#include <vector>

struct FenceModel;
class FenceWindow;

/// Manages all fence creation, deletion, and lifecycle
class FenceManager {
public:
    FenceManager();
    ~FenceManager();

    /// Create a new fence
    bool CreateFence(const std::wstring& title, const RECT& rect, const std::wstring& viewProviderId, std::wstring& outFenceId);

    /// Delete a fence
    bool DeleteFence(const std::wstring& fenceId);

    /// Get fence by ID
    FenceModel* GetFence(const std::wstring& fenceId);

    /// Find fence at screen coordinates
    FenceModel* FindFenceAtPoint(int screenX, int screenY);

    /// Get fence window HWND
    HWND GetFenceHwnd(const std::wstring& fenceId);

    /// Restore fences from configuration
    bool RestoreFences(const std::vector<FenceModel>& savedFences);

    /// Get all fences
    const std::unordered_map<std::wstring, std::unique_ptr<FenceModel>>& GetFences() const { return m_fences; }

    /// Save all fence states
    bool SaveAllFences();

    /// Set fence rolled-up state
    bool SetFenceRolledUp(const std::wstring& fenceId, bool rolledUp);

    /// Set fence opacity
    bool SetFenceOpacity(const std::wstring& fenceId, float opacity);

private:
    std::wstring GenerateFenceId();

    std::unordered_map<std::wstring, std::unique_ptr<FenceModel>> m_fences;
    std::unordered_map<std::wstring, HWND> m_fenceHwnds;
    uint32_t m_fenceCounter;
};
