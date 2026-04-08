using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;

namespace IVOESpaces.Core.Plugins;

/// <summary>
/// Interface implemented by all IVOE Spaces plug-ins.
/// Place compiled plugin DLLs in the "plugins/" folder next to IVOESpaces.dll.
/// The assembly must contain at least one non-abstract type implementing ISpacePlugin.
/// </summary>
public interface ISpacePlugin
{
    /// <summary>Stable reverse-domain identifier, e.g. "com.example.myplugin".</summary>
    string Id      { get; }

    string Name    { get; }
    string Version { get; }

    /// <summary>
    /// Called once immediately after the plugin is loaded.
    /// Use <paramref name="context"/> to register commands, query spaces, add rules, etc.
    /// </summary>
    void Initialize(IPluginContext context);

    /// <summary>Called when the application is shutting down or the plugin is unloaded.</summary>
    void Shutdown();
}

/// <summary>
/// Gateway injected into every plugin during <see cref="ISpacePlugin.Initialize"/>.
/// Exposes the subset of IVOE Spaces services that are safe to use from plugin code.
/// </summary>
public interface IPluginContext
{
    /// <summary>Live, read-only snapshot of all current spaces.</summary>
    IReadOnlyList<SpaceModel> Spaces { get; }

    /// <summary>Access to the sort rules engine for adding or querying rules.</summary>
    SortRulesEngine RuleEngine { get; }

    /// <summary>The behavior learning service for querying user observations.</summary>
    BehaviorLearningService BehaviorLearning { get; }

    /// <summary>Register a new command in the global command palette.</summary>
    void RegisterCommand(CommandPaletteEntry entry);

    /// <summary>Create a new standard space with the requested title.</summary>
    Guid CreateSpace(string title);

    /// <summary>Move/assign a file path into a target space by title.</summary>
    bool MoveFileToSpace(string filePath, string spaceTitle);

    /// <summary>Register plugin-defined settings metadata for shell integration.</summary>
    void RegisterSettings(IEnumerable<PluginSettingDefinition> settings);

    /// <summary>Read a persisted plugin setting.</summary>
    string? GetSetting(string pluginId, string key, string? fallback = null);

    /// <summary>Persist a plugin setting.</summary>
    void SetSetting(string pluginId, string key, string value);

    /// <summary>Current theme snapshot for plugin UI/state decisions.</summary>
    PluginThemeSnapshot CurrentTheme { get; }

    /// <summary>Shared host resources and known app paths.</summary>
    IReadOnlyDictionary<string, string> SharedResources { get; }

    /// <summary>Publish plugin status text for host UI and diagnostics.</summary>
    void ReportStatus(string pluginId, string status);

    /// <summary>Discover plugin metadata known to the host.</summary>
    IReadOnlyList<PluginMetadata> GetPluginMetadata();

    /// <summary>Get the current plugin update state. Null plugin id returns global state.</summary>
    PluginUpdateState GetUpdateState(string? pluginId = null);

    /// <summary>Subscribe to desktop file-created events.</summary>
    void RegisterFileAddedHandler(Action<string> handler);

    /// <summary>Subscribe to workspace/profile switched events.</summary>
    void RegisterWorkspaceSwitchedHandler(Action<string> handler);

    /// <summary>Log a message at information level (routed through Serilog).</summary>
    void Log(string message);
}
