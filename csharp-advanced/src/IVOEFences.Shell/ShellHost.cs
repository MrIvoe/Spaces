using IVOEFences.Shell.Desktop;
using IVOEFences.Shell.AI;
using IVOEFences.Shell.Fences;
using IVOEFences.Shell.Input;
using IVOEFences.Shell.Native;
using IVOEFences.Shell.Profiles;
using IVOEFences.Shell.Shell;
using IVOEFences.Shell.Settings;
using IVOEFences.Core;
using IVOEFences.Core.Models;
using IVOEFences.Core.Services;
using Serilog;

namespace IVOEFences.Shell;

internal sealed class ShellHost
{
    // ── core subsystems ──
    private readonly TrayHost _tray = new();
    private readonly FenceManager _fences = new();
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
    private FenceVisibilityController _visibility = null!;
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
    private static readonly uint WM_CMD_CREATE_FENCE = Win32.WM_APP + 1;
    private static readonly uint WM_CMD_SETTINGS = Win32.WM_APP + 2;

    public ShellHost()
    {
        _shellWindow = new ShellMessageWindow(OnShellMessage);
        _profileSwitcher = new ProfileSwitcher(_profiles);
        _pluginLoader = new PluginLoader(title => _fences.CreateFenceNow(title));
    }

    public async Task RunAsync()
    {
        Log.Information("ShellHost: starting");

        // ── coordinators ──
        _monitorLayout = new MonitorLayoutCoordinator(_fences);
        _visibility = new FenceVisibilityController(_fences, _fullscreenMonitor, _idleModeService);
        _settingsApplier = new RuntimeSettingsApplier(
            _fences, _hotkeys, _visibility, _idleModeService, RegisterAllHotkeys);
        _workspace = new WorkspaceCoordinator(_fences, _pluginLoader);
        _search = new SearchCoordinator(_fences);
        _commands = new CommandPaletteCoordinator(_workspace, () => _fences.CreateFenceNowIdAsync());
        _snapshots = new SnapshotCoordinator(TimeSpan.FromMinutes(3), () => _shellWindow.Handle);

        // ── startup ──
        FeatureBootstrapper.Initialize();
        MonitorLayoutCoordinator.LogStartupDiagnostics();
        _monitorLayout.InitializeHash();
        _shellWindow.Create();

        // Wire confirm-drop dialog callback
        DragDropPolicyService.Instance.ConfirmDropCallback = (fileName, fenceTitle) =>
        {
            int result = Win32.MessageBox(
                IntPtr.Zero,
                $"Add '{fileName}' to fence '{fenceTitle}'?",
                AppIdentity.ProductName,
                Win32.MB_OKCANCEL | Win32.MB_ICONQUESTION);
            return result == Win32.IDOK;
        };

        _tray.ExitRequested += OnExitRequested;
        _tray.SettingsRequested += OnSettingsRequested;
        _tray.NewFenceRequested += OnNewFenceRequested;
        _tray.ToggleFencesRequested += () => _visibility.ToggleAllFencesVisibility();
        _tray.Initialize();
        _visibility.SetPeekTimerOwner(_shellWindow.Handle);
        SettingsEvents.GlobalSettingsReloaded += _settingsApplier.OnGlobalSettingsReloaded;
        SettingsEvents.SettingChanged += _settingsApplier.OnSettingChanged;

        await _fences.InitializeAsync();
        await RuleDslRuntimeService.Instance.InitializeRulesAsync();
        RegisterBuiltInCommands();
        PageService.Instance.ReloadFromSettings();
        PageService.Instance.PageChanged += OnDesktopPageChanged;
        _visibility.Subscribe();
        _visibility.RefreshFenceVisibility();
        RegisterAllHotkeys();
        _mouseHook.MouseDoubleClick += OnGlobalMouseDoubleClick;
        _mouseHook.Install();
        _profileSwitcher.Start();
        BehaviorLearningService.Instance.RuleSuggested += OnRuleSuggested;
        DesktopWatcherService.Instance.ItemCreated += OnPluginDesktopItemCreated;
        _pluginLoader.DiscoverAndLoad(AppPaths.PluginsDir);
        FenceStateService.Instance.StateChanged += OnFenceStateChanged;

        _ = _ai;

        Log.Information("ShellHost: entering message loop");
        RunMessageLoop();

        // ── shutdown ──
        Log.Information("ShellHost: message loop exited — shutting down");
        SafeShutdownStep("plugin loader dispose", () => _pluginLoader.Dispose());
        SafeShutdownStep("desktop watcher unsubscribe", () => DesktopWatcherService.Instance.ItemCreated -= OnPluginDesktopItemCreated);
        SafeShutdownStep("behavior learning unsubscribe", () => BehaviorLearningService.Instance.RuleSuggested -= OnRuleSuggested);
        SafeShutdownStep("fence state unsubscribe", () => FenceStateService.Instance.StateChanged -= OnFenceStateChanged);
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
        SafeShutdownStep("fence manager dispose", () => _fences.Dispose());
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
                foreach (var win in _fences.Windows)
                    win.RefreshBlurBackground();
            }

            if (msg.message == Win32.WM_DISPLAYCHANGE)
                _monitorLayout.HandleDisplayChange();

            Win32.TranslateMessage(ref msg);
            Win32.DispatchMessage(ref msg);
        }
    }

    private void OnFenceStateChanged(object? sender, EventArgs e)
    {
        _snapshots.OnStatePossiblyChanged();
    }

    private void OnShellMessage(uint msg, IntPtr wParam, IntPtr lParam)
    {
        if (msg == Win32.WM_TIMER)
            _ = _visibility.HandlePeekTimer(_shellWindow.Handle, wParam);
        
        else if (msg == WM_CMD_CREATE_FENCE)
            HandleCreateFenceCommand();
        
        else if (msg == WM_CMD_SETTINGS)
            HandleSettingsCommand();
    }

    private void HandleCreateFenceCommand()
    {
        try
        {
            Log.Information("ShellHost: executing create fence command (dispatched from tray)");
            var result = _fences.CreateFenceNow();
            if (result.WindowCreated && result.WindowHandle != IntPtr.Zero)
            {
                Log.Information("ShellHost: create fence succeeded (id={FenceId}, hwnd={Handle:X})", 
                    result.FenceId, result.WindowHandle);
            }
            else
            {
                Log.Warning("ShellHost: create fence model succeeded but window creation failed (id={FenceId})", 
                    result.FenceId);
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "ShellHost: create fence command failed — keeping process alive");
        }
    }

    private void HandleSettingsCommand()
    {
        try
        {
            Log.Information("ShellHost: executing settings command (dispatched from tray)");
            SettingsWindowHost.ShowOrFocus(_fences, _pluginLoader.PluginSettings);
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
            createFence: () =>
            {
                try
                {
                    var result = _fences.CreateFenceNow();
                    Log.Information("ShellHost: hotkey CreateFence completed (created={Created}, id={FenceId})", 
                        result.WindowCreated, result.FenceId);
                }
                catch (Exception ex)
                {
                    Log.Error(ex, "ShellHost: hotkey CreateFence failed");
                }
            },
            toggleCollapseAll: () => _fences.ToggleCollapseExpandAll(),
            cycleSortMode: CycleSortMode,
            applyIconSize: size => _fences.ApplyIconSize(size),
            toggleAllVisibility: () => _visibility.ToggleAllFencesVisibility(),
            searchAcrossFences: () => _search.PromptSearchAcrossFences(),
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

    // Fullscreen/idle/visibility → FenceVisibilityController

    private void OnRuleSuggested(object? sender, BehaviorLearningService.RuleSuggestion suggestion)
    {
        Log.Information(
            "ShellHost: suggestion available — auto-sort {Ext} to '{Fence}' ({Count} drops)",
            suggestion.Extension,
            suggestion.FenceTitle,
            suggestion.MoveCount);
    }

    // Peek/toggle → FenceVisibilityController

    private void OnGlobalMouseDoubleClick(int x, int y)
    {
        var settings = AppSettingsRepository.Instance.Current;
        if (!settings.EnableQuickHideMode)
            return;

        if (_fences.ContainsScreenPoint(x, y))
            return;

        _visibility.ToggleAllFencesVisibility();
        Log.Debug("ShellHost: mouse hook double-click toggle at ({X},{Y})", x, y);
    }

    private void PromptSearchAcrossFences()
    {
        _search.PromptSearchAcrossFences();
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
        FenceWindow.FenceSortMode mode = (_sortHotkeyCycle % 3) switch
        {
            0 => FenceWindow.FenceSortMode.Name,
            1 => FenceWindow.FenceSortMode.Type,
            _ => FenceWindow.FenceSortMode.DateModified,
        };

        _fences.SortAllFences(mode);
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

    private void OnNewFenceRequested()
    {
        Log.Information("ShellHost: new fence command requested from tray — posting to shell message window");
        if (_shellWindow.Handle != IntPtr.Zero)
        {
            if (!Win32.PostMessage(_shellWindow.Handle, WM_CMD_CREATE_FENCE, IntPtr.Zero, IntPtr.Zero))
            {
                Log.Warning("ShellHost: failed to post create fence command to shell window (err {Err})", 
                    System.Runtime.InteropServices.Marshal.GetLastWin32Error());
            }
        }
        else
        {
            Log.Warning("ShellHost: cannot dispatch create fence command — shell window not available");
        }
    }

    private void OnDesktopPageChanged(object? sender, PageService.PageChangedEventArgs e)
    {
        _visibility.RefreshFenceVisibility();
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
