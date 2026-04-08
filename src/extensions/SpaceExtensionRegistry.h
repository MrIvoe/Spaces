#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Models.h"
#include "extensions/PluginContracts.h"

struct SpaceContentProviderDescriptor
{
    std::wstring providerId;
    std::wstring contentType;
    std::wstring displayName;
    bool isCoreDefault = false;
};

struct SpaceContentProviderCallbacks
{
    std::function<std::vector<SpaceItem>(const SpaceMetadata& space)> enumerateItems;
    std::function<bool(const SpaceMetadata& space, const std::vector<std::wstring>& paths)> handleDrop;
    std::function<bool(const SpaceMetadata& space, const SpaceItem& item)> deleteItem;
};

class SpaceExtensionRegistry
{
public:
    SpaceExtensionRegistry();

    void RegisterContentProvider(const SpaceContentProviderDescriptor& provider);
    void RegisterContentProvider(const SpaceContentProviderDescriptor& provider, const SpaceContentProviderCallbacks& callbacks);
    std::vector<SpaceContentProviderDescriptor> GetContentProviders() const;
    bool HasProvider(const std::wstring& contentType, const std::wstring& providerId) const;
    SpaceContentProviderDescriptor ResolveOrDefault(const std::wstring& contentType, const std::wstring& providerId) const;
    const SpaceContentProviderCallbacks* ResolveCallbacks(const std::wstring& contentType, const std::wstring& providerId) const;

private:
    SpaceContentProviderDescriptor DefaultFileCollectionProvider() const;

    struct RegisteredProvider
    {
        SpaceContentProviderDescriptor descriptor;
        SpaceContentProviderCallbacks callbacks;
    };

    std::vector<RegisteredProvider> m_contentProviders;
};
