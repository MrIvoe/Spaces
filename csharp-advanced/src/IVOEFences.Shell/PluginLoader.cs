using IVOEFences.Core.Models;
using IVOEFences.Core.Plugins;
using IVOEFences.Core.Services;
using IVOEFences.Core;
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
    private readonly PluginManagerService _pluginManager;
    private readonly PluginUpdateService _pluginUpdateService;
    private readonly PluginManifestReader _pluginManifestReader;
    private readonly PluginPackageService _pluginPackageService;
    private readonly PluginCompatibilityService _pluginCompatibilityService;
    private readonly PluginTrustPolicyService _pluginTrustPolicyService;
    private bool _disposed;

    public PluginLoader(Func<string, CreateFenceResult>? createFence = null)
    {
        _pluginManager = PluginManagerService.Instance;
        _pluginUpdateService = PluginUpdateService.Instance;
        _pluginManifestReader = new PluginManifestReader();
        _pluginPackageService = PluginPackageService.Instance;
        _pluginCompatibilityService = PluginCompatibilityService.Instance;
        _pluginTrustPolicyService = PluginTrustPolicyService.Instance;
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
            Directory.CreateDirectory(pluginsDirectory);
        }

        _pluginUpdateService.BeginCheck("startup scan");
        IReadOnlyList<PluginManifestReader.PluginManifest> manifests =
            _pluginPackageService.GetCandidateManifests(pluginsDirectory, _pluginManifestReader);

        _pluginManager.RefreshFromManifestList(manifests);
        _pluginUpdateService.CheckFromManifests(manifests);

        foreach (PluginManifestReader.PluginManifest manifest in manifests)
        {
            LoadFromManifest(manifest);
        }

        if (manifests.Count == 0)
        {
            foreach (string dllPath in Directory.EnumerateFiles(pluginsDirectory, "*.Plugin.dll"))
            {
                LoadAssembly(dllPath);
            }
        }

        _pluginUpdateService.RefreshFromMetadata(_pluginManager.GetPlugins());

        Log.Information("PluginLoader: loaded {Count} plugin(s)", _loaded.Count);
    }

    private void LoadFromManifest(PluginManifestReader.PluginManifest manifest)
    {
        if (!_pluginManager.IsPluginEnabled(manifest.Id))
        {
            _pluginManager.SetPluginStatus(manifest.Id, "Disabled");
            return;
        }

        PluginCompatibilityResult compatibility = _pluginCompatibilityService.Evaluate(manifest);
        if (!compatibility.IsCompatible)
        {
            _pluginManager.SetPluginStatus(manifest.Id, compatibility.Reason);
            return;
        }

        PluginTrustDecision trustDecision = _pluginTrustPolicyService.EvaluateManifest(manifest);
        if (!trustDecision.IsTrusted)
        {
            _pluginManager.SetPluginStatus(manifest.Id, $"Blocked by trust policy: {trustDecision.Reason}");
            _pluginUpdateService.SetPluginState(manifest.Id, PluginUpdateState.Create(
                PluginUpdateStateKind.Failed,
                trustDecision.Reason,
                manifest.Version));
            return;
        }

        string? assemblyPath = ResolveAssemblyPath(manifest);
        if (string.IsNullOrWhiteSpace(assemblyPath) || !File.Exists(assemblyPath))
        {
            _pluginManager.SetPluginStatus(manifest.Id, "Package manifest present but no plugin assembly was found.");
            return;
        }

        LoadAssembly(assemblyPath);
    }

    private static string? ResolveAssemblyPath(PluginManifestReader.PluginManifest manifest)
    {
        if (!string.IsNullOrWhiteSpace(manifest.Assembly))
        {
            string resolved = Path.Combine(manifest.DirectoryPath, manifest.Assembly);
            if (File.Exists(resolved))
                return resolved;
        }

        return Directory
            .EnumerateFiles(manifest.DirectoryPath, "*.Plugin.dll", SearchOption.AllDirectories)
            .FirstOrDefault();
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
            _pluginUpdateService.SetGlobalState(PluginUpdateState.Create(
                PluginUpdateStateKind.Failed,
                $"Plugin assembly load failed: {Path.GetFileName(dllPath)}"));
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
            _pluginManager.RegisterLoadedPlugin(plugin, dllPath);
            _pluginUpdateService.SetPluginState(plugin.Id, PluginUpdateState.Create(
                PluginUpdateStateKind.UpToDate,
                "Loaded successfully.",
                plugin.Version));
            Log.Information("PluginLoader: loaded plugin '{Name}' v{Version} (id={Id}) from '{Path}'",
                plugin.Name, plugin.Version, plugin.Id, dllPath);
        }
        catch (Exception ex)
        {
            _pluginUpdateService.SetGlobalState(PluginUpdateState.Create(
                PluginUpdateStateKind.Failed,
                $"Plugin initialization failed: {type.Name}"));
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
        private readonly PluginManagerService _pluginManager;
        private readonly PluginUpdateService _pluginUpdateService;
        private readonly ThemeService _themeService;

        public PluginContext(Func<string, CreateFenceResult>? createFence)
        {
            _createFence = createFence;
            _pluginManager = PluginManagerService.Instance;
            _pluginUpdateService = PluginUpdateService.Instance;
            _themeService = ThemeService.Instance;
        }

        public IReadOnlyList<PluginSettingDefinition> PluginSettings => _pluginSettings;

        public PluginThemeSnapshot CurrentTheme => _themeService.GetCurrentTheme();

        public IReadOnlyDictionary<string, string> SharedResources
        {
            get
            {
                Dictionary<string, string> resources = new(StringComparer.OrdinalIgnoreCase);
                foreach ((string key, string value) in _themeService.GetSharedResources())
                    resources[key] = value;

                resources["app.dataRoot"] = AppPaths.DataRoot;
                resources["app.pluginsDir"] = AppPaths.PluginsDir;
                resources["app.pluginInstalledDir"] = AppPaths.PluginInstalledDir;
                resources["app.logsDir"] = AppPaths.LogsDir;
                resources["app.settingsFile"] = AppPaths.SettingsConfig;
                resources["app.pluginSettingsFile"] = AppPaths.PluginSettingsConfig;
                resources["theme.systemDir"] = AppPaths.Win32ThemeSystemDir;
                return resources;
            }
        }

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

        public void ReportStatus(string pluginId, string status)
        {
            _pluginManager.SetPluginStatus(pluginId, status);
            if (!string.IsNullOrWhiteSpace(pluginId))
            {
                _pluginUpdateService.SetPluginState(pluginId, PluginUpdateState.Create(
                    PluginUpdateStateKind.Idle,
                    status));
            }

            Serilog.Log.Information("[Plugin:{PluginId}] {Status}", pluginId, status);
        }

        public IReadOnlyList<PluginMetadata> GetPluginMetadata()
        {
            return _pluginManager.GetPlugins();
        }

        public PluginUpdateState GetUpdateState(string? pluginId = null)
        {
            return string.IsNullOrWhiteSpace(pluginId)
                ? _pluginUpdateService.GetGlobalState()
                : _pluginUpdateService.GetPluginState(pluginId);
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
