#include "core/CommandDispatcher.h"

#include <exception>

#include <windows.h>

namespace
{
    std::wstring Utf8ToWString(const std::string& text)
    {
        if (text.empty())
        {
            return {};
        }

        const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
        if (size <= 0)
        {
            return L"(message conversion failed)";
        }

        std::wstring wide(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), wide.data(), size);
        return wide;
    }
}

bool CommandDispatcher::RegisterCommand(const std::wstring& commandId, CommandHandler handler, bool replaceExisting)
{
    if (commandId.empty() || !handler)
    {
        return false;
    }

    const auto it = m_handlers.find(commandId);
    if (it != m_handlers.end() && !replaceExisting)
    {
        return false;
    }

    m_handlers[commandId] = std::move(handler);
    return true;
}

bool CommandDispatcher::RegisterCommand(const std::wstring& commandId, std::function<void()> handler, bool replaceExisting)
{
    if (!handler)
    {
        return false;
    }

    return RegisterCommand(commandId, [handler = std::move(handler)](const CommandContext&) {
        handler();
    }, replaceExisting);
}

bool CommandDispatcher::Dispatch(const std::wstring& commandId) const
{
    const auto result = DispatchDetailed(commandId);
    return result.handled && result.succeeded;
}

bool CommandDispatcher::Dispatch(const std::wstring& commandId, const CommandContext& context) const
{
    const auto result = DispatchDetailed(commandId, context);
    return result.handled && result.succeeded;
}

CommandDispatchResult CommandDispatcher::DispatchDetailed(const std::wstring& commandId) const
{
    CommandContext context;
    context.commandId = commandId;
    return DispatchDetailed(commandId, context);
}

CommandDispatchResult CommandDispatcher::DispatchDetailed(const std::wstring& commandId, const CommandContext& context) const
{
    CommandDispatchResult result;

    const auto it = m_handlers.find(commandId);
    if (it == m_handlers.end())
    {
        return result;
    }

    result.handled = true;

    try
    {
        CommandContext effectiveContext = context;
        if (effectiveContext.commandId.empty())
        {
            effectiveContext.commandId = commandId;
        }

        it->second(effectiveContext);
        result.succeeded = true;
    }
    catch (const std::exception& ex)
    {
        result.error = L"Command threw exception: " + Utf8ToWString(ex.what());
    }
    catch (...)
    {
        result.error = L"Command threw unknown exception.";
    }

    return result;
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
