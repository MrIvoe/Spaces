#include "extensions/FenceExtensionRegistry.h"

FenceExtensionRegistry::FenceExtensionRegistry()
{
    RegisterContentProvider(DefaultFileCollectionProvider());
}

void FenceExtensionRegistry::RegisterContentProvider(const FenceContentProviderDescriptor& provider)
{
    RegisterContentProvider(provider, {});
}

void FenceExtensionRegistry::RegisterContentProvider(const FenceContentProviderDescriptor& provider, const FenceContentProviderCallbacks& callbacks)
{
    for (auto& existing : m_contentProviders)
    {
        if (existing.descriptor.contentType == provider.contentType && existing.descriptor.providerId == provider.providerId)
        {
            existing.callbacks = callbacks;
            return;
        }
    }

    m_contentProviders.push_back(RegisteredProvider{provider, callbacks});
}

std::vector<FenceContentProviderDescriptor> FenceExtensionRegistry::GetContentProviders() const
{
    std::vector<FenceContentProviderDescriptor> items;
    items.reserve(m_contentProviders.size());
    for (const auto& provider : m_contentProviders)
    {
        items.push_back(provider.descriptor);
    }
    return items;
}

bool FenceExtensionRegistry::HasProvider(const std::wstring& contentType, const std::wstring& providerId) const
{
    for (const auto& provider : m_contentProviders)
    {
        if (provider.descriptor.contentType == contentType && provider.descriptor.providerId == providerId)
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
        if (provider.descriptor.contentType == contentType && provider.descriptor.providerId == providerId)
        {
            return provider.descriptor;
        }
    }

    return DefaultFileCollectionProvider();
}

const FenceContentProviderCallbacks* FenceExtensionRegistry::ResolveCallbacks(const std::wstring& contentType, const std::wstring& providerId) const
{
    for (const auto& provider : m_contentProviders)
    {
        if (provider.descriptor.contentType == contentType && provider.descriptor.providerId == providerId)
        {
            return &provider.callbacks;
        }
    }

    return nullptr;
}

FenceContentProviderDescriptor FenceExtensionRegistry::DefaultFileCollectionProvider() const
{
    return FenceContentProviderDescriptor{L"core.file_collection", L"file_collection", L"File Collection", true};
}
