using System.Text.Json;
using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

public sealed class WorkspaceOrchestrator
{
    private static readonly Lazy<WorkspaceOrchestrator> _instance = new(() => new WorkspaceOrchestrator());
    private readonly Dictionary<string, WorkspaceProfileBinding> _bindings = new(StringComparer.OrdinalIgnoreCase);
    private readonly object _lock = new();

    public static WorkspaceOrchestrator Instance => _instance.Value;

    private WorkspaceOrchestrator()
    {
        Load();
    }

    public void RegisterBinding(WorkspaceProfileBinding binding)
    {
        lock (_lock)
        {
            _bindings[binding.ProfileId] = binding;
            Save_NoLock();
        }
    }

    public async Task<bool> ActivateProfileWorkspaceAsync(FenceProfileModel profile)
    {
        WorkspaceProfileBinding? binding;
        lock (_lock)
        {
            _bindings.TryGetValue(profile.Id, out binding);
        }

        if (binding != null
            && !string.IsNullOrWhiteSpace(binding.SnapshotId)
            && Guid.TryParse(binding.SnapshotId, out Guid snapshotId))
        {
            await SnapshotRepository.Instance.RestoreSnapshot(snapshotId);
        }

        if (binding != null)
            ApplyGlobalOverrides(binding.GlobalSettingsOverrides);

        WorkspaceLinkService.Instance.SwitchWorkspace(profile, FenceStateService.Instance.Fences);
        return true;
    }

    private static void ApplyGlobalOverrides(Dictionary<string, string> overrides)
    {
        if (overrides.Count == 0)
            return;

        SettingsManager.Instance.Update(settings =>
        {
            foreach ((string key, string value) in overrides)
            {
                switch (key)
                {
                    case "ThemeMode":
                        settings.ThemeMode = value;
                        break;
                    case "FenceOpacity":
                        if (int.TryParse(value, out int opacity))
                            settings.FenceOpacity = Math.Clamp(opacity, 20, 100);
                        break;
                    case "EnableGlobalPlacementRules":
                        if (bool.TryParse(value, out bool enableRules))
                            settings.EnableGlobalPlacementRules = enableRules;
                        break;
                    case "EnableQuickHideMode":
                        if (bool.TryParse(value, out bool enableQuickHide))
                            settings.EnableQuickHideMode = enableQuickHide;
                        break;
                    case "StandardFenceDropMode":
                        settings.StandardFenceDropMode = value;
                        break;
                }
            }
        }, overrides.Keys.Select(key => $"workspace.{key}").ToArray());
    }

    private void Load()
    {
        lock (_lock)
        {
            try
            {
                if (!File.Exists(AppPaths.WorkspaceBindingsConfig))
                    return;

                string json = File.ReadAllText(AppPaths.WorkspaceBindingsConfig);
                List<WorkspaceProfileBinding>? bindings = JsonSerializer.Deserialize<List<WorkspaceProfileBinding>>(json);
                _bindings.Clear();
                if (bindings == null)
                    return;

                foreach (WorkspaceProfileBinding binding in bindings)
                    _bindings[binding.ProfileId] = binding;
            }
            catch
            {
                _bindings.Clear();
            }
        }
    }

    private void Save_NoLock()
    {
        Directory.CreateDirectory(Path.GetDirectoryName(AppPaths.WorkspaceBindingsConfig)!);
        string json = JsonSerializer.Serialize(_bindings.Values.ToList(), new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(AppPaths.WorkspaceBindingsConfig, json);
    }
}