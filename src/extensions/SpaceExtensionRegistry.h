#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Models.h"
#include "extensions/PluginContracts.h"

struct FenceContentProviderDescriptor
{
    std::wstring providerId;
    std::wstring contentType;
    std::wstring displayName;
    bool isCoreDefault = false;
};

struct FenceContentProviderCallbacks
{
    std::function<std::vector<FenceItem>(const FenceMetadata& fence)> enumerateItems;
    std::function<bool(const FenceMetadata& fence, const std::vector<std::wstring>& paths)> handleDrop;
    std::function<bool(const FenceMetadata& fence, const FenceItem& item)> deleteItem;
};

class SpaceExtensionRegistry
{
public:
    SpaceExtensionRegistry();

    void RegisterContentProvider(const FenceContentProviderDescriptor& provider);
    void RegisterContentProvider(const FenceContentProviderDescriptor& provider, const FenceContentProviderCallbacks& callbacks);
    std::vector<FenceContentProviderDescriptor> GetContentProviders() const;
    bool HasProvider(const std::wstring& contentType, const std::wstring& providerId) const;
    FenceContentProviderDescriptor ResolveOrDefault(const std::wstring& contentType, const std::wstring& providerId) const;
    const FenceContentProviderCallbacks* ResolveCallbacks(const std::wstring& contentType, const std::wstring& providerId) const;

private:
    FenceContentProviderDescriptor DefaultFileCollectionProvider() const;

    struct RegisteredProvider
    {
        FenceContentProviderDescriptor descriptor;
        FenceContentProviderCallbacks callbacks;
    };

    std::vector<RegisteredProvider> m_contentProviders;
};
