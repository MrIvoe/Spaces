#include "core/CommandDispatcher.h"

bool CommandDispatcher::RegisterCommand(const std::wstring& commandId, CommandHandler handler)
{
    if (commandId.empty() || !handler)
    {
        return false;
    }

    m_handlers[commandId] = std::move(handler);
    return true;
}

bool CommandDispatcher::Dispatch(const std::wstring& commandId) const
{
    const auto it = m_handlers.find(commandId);
    if (it == m_handlers.end())
    {
        return false;
    }

    it->second();
    return true;
}

bool CommandDispatcher::HasCommand(const std::wstring& commandId) const
{
    return m_handlers.find(commandId) != m_handlers.end();
}

std::vector<std::wstring> CommandDispatcher::ListCommandIds() const
{
    std::vector<std::wstring> ids;
    ids.reserve(m_handlers.size());
    for (const auto& entry : m_handlers)
    {
        ids.push_back(entry.first);
    }
    return ids;
}
