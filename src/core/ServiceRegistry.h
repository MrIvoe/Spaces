#pragma once

#include <memory>
#include <string>
#include <unordered_map>

class ServiceRegistry
{
public:
    template <typename T>
    void Register(const std::wstring& key, std::shared_ptr<T> service)
    {
        m_services[key] = service;
    }

    template <typename T>
    std::shared_ptr<T> Get(const std::wstring& key) const
    {
        const auto it = m_services.find(key);
        if (it == m_services.end())
        {
            return nullptr;
        }

        return std::static_pointer_cast<T>(it->second);
    }

private:
    std::unordered_map<std::wstring, std::shared_ptr<void>> m_services;
};
