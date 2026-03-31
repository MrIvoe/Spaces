#pragma once

#include <windows.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

/// Describes a command that can be invoked
struct Command {
    std::wstring commandId;
    std::wstring displayName;
    std::function<void()> handler;
};

/// Registry for application commands
class CommandRegistry {
public:
    CommandRegistry();
    ~CommandRegistry();

    /// Register a command
    bool RegisterCommand(const std::wstring& commandId, const std::wstring& displayName, std::function<void()> handler);

    /// Execute a command by ID
    bool ExecuteCommand(const std::wstring& commandId);

    /// Get all registered commands
    const std::vector<Command>& GetAllCommands() const { return m_commands; }

    /// Get command by ID
    const Command* GetCommand(const std::wstring& commandId);

private:
    std::vector<Command> m_commands;
    std::unordered_map<std::wstring, size_t> m_commandMap;  // ID -> index
};
