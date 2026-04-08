#include "extensions/SpaceExtensionRegistry.h"

SpaceExtensionRegistry::SpaceExtensionRegistry()
{
    RegisterContentProvider(DefaultFileCollectionProvider());
}

void SpaceExtensionRegistry::RegisterContentProvider(const SpaceContentProviderDescriptor& provider)
{
    RegisterContentProvider(provider, {});
}

void SpaceExtensionRegistry::RegisterContentProvider(const SpaceContentProviderDescriptor& provider, const SpaceContentProviderCallbacks& callbacks)
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

std::vector<SpaceContentProviderDescriptor> SpaceExtensionRegistry::GetContentProviders() const
{
    std::vector<SpaceContentProviderDescriptor> items;
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

SpaceContentProviderDescriptor SpaceExtensionRegistry::ResolveOrDefault(const std::wstring& contentType, const std::wstring& providerId) const
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

const SpaceContentProviderCallbacks* SpaceExtensionRegistry::ResolveCallbacks(const std::wstring& contentType, const std::wstring& providerId) const
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

SpaceContentProviderDescriptor SpaceExtensionRegistry::DefaultFileCollectionProvider() const
{
    return SpaceContentProviderDescriptor{L"core.file_collection", L"file_collection", L"File Collection", true};
}
