using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Observes manual drag-drop moves to learn each user's file-type → fence preferences.
/// After <see cref="SuggestionThreshold"/> moves of the same file extension to the same
/// fence, fires <see cref="RuleSuggested"/> so the shell can prompt the user to
/// permanently automate that sort rule.
///
/// Persists observations to behavior.json with a single-level backup.
/// Thread-safe; all writes go through the internal lock.
/// </summary>
public sealed class BehaviorLearningService
{
    private static readonly Lazy<BehaviorLearningService> _instance =
        new(() => new BehaviorLearningService());

    public static BehaviorLearningService Instance => _instance.Value;

    /// <summary>Number of manual moves of (ext → fence) that triggers a rule suggestion.</summary>
    public const int SuggestionThreshold = 3;

    private readonly object _lock = new();

    // ext (normalised, e.g. ".png") → fenceId string → observation
    private Dictionary<string, Dictionary<string, BehaviorObservation>> _observations = new();

    // "ext|fenceIdN" keys that must never be re-suggested (dismissed or accepted)
    private readonly HashSet<string> _dismissed = new();

    private Timer? _saveTimer;

    public event EventHandler<RuleSuggestion>? RuleSuggested;

    // ── Nested types ─────────────────────────────────────────────────────────

    public sealed class BehaviorObservation
    {
        public string FenceId    { get; set; } = string.Empty;
        public string FenceTitle { get; set; } = string.Empty;
        public int    Count      { get; set; }
        public bool   Accepted   { get; set; }
        public bool   Dismissed  { get; set; }
    }

    public sealed class RuleSuggestion
    {
        public string Extension { get; set; } = string.Empty;
        public Guid   FenceId   { get; set; }
        public string FenceTitle{ get; set; } = string.Empty;
        public int    MoveCount { get; set; }
    }

    private sealed class BehaviorPersistence
    {
        public Dictionary<string, Dictionary<string, BehaviorObservation>>? Observations { get; set; }
        public List<string>? Dismissed { get; set; }
    }

    // ── Construction ─────────────────────────────────────────────────────────

    private BehaviorLearningService()
    {
        DetachedTaskObserver.Run(
            LoadAsync(),
            ex => Serilog.Log.Warning(ex, "BehaviorLearningService: failed to load behavior data"));
    }

    // ── Public API ───────────────────────────────────────────────────────────

    /// <summary>
    /// Record that the user manually dropped a file with <paramref name="extension"/>
    /// into the fence <paramref name="fenceId"/>. Call this from
    /// FenceWindow.HandleShellDrop for each accepted item.
    /// </summary>
    public void RecordItemDroppedToFence(string extension, Guid fenceId, string fenceTitle)
    {
        if (string.IsNullOrWhiteSpace(extension) || fenceId == Guid.Empty)
            return;

        extension = NormaliseExt(extension);
        string fenceIdStr = fenceId.ToString("N");
        string dismissKey = MakeDismissKey(extension, fenceIdStr);

        RuleSuggestion? toFire = null;

        lock (_lock)
        {
            if (_dismissed.Contains(dismissKey))
                return;

            if (!_observations.TryGetValue(extension, out var byFence))
            {
                byFence = new Dictionary<string, BehaviorObservation>();
                _observations[extension] = byFence;
            }

            if (!byFence.TryGetValue(fenceIdStr, out var obs))
            {
                obs = new BehaviorObservation { FenceId = fenceIdStr, FenceTitle = fenceTitle };
                byFence[fenceIdStr] = obs;
            }

            obs.FenceTitle = fenceTitle;
            obs.Count++;

            if (obs.Count == SuggestionThreshold && !obs.Accepted && !obs.Dismissed)
            {
                toFire = new RuleSuggestion
                {
                    Extension  = extension,
                    FenceId    = fenceId,
                    FenceTitle = fenceTitle,
                    MoveCount  = obs.Count,
                };
            }
        }

        ScheduleSave();

        if (toFire != null)
        {
            Serilog.Log.Information(
                "BehaviorLearning: rule suggestion triggered — {Ext} → '{Fence}' ({Count}x)",
                toFire.Extension, toFire.FenceTitle, toFire.MoveCount);
            RuleSuggested?.Invoke(this, toFire);
        }
    }

    /// <summary>Returns all pending suggestions (threshold crossed, not dismissed or accepted).</summary>
    public IReadOnlyList<RuleSuggestion> GetPendingSuggestions()
    {
        lock (_lock)
        {
            var result = new List<RuleSuggestion>();
            foreach (var (ext, byFence) in _observations)
            {
                foreach (var (fenceIdStr, obs) in byFence)
                {
                    if (obs.Count >= SuggestionThreshold && !obs.Accepted && !obs.Dismissed
                        && Guid.TryParse(obs.FenceId, out var fenceId))
                    {
                        result.Add(new RuleSuggestion
                        {
                            Extension  = ext,
                            FenceId    = fenceId,
                            FenceTitle = obs.FenceTitle,
                            MoveCount  = obs.Count,
                        });
                    }
                }
            }
            return result;
        }
    }

    /// <summary>
    /// Returns the highest-confidence learned fence for a file extension.
    /// Only considers non-dismissed observations.
    /// </summary>
    public Guid? TrySuggestFenceForExtension(string extension)
    {
        if (string.IsNullOrWhiteSpace(extension))
            return null;

        extension = NormaliseExt(extension);

        lock (_lock)
        {
            if (!_observations.TryGetValue(extension, out var byFence) || byFence.Count == 0)
                return null;

            BehaviorObservation? best = byFence.Values
                .Where(o => !o.Dismissed)
                .OrderByDescending(o => o.Accepted)
                .ThenByDescending(o => o.Count)
                .FirstOrDefault();

            if (best == null)
                return null;

            return Guid.TryParse(best.FenceId, out Guid parsed) ? parsed : null;
        }
    }

    /// <summary>User dismissed the suggestion — never re-suggest this ext+fence pairing.</summary>
    public void DismissSuggestion(string extension, Guid fenceId)
    {
        extension = NormaliseExt(extension);
        string fenceIdStr = fenceId.ToString("N");
        string key        = MakeDismissKey(extension, fenceIdStr);

        lock (_lock)
        {
            _dismissed.Add(key);
            if (_observations.TryGetValue(extension, out var byFence)
                && byFence.TryGetValue(fenceIdStr, out var obs))
                obs.Dismissed = true;
        }

        ScheduleSave();
    }

    /// <summary>
    /// User accepted the suggestion — automatically creates a permanent sort rule
    /// and marks this pairing so it is never suggested again.
    /// </summary>
    public SortRulesEngine.SortRule? AcceptSuggestion(string extension, Guid fenceId, string fenceTitle)
    {
        extension = NormaliseExt(extension);
        string fenceIdStr = fenceId.ToString("N");

        lock (_lock)
        {
            _dismissed.Add(MakeDismissKey(extension, fenceIdStr));
            if (_observations.TryGetValue(extension, out var byFence)
                && byFence.TryGetValue(fenceIdStr, out var obs))
                obs.Accepted = true;
        }

        var rule = new SortRulesEngine.SortRule
        {
            Name             = $"Auto: {extension} → {fenceTitle}",
            TargetFenceId    = fenceId,
            TargetFenceTitle = fenceTitle,
            Type             = SortRulesEngine.RuleType.FileExtension,
            Pattern          = extension,
            Priority         = 50,
        };

        SortRulesEngine.Instance.AddRule(rule);
        _ = SortRulesEngine.Instance.SaveAsync();
        ScheduleSave();

        Serilog.Log.Information(
            "BehaviorLearning: accepted suggestion — created rule for {Ext} → '{Fence}'",
            extension, fenceTitle);

        return rule;
    }

    // ── Persistence ──────────────────────────────────────────────────────────

    private void ScheduleSave()
    {
        if (_saveTimer is null)
        {
            _saveTimer = new Timer(_ =>
                DetachedTaskObserver.Run(
                    SaveAsync(),
                    ex => Serilog.Log.Warning(ex, "BehaviorLearningService: failed to persist behavior data")),
                null,
                TimeSpan.FromMilliseconds(800), Timeout.InfiniteTimeSpan);
        }
        else
        {
            _saveTimer.Change(TimeSpan.FromMilliseconds(800), Timeout.InfiniteTimeSpan);
        }
    }

    public async Task SaveAsync()
    {
        try
        {
            BehaviorPersistence payload;
            lock (_lock)
            {
                payload = new BehaviorPersistence
                {
                    Observations = new Dictionary<string, Dictionary<string, BehaviorObservation>>(_observations),
                    Dismissed    = _dismissed.ToList(),
                };
            }

            string path = AppPaths.BehaviorLog;
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);

            string bak = path + ".bak";
            if (File.Exists(path))
                File.Move(path, bak, overwrite: true);

            await File.WriteAllTextAsync(path,
                JsonSerializer.Serialize(payload, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "BehaviorLearningService: save failed");
        }
    }

    private async Task LoadAsync()
    {
        try
        {
            string path = AppPaths.BehaviorLog;
            if (!File.Exists(path))
                return;

            string json    = await File.ReadAllTextAsync(path);
            var    payload = JsonSerializer.Deserialize<BehaviorPersistence>(json,
                new JsonSerializerOptions { PropertyNameCaseInsensitive = true });

            if (payload is null)
                return;

            lock (_lock)
            {
                if (payload.Observations is not null)
                    _observations = payload.Observations;
                if (payload.Dismissed is not null)
                    foreach (string d in payload.Dismissed)
                        _dismissed.Add(d);
            }

            Serilog.Log.Debug("BehaviorLearningService: loaded {Count} extension observations",
                _observations.Count);
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "BehaviorLearningService: load failed — starting fresh");
        }
    }

    // ── Helpers ──────────────────────────────────────────────────────────────

    private static string NormaliseExt(string ext)
    {
        ext = ext.ToLowerInvariant().Trim();
        return ext.StartsWith('.') ? ext : "." + ext;
    }

    private static string MakeDismissKey(string ext, string fenceIdStr) =>
        $"{ext}|{fenceIdStr}";
}
