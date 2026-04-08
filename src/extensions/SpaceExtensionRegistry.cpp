#include "extensions/SpaceExtensionRegistry.h"

SpaceExtensionRegistry::SpaceExtensionRegistry()
{
    RegisterContentProvider(DefaultFileCollectionProvider());
}

void SpaceExtensionRegistry::RegisterContentProvider(const FenceContentProviderDescriptor& provider)
{
    RegisterContentProvider(provider, {});
}

void SpaceExtensionRegistry::RegisterContentProvider(const FenceContentProviderDescriptor& provider, const FenceContentProviderCallbacks& callbacks)
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

std::vector<FenceContentProviderDescriptor> SpaceExtensionRegistry::GetContentProviders() const
{
    std::vector<FenceContentProviderDescriptor> items;
    items.reserve(m_contentProviders.size());
    for (const auto& provider : m_contentProviders)
    {
        items.push_back(provider.descriptor);
    }
    return items;
}

bool SpaceExtensionRegistry::HasProvider(const std::wstring& contentType, const std::wstring& providerId) const
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

FenceContentProviderDescriptor SpaceExtensionRegistry::ResolveOrDefault(const std::wstring& contentType, const std::wstring& providerId) const
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

const FenceContentProviderCallbacks* SpaceExtensionRegistry::ResolveCallbacks(const std::wstring& contentType, const std::wstring& providerId) const
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

FenceContentProviderDescriptor SpaceExtensionRegistry::DefaultFileCollectionProvider() const
{
    return FenceContentProviderDescriptor{L"core.file_collection", L"file_collection", L"File Collection", true};
}
