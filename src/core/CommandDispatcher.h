#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class CommandDispatcher
{
public:
    using CommandHandler = std::function<void()>;

    bool RegisterCommand(const std::wstring& commandId, CommandHandler handler);
    bool Dispatch(const std::wstring& commandId) const;
    bool HasCommand(const std::wstring& commandId) const;
    std::vector<std::wstring> ListCommandIds() const;

private:
    std::unordered_map<std::wstring, CommandHandler> m_handlers;
};
