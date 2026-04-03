#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "extensions/PluginContracts.h"

struct CommandDispatchResult
{
    bool handled = false;
    bool succeeded = false;
    std::wstring error;
};

class CommandDispatcher
{
public:
    using CommandHandler = std::function<void(const CommandContext&)>;

    bool RegisterCommand(const std::wstring& commandId, CommandHandler handler, bool replaceExisting = false);
    bool RegisterCommand(const std::wstring& commandId, std::function<void()> handler, bool replaceExisting = false);
    bool Dispatch(const std::wstring& commandId) const;
    bool Dispatch(const std::wstring& commandId, const CommandContext& context) const;
    CommandDispatchResult DispatchDetailed(const std::wstring& commandId) const;
    CommandDispatchResult DispatchDetailed(const std::wstring& commandId, const CommandContext& context) const;
    bool HasCommand(const std::wstring& commandId) const;
    std::vector<std::wstring> ListCommandIds() const;

private:
    std::unordered_map<std::wstring, CommandHandler> m_handlers;
};
