#pragma once

#include <string>
#include <vector>

struct FenceContentProviderDescriptor
{
    std::wstring providerId;
    std::wstring contentType;
    std::wstring displayName;
    bool isCoreDefault = false;
};

class FenceExtensionRegistry
{
public:
    FenceExtensionRegistry();

    void RegisterContentProvider(const FenceContentProviderDescriptor& provider);
    std::vector<FenceContentProviderDescriptor> GetContentProviders() const;
    bool HasProvider(const std::wstring& contentType, const std::wstring& providerId) const;
    FenceContentProviderDescriptor ResolveOrDefault(const std::wstring& contentType, const std::wstring& providerId) const;

private:
    FenceContentProviderDescriptor DefaultFileCollectionProvider() const;

    std::vector<FenceContentProviderDescriptor> m_contentProviders;
};
