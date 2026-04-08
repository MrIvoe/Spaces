using IVOESpaces.Shell.Desktop;
using IVOESpaces.Shell.AI;
using IVOESpaces.Shell.Spaces;
using IVOESpaces.Shell.Input;
using IVOESpaces.Shell.Native;
using IVOESpaces.Shell.Profiles;
using IVOESpaces.Shell.Shell;
using IVOESpaces.Shell.Settings;
using IVOESpaces.Core;
using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;
using Serilog;

namespace IVOESpaces.Shell;

internal sealed class ShellHost
{
    // ── core subsystems ──
    private readonly TrayHost _tray = new();
    private readonly SpaceManager _spaces = new();
    private readonly ProfileManager _profiles = new();
    private readonly ProfileSwitcher _profileSwitcher;
    private readonly AIManager _ai = new();
    private readonly MouseHook _mouseHook = new();
    private readonly FullscreenMonitor _fullscreenMonitor = new();
    private readonly IdleModeService _idleModeService = new();
    private readonly PluginLoader _pluginLoader;
    private readonly ShellMessageWindow _shellWindow;

    // ── coordinators (extracted responsibilities) ──
    private readonly HotkeyCoordinator _hotkeys = new();
    private MonitorLayoutCoordinator _monitorLayout = null!;
    private SpaceVisibilityController _visibility = null!;
    private RuntimeSettingsApplier _settingsApplier = null!;
    private WorkspaceCoordinator _workspace = null!;
    private SearchCoordinator _search = null!;
    private CommandPaletteCoordinator _commands = null!;
    private SnapshotCoordinator _snapshots = null!;

    // ── remaining ShellHost state ──
    private int _sortHotkeyCycle;

    private static readonly uint WM_TASKBARCREATED =
        Win32.RegisterWindowMessage("TaskbarCreated");

    // ── Custom window messages for safe command dispatch ──
    private static readonly uint WM_CMD_CREATE_SPACE = Win32.WM_APP + 1;
    private static readonly uint WM_CMD_SETTINGS = Win32.WM_APP + 2;

    public ShellHost()
    {
        _shellWindow = new ShellMessageWindow(OnShellMessage);
        _profileSwitcher = new ProfileSwitcher(_profiles);
        _pluginLoader = new PluginLoader(title => _spaces.CreateSpaceNow(title));
    }

    public async Task RunAsync()
    {
        Log.Information("ShellHost: starting");

        // ── coordinators ──
        _monitorLayout = new MonitorLayoutCoordinator(_spaces);
        _visibility = new SpaceVisibilityController(_spaces, _fullscreenMonitor, _idleModeService);
        _settingsApplier = new RuntimeSettingsApplier(
            _spaces, _hotkeys, _visibility, _idleModeService, RegisterAllHotkeys);
        _workspace = new WorkspaceCoordinator(_spaces, _pluginLoader);
        _search = new SearchCoordinator(_spaces);
        _commands = new CommandPaletteCoordinator(_workspace, () => _spaces.CreateSpaceNowIdAsync());
        _snapshots = new SnapshotCoordinator(TimeSpan.FromMinutes(3), () => _shellWindow.Handle);

        // ── startup ──
        FeatureBootstrapper.Initialize();
        MonitorLayoutCoordinator.LogStartupDiagnostics();
        _monitorLayout.InitializeHash();
        _shellWindow.Create();

        // Wire confirm-drop dialog callback
        DragDropPolicyService.Instance.ConfirmDropCallback = (fileName, spaceTitle) =>
        {
            int result = Win32.MessageBox(
                IntPtr.Zero,
                $"Add '{fileName}' to space '{spaceTitle}'?",
                AppIdentity.ProductName,
                Win32.MB_OKCANCEL | Win32.MB_ICONQUESTION);
            return result == Win32.IDOK;
        };

        _tray.ExitRequested += OnExitRequested;
        _tray.SettingsRequested += OnSettingsRequested;
        _tray.NewSpaceRequested += OnNewSpaceRequested;
        _tray.ToggleSpacesRequested += () => _visibility.ToggleAllSpacesVisibility();
        _tray.Initialize();
        _visibility.SetPeekTimerOwner(_shellWindow.Handle);
        SettingsEvents.GlobalSettingsReloaded += _settingsApplier.OnGlobalSettingsReloaded;
        SettingsEvents.SettingChanged += _settingsApplier.OnSettingChanged;

        await _spaces.InitializeAsync();
        await RuleDslRuntimeService.Instance.InitializeRulesAsync();
        RegisterBuiltInCommands();
        PageService.Instance.ReloadFromSettings();
        PageService.Instance.PageChanged += OnDesktopPageChanged;
        _visibility.Subscribe();
        _visibility.RefreshSpaceVisibility();
        RegisterAllHotkeys();
        _mouseHook.MouseDoubleClick += OnGlobalMouseDoubleClick;
        _mouseHook.Install();
        _profileSwitcher.Start();
        BehaviorLearningService.Instance.RuleSuggested += OnRuleSuggested;
        DesktopWatcherService.Instance.ItemCreated += OnPluginDesktopItemCreated;
        _pluginLoader.DiscoverAndLoad(AppPaths.PluginsDir);
        SpaceStateService.Instance.StateChanged += OnSpaceStateChanged;

        _ = _ai;

        Log.Information("ShellHost: entering message loop");
        RunMessageLoop();

        // ── shutdown ──
        Log.Information("ShellHost: message loop exited — shutting down");
        SafeShutdownStep("plugin loader dispose", () => _pluginLoader.Dispose());
        SafeShutdownStep("desktop watcher unsubscribe", () => DesktopWatcherService.Instance.ItemCreated -= OnPluginDesktopItemCreated);
        SafeShutdownStep("behavior learning unsubscribe", () => BehaviorLearningService.Instance.RuleSuggested -= OnRuleSuggested);
        SafeShutdownStep("space state unsubscribe", () => SpaceStateService.Instance.StateChanged -= OnSpaceStateChanged);
        SafeShutdownStep("snapshot coordinator dispose", () => _snapshots.Dispose());
        SafeShutdownStep("shell message window dispose", () => _shellWindow.Dispose());
        SafeShutdownStep("visibility controller dispose", () => _visibility.Dispose());
        SafeShutdownStep("idle mode dispose", () => _idleModeService.Dispose());
        SafeShutdownStep("fullscreen monitor dispose", () => _fullscreenMonitor.Dispose());
        SafeShutdownStep("profile switcher dispose", () => _profileSwitcher.Dispose());
        SafeShutdownStep("mouse hook unsubscribe", () => _mouseHook.MouseDoubleClick -= OnGlobalMouseDoubleClick);
        SafeShutdownStep("mouse hook dispose", () => _mouseHook.Dispose());
        SafeShutdownStep("page service unsubscribe", () => PageService.Instance.PageChanged -= OnDesktopPageChanged);
        SafeShutdownStep("hotkeys dispose", () => _hotkeys.Dispose());
        SafeShutdownStep("tray dispose", () => _tray.Dispose());
        SafeShutdownStep("settings reload unsubscribe", () => SettingsEvents.GlobalSettingsReloaded -= _settingsApplier.OnGlobalSettingsReloaded);
        SafeShutdownStep("setting changed unsubscribe", () => SettingsEvents.SettingChanged -= _settingsApplier.OnSettingChanged);
        SafeShutdownStep("space manager dispose", () => _spaces.Dispose());
    }

    private static void SafeShutdownStep(string stepName, Action action)
    {
        try
        {
            action();
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "ShellHost: shutdown step failed: {Step}", stepName);
        }
    }

    private void RunMessageLoop()
    {
        Win32.MSG msg;
        while (Win32.GetMessage(out msg, IntPtr.Zero, 0, 0))
        {
            if (msg.message == WM_TASKBARCREATED)
            {
                Log.Information("ShellHost: WM_TASKBARCREATED — Explorer restarted");
                DesktopHost.Instance.OnExplorerRestarted();
                _tray.RecreateIfNeeded();
                continue;
            }

            if (msg.message == Win32.WM_HOTKEY)
            {
                _hotkeys.HandleHotkey(msg.wParam.ToInt32());
                continue;
            }

            if (msg.message == Win32.WM_TIMER && _visibility.HandlePeekTimer(msg.hwnd, msg.wParam))
                continue;

            if (msg.message == Win32.WM_TIMER && _snapshots.HandleTimer(msg.hwnd, msg.wParam))
                continue;

            if (msg.message == Win32.WM_SETTINGCHANGE || msg.message == Win32.WM_THEMECHANGED)
            {
                ThemeEngine.Instance.RefreshAll();
                foreach (var win in _spaces.Windows)
                    win.RefreshBlurBackground();
            }

            if (msg.message == Win32.WM_DISPLAYCHANGE)
                _monitorLayout.HandleDisplayChange();

            Win32.TranslateMessage(ref msg);
            Win32.DispatchMessage(ref msg);
        }
    }

    private void OnSpaceStateChanged(object? sender, EventArgs e)
    {
        _snapshots.OnStatePossiblyChanged();
    }

    private void OnShellMessage(uint msg, IntPtr wParam, IntPtr lParam)
    {
        if (msg == Win32.WM_TIMER)
            _ = _visibility.HandlePeekTimer(_shellWindow.Handle, wParam);
        
        else if (msg == WM_CMD_CREATE_SPACE)
            HandleCreateSpaceCommand();
        
        else if (msg == WM_CMD_SETTINGS)
            HandleSettingsCommand();
    }

    private void HandleCreateSpaceCommand()
    {
        try
        {
            Log.Information("ShellHost: executing create space command (dispatched from tray)");
            var result = _spaces.CreateSpaceNow();
            if (result.WindowCreated && result.WindowHandle != IntPtr.Zero)
            {
                Log.Information("ShellHost: create space succeeded (id={SpaceId}, hwnd={Handle:X})", 
                    result.SpaceId, result.WindowHandle);
            }
            else
            {
                Log.Warning("ShellHost: create space model succeeded but window creation failed (id={SpaceId})", 
                    result.SpaceId);
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "ShellHost: create space command failed — keeping process alive");
        }
    }

    private void HandleSettingsCommand()
    {
        try
        {
            Log.Information("ShellHost: executing settings command (dispatched from tray)");
            SettingsWindowHost.ShowOrFocus(_spaces, _pluginLoader.PluginSettings);
            Log.Information("ShellHost: settings command completed successfully");
        }
        catch (Exception ex)
        {
            Log.Error(ex, "ShellHost: settings command failed — keeping process alive");
        }
    }

    // LogStartupDiagnostics → MonitorLayoutCoordinator.LogStartupDiagnostics()
    // TryGetMonitorTopology → MonitorLayoutCoordinator.TryGetMonitorTopology()
    // HandleDisplayChange → MonitorLayoutCoordinator.HandleDisplayChange()

    // Hotkey registration → HotkeyCoordinator.RegisterStandardHotkeys()
    // Hotkey parsing → HotkeyCoordinator.TryParseHotkey()
    // Hotkey dispatch → HotkeyCoordinator.HandleHotkey()

    private void RegisterAllHotkeys()
    {
        _hotkeys.RegisterStandardHotkeys(
            createSpace: () =>
            {
                try
                {
                    var result = _spaces.CreateSpaceNow();
                    Log.Information("ShellHost: hotkey CreateSpace completed (created={Created}, id={SpaceId})", 
                        result.WindowCreated, result.SpaceId);
                }
                catch (Exception ex)
                {
                    Log.Error(ex, "ShellHost: hotkey CreateSpace failed");
                }
            },
            toggleCollapseAll: () => _spaces.ToggleCollapseExpandAll(),
            cycleSortMode: CycleSortMode,
            applyIconSize: size => _spaces.ApplyIconSize(size),
            toggleAllVisibility: () => _visibility.ToggleAllSpacesVisibility(),
            searchAcrossSpaces: () => _search.PromptSearchAcrossSpaces(),
            switchDesktopPage: SwitchDesktopPage,
            switchWorkspace: index => _workspace.SwitchWorkspaceByIndex(index),
            commandPalette: () => _commands.PromptCommandPalette());
    }

    private void SwitchWorkspaceByIndex(int index)
    {
        _workspace.SwitchWorkspaceByIndex(index);
    }

    private void OnPluginDesktopItemCreated(object? sender, DesktopItemEventArgs e)
    {
        _pluginLoader.NotifyFileAdded(e.FullPath);
    }

    // Fullscreen/idle/visibility → SpaceVisibilityController

    private void OnRuleSuggested(object? sender, BehaviorLearningService.RuleSuggestion suggestion)
    {
        Log.Information(
            "ShellHost: suggestion available — auto-sort {Ext} to '{Space}' ({Count} drops)",
            suggestion.Extension,
            suggestion.SpaceTitle,
            suggestion.MoveCount);
    }

    // Peek/toggle → SpaceVisibilityController

    private void OnGlobalMouseDoubleClick(int x, int y)
    {
        var settings = AppSettingsRepository.Instance.Current;
        if (!settings.EnableQuickHideMode)
            return;

        if (_spaces.ContainsScreenPoint(x, y))
            return;

        _visibility.ToggleAllSpacesVisibility();
        Log.Debug("ShellHost: mouse hook double-click toggle at ({X},{Y})", x, y);
    }

    private void PromptSearchAcrossSpaces()
    {
        _search.PromptSearchAcrossSpaces();
    }

    private void RegisterBuiltInCommands()
    {
        _commands.RegisterBuiltInCommands();
    }

    private void PromptCommandPalette()
    {
        _commands.PromptCommandPalette();
    }

    private void CleanUpDesktopLayout()
    {
        _workspace.CleanUpDesktopLayout();
    }

    private void CycleSortMode()
    {
        SpaceWindow.SpaceSortMode mode = (_sortHotkeyCycle % 3) switch
        {
            0 => SpaceWindow.SpaceSortMode.Name,
            1 => SpaceWindow.SpaceSortMode.Type,
            _ => SpaceWindow.SpaceSortMode.DateModified,
        };

        _spaces.SortAllSpaces(mode);
        _sortHotkeyCycle++;
        Log.Information("ShellHost: hotkey sort mode -> {Mode}", mode);
    }

    private void OnExitRequested()
    {
        Win32.PostQuitMessage(0);
    }

    private void OnSettingsRequested()
    {
        Log.Information("ShellHost: settings command requested from tray — posting to shell message window");
        if (_shellWindow.Handle != IntPtr.Zero)
        {
            if (!Win32.PostMessage(_shellWindow.Handle, WM_CMD_SETTINGS, IntPtr.Zero, IntPtr.Zero))
            {
                Log.Warning("ShellHost: failed to post settings command to shell window (err {Err})", 
                    System.Runtime.InteropServices.Marshal.GetLastWin32Error());
            }
        }
        else
        {
            Log.Warning("ShellHost: cannot dispatch settings command — shell window not available");
        }
    }

    private void OnNewSpaceRequested()
    {
        Log.Information("ShellHost: new space command requested from tray — posting to shell message window");
        if (_shellWindow.Handle != IntPtr.Zero)
        {
            if (!Win32.PostMessage(_shellWindow.Handle, WM_CMD_CREATE_SPACE, IntPtr.Zero, IntPtr.Zero))
            {
                Log.Warning("ShellHost: failed to post create space command to shell window (err {Err})", 
                    System.Runtime.InteropServices.Marshal.GetLastWin32Error());
            }
        }
        else
        {
            Log.Warning("ShellHost: cannot dispatch create space command — shell window not available");
        }
    }

    private void OnDesktopPageChanged(object? sender, PageService.PageChangedEventArgs e)
    {
        _visibility.RefreshSpaceVisibility();
        Log.Information("ShellHost: desktop page changed {Previous} -> {Current}", e.PreviousPageIndex + 1, e.CurrentPageIndex + 1);
    }

    private void SwitchDesktopPage(int delta)
    {
        var settings = AppSettingsRepository.Instance.Current;
        if (!settings.EnableDesktopPages)
        {
            Log.Information("ShellHost: desktop page hotkey ignored because desktop pages are disabled");
            return;
        }

        if (delta < 0)
            PageService.Instance.PreviousPage();
        else
            PageService.Instance.NextPage();
    }
}
