using IVOEFences.Core.Models;
using IVOEFences.Core.Plugins;
using IVOEFences.Core.Services;
using IVOEFences.Shell.Desktop;
using IVOEFences.Shell.Fences;
using Serilog;
using System.Reflection;

namespace IVOEFences.Shell;

/// <summary>
/// Scans the "plugins/" directory next to the executable for assemblies that contain
/// types implementing <see cref="IFencePlugin"/>, loads them, and manages their lifecycle.
///
/// Plugin DLLs must:
///   - End in ".Plugin.dll" (e.g. "MyTool.Plugin.dll")
///   - Contain at least one non-abstract class implementing <see cref="IFencePlugin"/>
///   - Reference IVOEFences.Core but NOT IVOEFences.Shell (to avoid circular deps)
/// </summary>
internal sealed class PluginLoader : IDisposable
{
    private readonly List<IFencePlugin> _loaded = new();
    private readonly HashSet<string> _loadedPluginIds = new(StringComparer.OrdinalIgnoreCase);
    private readonly PluginContext _context;
    private bool _disposed;

    public PluginLoader(Func<string, CreateFenceResult>? createFence = null)
    {
        _context = new PluginContext(createFence);
    }

    public IReadOnlyList<IFencePlugin> Plugins => _loaded;
    public IReadOnlyList<PluginSettingDefinition> PluginSettings => _context.PluginSettings;

    public void NotifyFileAdded(string fullPath) => _context.NotifyFileAdded(fullPath);

    public void NotifyWorkspaceSwitched(string profileId) => _context.NotifyWorkspaceSwitched(profileId);

    /// <summary>Discover and load all plugins from <paramref name="pluginsDirectory"/>.</summary>
    public void DiscoverAndLoad(string pluginsDirectory)
    {
        if (!Directory.Exists(pluginsDirectory))
        {
            Log.Debug("PluginLoader: plugins directory not found at '{Dir}' — skipping", pluginsDirectory);
            return;
        }

        foreach (string dllPath in Directory.EnumerateFiles(pluginsDirectory, "*.Plugin.dll"))
        {
            LoadAssembly(dllPath);
        }

        Log.Information("PluginLoader: loaded {Count} plugin(s)", _loaded.Count);
    }

    private void LoadAssembly(string dllPath)
    {
        try
        {
            Assembly asm = Assembly.LoadFrom(dllPath);
            IEnumerable<Type> pluginTypes;

            try
            {
                pluginTypes = asm.GetTypes()
                    .Where(t => typeof(IFencePlugin).IsAssignableFrom(t)
                             && !t.IsAbstract
                             && t.GetConstructor(Type.EmptyTypes) != null);
            }
            catch (ReflectionTypeLoadException ex)
            {
                foreach (Exception? loaderEx in ex.LoaderExceptions)
                {
                    if (loaderEx != null)
                        Log.Warning(loaderEx, "PluginLoader: type load failure in '{Path}'", dllPath);
                }

                pluginTypes = ex.Types
                    .Where(t => t != null)
                    .Cast<Type>()
                    .Where(t => typeof(IFencePlugin).IsAssignableFrom(t)
                             && !t.IsAbstract
                             && t.GetConstructor(Type.EmptyTypes) != null);
            }

            foreach (Type type in pluginTypes)
                LoadPlugin(type, dllPath);
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "PluginLoader: failed to load assembly '{Path}'", dllPath);
        }
    }

    private void LoadPlugin(Type type, string dllPath)
    {
        try
        {
            var plugin = (IFencePlugin)Activator.CreateInstance(type)!;

            if (string.IsNullOrWhiteSpace(plugin.Id) || string.IsNullOrWhiteSpace(plugin.Name))
            {
                Log.Warning("PluginLoader: rejected plugin type '{Type}' from '{Path}' due to missing Id/Name", type.FullName, dllPath);
                return;
            }

            if (!_loadedPluginIds.Add(plugin.Id))
            {
                Log.Warning("PluginLoader: rejected duplicate plugin id '{Id}' from '{Path}'", plugin.Id, dllPath);
                return;
            }

            plugin.Initialize(_context);
            _loaded.Add(plugin);
            Log.Information("PluginLoader: loaded plugin '{Name}' v{Version} (id={Id}) from '{Path}'",
                plugin.Name, plugin.Version, plugin.Id, dllPath);
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "PluginLoader: failed to initialize plugin type '{Type}'", type.FullName);
        }
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;

        foreach (var plugin in _loaded)
        {
            try { plugin.Shutdown(); }
            catch (Exception ex)
            {
                Log.Warning(ex, "PluginLoader: plugin '{Name}' threw during Shutdown", plugin.Name);
            }
        }

        _loaded.Clear();
        _loadedPluginIds.Clear();
    }

    // ── PluginContext ─────────────────────────────────────────────────────────

    private sealed class PluginContext : IPluginContext
    {
        private readonly List<Action<string>> _fileAddedHandlers = new();
        private readonly List<Action<string>> _workspaceHandlers = new();
        private readonly List<PluginSettingDefinition> _pluginSettings = new();
        private readonly Func<string, CreateFenceResult>? _createFence;

        public PluginContext(Func<string, CreateFenceResult>? createFence)
        {
            _createFence = createFence;
        }

        public IReadOnlyList<PluginSettingDefinition> PluginSettings => _pluginSettings;

        public IReadOnlyList<FenceModel> Fences =>
            FenceStateService.Instance.Fences;

        public SortRulesEngine RuleEngine =>
            SortRulesEngine.Instance;

        public BehaviorLearningService BehaviorLearning =>
            BehaviorLearningService.Instance;

        public void RegisterCommand(CommandPaletteEntry entry) =>
            CommandPaletteService.Instance.Register(entry);

        public Guid CreateFence(string title)
        {
            if (_createFence == null)
            {
                string message = "PluginContext: CreateFence callback unavailable; refusing persist-only fallback";
                Serilog.Log.Error("{Message} title={Title}", message, title);
                throw new InvalidOperationException(message);
            }

            CreateFenceResult result = _createFence(title);
            if (result.FenceId == Guid.Empty || !result.WindowCreated || result.WindowHandle == IntPtr.Zero)
            {
                string message =
                    $"PluginContext: CreateFence failed to materialize runtime fence title='{title}' id='{result.FenceId}' windowCreated={result.WindowCreated} hwnd=0x{result.WindowHandle.ToInt64():X}";
                Serilog.Log.Error("{Message}", message);
                throw new InvalidOperationException(message);
            }

            Serilog.Log.Information(
                "PluginContext: CreateFence materialized title='{Title}' id='{FenceId}' hwnd=0x{Hwnd:X}",
                title,
                result.FenceId,
                result.WindowHandle.ToInt64());

            return result.FenceId;
        }

        public bool MoveFileToFence(string filePath, string fenceTitle)
        {
            if (string.IsNullOrWhiteSpace(filePath) || string.IsNullOrWhiteSpace(fenceTitle))
                return false;

            FenceModel? fence = FenceStateService.Instance.Fences.FirstOrDefault(f =>
                f.Type == FenceType.Standard &&
                string.Equals(f.Title, fenceTitle, StringComparison.OrdinalIgnoreCase));

            if (fence == null)
                return false;

            bool isDir = Directory.Exists(filePath);
            bool isFile = File.Exists(filePath);
            if (!isDir && !isFile)
                return false;

            return DragDropPolicyService.Instance.ImportIntoFence(fence, filePath) != null;
        }

        public void RegisterSettings(IEnumerable<PluginSettingDefinition> settings)
        {
            foreach (PluginSettingDefinition setting in settings)
            {
                if (_pluginSettings.Any(existing => existing.PluginId == setting.PluginId && existing.Key == setting.Key))
                    continue;

                _pluginSettings.Add(setting);

                if (setting.DefaultValue != null && PluginSettingsStore.Instance.Get(setting.PluginId, setting.Key) == null)
                    PluginSettingsStore.Instance.Set(setting.PluginId, setting.Key, setting.DefaultValue);
            }
        }

        public string? GetSetting(string pluginId, string key, string? fallback = null)
        {
            return PluginSettingsStore.Instance.Get(pluginId, key, fallback);
        }

        public void SetSetting(string pluginId, string key, string value)
        {
            PluginSettingsStore.Instance.Set(pluginId, key, value);
        }

        public void RegisterFileAddedHandler(Action<string> handler)
        {
            if (handler != null)
                _fileAddedHandlers.Add(handler);
        }

        public void RegisterWorkspaceSwitchedHandler(Action<string> handler)
        {
            if (handler != null)
                _workspaceHandlers.Add(handler);
        }

        public void Log(string message) =>
            Serilog.Log.Information("[Plugin] {Message}", message);

        public void NotifyFileAdded(string fullPath)
        {
            foreach (var handler in _fileAddedHandlers)
            {
                try { handler(fullPath); }
                catch (Exception ex) { Serilog.Log.Warning(ex, "Plugin file-added handler failed"); }
            }
        }

        public void NotifyWorkspaceSwitched(string profileId)
        {
            foreach (var handler in _workspaceHandlers)
            {
                try { handler(profileId); }
                catch (Exception ex) { Serilog.Log.Warning(ex, "Plugin workspace handler failed"); }
            }
        }
    }
}
