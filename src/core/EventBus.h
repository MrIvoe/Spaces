#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class EventBus
{
public:
    using EventHandler = std::function<void(const std::wstring& eventName)>;

    int Subscribe(const std::wstring& eventName, EventHandler handler);
    void Unsubscribe(int subscriptionId);
    void Publish(const std::wstring& eventName) const;

private:
    struct Subscription
    {
        int id = 0;
        std::wstring eventName;
        EventHandler handler;
    };

    int m_nextId = 1;
    std::vector<Subscription> m_subscriptions;
};
