#include "core/EventBus.h"

#include <algorithm>

int EventBus::Subscribe(const std::wstring& eventName, EventHandler handler)
{
    if (eventName.empty() || !handler)
    {
        return 0;
    }

    const int id = m_nextId++;
    m_subscriptions.push_back(Subscription{id, eventName, std::move(handler)});
    return id;
}

void EventBus::Unsubscribe(int subscriptionId)
{
    m_subscriptions.erase(
        std::remove_if(
            m_subscriptions.begin(),
            m_subscriptions.end(),
            [subscriptionId](const Subscription& item) { return item.id == subscriptionId; }),
        m_subscriptions.end());
}

void EventBus::Publish(const std::wstring& eventName) const
{
    for (const auto& subscription : m_subscriptions)
    {
        if (subscription.eventName == eventName)
        {
            subscription.handler(eventName);
        }
    }
}
