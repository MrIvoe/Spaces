#include "extensions/FenceExtensionRegistry.h"

FenceExtensionRegistry::FenceExtensionRegistry()
{
    RegisterContentProvider(DefaultFileCollectionProvider());
}

void FenceExtensionRegistry::RegisterContentProvider(const FenceContentProviderDescriptor& provider)
{
    for (const auto& existing : m_contentProviders)
    {
        if (existing.contentType == provider.contentType && existing.providerId == provider.providerId)
        {
            return;
        }
    }

    m_contentProviders.push_back(provider);
}

std::vector<FenceContentProviderDescriptor> FenceExtensionRegistry::GetContentProviders() const
{
    return m_contentProviders;
}

bool FenceExtensionRegistry::HasProvider(const std::wstring& contentType, const std::wstring& providerId) const
{
    for (const auto& provider : m_contentProviders)
    {
        if (provider.contentType == contentType && provider.providerId == providerId)
        {
            return true;
        }
    }

    return false;
}

FenceContentProviderDescriptor FenceExtensionRegistry::ResolveOrDefault(const std::wstring& contentType, const std::wstring& providerId) const
{
    for (const auto& provider : m_contentProviders)
    {
        if (provider.contentType == contentType && provider.providerId == providerId)
        {
            return provider;
        }
    }

    return DefaultFileCollectionProvider();
}

FenceContentProviderDescriptor FenceExtensionRegistry::DefaultFileCollectionProvider() const
{
    return FenceContentProviderDescriptor{L"core.file_collection", L"file_collection", L"File Collection", true};
}
