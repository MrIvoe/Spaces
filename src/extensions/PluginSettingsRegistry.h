#pragma once

#include "extensions/SettingsSchema.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class SettingsStore;

struct PluginSettingsPage
{
    std::wstring pluginId;
    std::wstring pageId;
    std::wstring title;
    int order = 0;
    std::vector<SettingsFieldDescriptor> fields; // interactive controls for this page
};

class PluginSettingsRegistry
{
public:
    using SettingsObserver = std::function<void(const std::wstring& key, const std::wstring& value)>;

    // Called by AppKernel to attach persistent storage.
    void SetStore(SettingsStore* store);

    bool RegisterPage(const PluginSettingsPage& page);
    std::vector<PluginSettingsPage> GetAllPages() const;
    void ClearPages();

    // Read / write a persisted setting value.
    // If no SettingsStore is attached the in-memory map is used (session-only).
    std::wstring GetValue(const std::wstring& key, const std::wstring& defaultValue) const;
    void         SetValue(const std::wstring& key, const std::wstring& value);

    int RegisterObserver(SettingsObserver observer);
    void UnregisterObserver(int observerToken);

private:
    std::vector<PluginSettingsPage> m_pages;
    SettingsStore* m_store = nullptr;
    // Fallback in-memory map used when no store is attached.
    mutable std::unordered_map<std::wstring, std::wstring> m_memValues;
    int m_nextObserverToken = 1;
    std::unordered_map<int, SettingsObserver> m_observers;
};
