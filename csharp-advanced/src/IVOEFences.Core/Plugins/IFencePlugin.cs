using IVOEFences.Core.Models;
using IVOEFences.Core.Services;

namespace IVOEFences.Core.Plugins;

/// <summary>
/// Interface implemented by all IVOE Fences plug-ins.
/// Place compiled plugin DLLs in the "plugins/" folder next to IVOEFences.dll.
/// The assembly must contain at least one non-abstract type implementing IFencePlugin.
/// </summary>
public interface IFencePlugin
{
    /// <summary>Stable reverse-domain identifier, e.g. "com.example.myplugin".</summary>
    string Id      { get; }

    string Name    { get; }
    string Version { get; }

    /// <summary>
    /// Called once immediately after the plugin is loaded.
    /// Use <paramref name="context"/> to register commands, query fences, add rules, etc.
    /// </summary>
    void Initialize(IPluginContext context);

    /// <summary>Called when the application is shutting down or the plugin is unloaded.</summary>
    void Shutdown();
}

/// <summary>
/// Gateway injected into every plugin during <see cref="IFencePlugin.Initialize"/>.
/// Exposes the subset of IVOE Fences services that are safe to use from plugin code.
/// </summary>
public interface IPluginContext
{
    /// <summary>Live, read-only snapshot of all current fences.</summary>
    IReadOnlyList<FenceModel> Fences { get; }

    /// <summary>Access to the sort rules engine for adding or querying rules.</summary>
    SortRulesEngine RuleEngine { get; }

    /// <summary>The behavior learning service for querying user observations.</summary>
    BehaviorLearningService BehaviorLearning { get; }

    /// <summary>Register a new command in the global command palette.</summary>
    void RegisterCommand(CommandPaletteEntry entry);

    /// <summary>Create a new standard fence with the requested title.</summary>
    Guid CreateFence(string title);

    /// <summary>Move/assign a file path into a target fence by title.</summary>
    bool MoveFileToFence(string filePath, string fenceTitle);

    /// <summary>Register plugin-defined settings metadata for shell integration.</summary>
    void RegisterSettings(IEnumerable<PluginSettingDefinition> settings);

    /// <summary>Read a persisted plugin setting.</summary>
    string? GetSetting(string pluginId, string key, string? fallback = null);

    /// <summary>Persist a plugin setting.</summary>
    void SetSetting(string pluginId, string key, string value);

    /// <summary>Subscribe to desktop file-created events.</summary>
    void RegisterFileAddedHandler(Action<string> handler);

    /// <summary>Subscribe to workspace/profile switched events.</summary>
    void RegisterWorkspaceSwitchedHandler(Action<string> handler);

    /// <summary>Log a message at information level (routed through Serilog).</summary>
    void Log(string message);
}
