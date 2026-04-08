using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Observes manual drag-drop moves to learn each user's file-type → space preferences.
/// After <see cref="SuggestionThreshold"/> moves of the same file extension to the same
/// space, fires <see cref="RuleSuggested"/> so the shell can prompt the user to
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

    /// <summary>Number of manual moves of (ext → space) that triggers a rule suggestion.</summary>
    public const int SuggestionThreshold = 3;

    private readonly object _lock = new();

    // ext (normalised, e.g. ".png") → spaceId string → observation
    private Dictionary<string, Dictionary<string, BehaviorObservation>> _observations = new();

    // "ext|spaceIdN" keys that must never be re-suggested (dismissed or accepted)
    private readonly HashSet<string> _dismissed = new();

    private Timer? _saveTimer;

    public event EventHandler<RuleSuggestion>? RuleSuggested;

    // ── Nested types ─────────────────────────────────────────────────────────

    public sealed class BehaviorObservation
    {
        public string SpaceId    { get; set; } = string.Empty;
        public string SpaceTitle { get; set; } = string.Empty;
        public int    Count      { get; set; }
        public bool   Accepted   { get; set; }
        public bool   Dismissed  { get; set; }
    }

    public sealed class RuleSuggestion
    {
        public string Extension { get; set; } = string.Empty;
        public Guid   SpaceId   { get; set; }
        public string SpaceTitle{ get; set; } = string.Empty;
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
    /// into the space <paramref name="spaceId"/>. Call this from
    /// SpaceWindow.HandleShellDrop for each accepted item.
    /// </summary>
    public void RecordItemDroppedToSpace(string extension, Guid spaceId, string spaceTitle)
    {
        if (string.IsNullOrWhiteSpace(extension) || spaceId == Guid.Empty)
            return;

        extension = NormaliseExt(extension);
        string spaceIdStr = spaceId.ToString("N");
        string dismissKey = MakeDismissKey(extension, spaceIdStr);

        RuleSuggestion? toFire = null;

        lock (_lock)
        {
            if (_dismissed.Contains(dismissKey))
                return;

            if (!_observations.TryGetValue(extension, out var bySpace))
            {
                bySpace = new Dictionary<string, BehaviorObservation>();
                _observations[extension] = bySpace;
            }

            if (!bySpace.TryGetValue(spaceIdStr, out var obs))
            {
                obs = new BehaviorObservation { SpaceId = spaceIdStr, SpaceTitle = spaceTitle };
                bySpace[spaceIdStr] = obs;
            }

            obs.SpaceTitle = spaceTitle;
            obs.Count++;

            if (obs.Count == SuggestionThreshold && !obs.Accepted && !obs.Dismissed)
            {
                toFire = new RuleSuggestion
                {
                    Extension  = extension,
                    SpaceId    = spaceId,
                    SpaceTitle = spaceTitle,
                    MoveCount  = obs.Count,
                };
            }
        }

        ScheduleSave();

        if (toFire != null)
        {
            Serilog.Log.Information(
                "BehaviorLearning: rule suggestion triggered — {Ext} → '{Space}' ({Count}x)",
                toFire.Extension, toFire.SpaceTitle, toFire.MoveCount);
            RuleSuggested?.Invoke(this, toFire);
        }
    }

    /// <summary>Returns all pending suggestions (threshold crossed, not dismissed or accepted).</summary>
    public IReadOnlyList<RuleSuggestion> GetPendingSuggestions()
    {
        lock (_lock)
        {
            var result = new List<RuleSuggestion>();
            foreach (var (ext, bySpace) in _observations)
            {
                foreach (var (spaceIdStr, obs) in bySpace)
                {
                    if (obs.Count >= SuggestionThreshold && !obs.Accepted && !obs.Dismissed
                        && Guid.TryParse(obs.SpaceId, out var spaceId))
                    {
                        result.Add(new RuleSuggestion
                        {
                            Extension  = ext,
                            SpaceId    = spaceId,
                            SpaceTitle = obs.SpaceTitle,
                            MoveCount  = obs.Count,
                        });
                    }
                }
            }
            return result;
        }
    }

    /// <summary>
    /// Returns the highest-confidence learned space for a file extension.
    /// Only considers non-dismissed observations.
    /// </summary>
    public Guid? TrySuggestSpaceForExtension(string extension)
    {
        if (string.IsNullOrWhiteSpace(extension))
            return null;

        extension = NormaliseExt(extension);

        lock (_lock)
        {
            if (!_observations.TryGetValue(extension, out var bySpace) || bySpace.Count == 0)
                return null;

            BehaviorObservation? best = bySpace.Values
                .Where(o => !o.Dismissed)
                .OrderByDescending(o => o.Accepted)
                .ThenByDescending(o => o.Count)
                .FirstOrDefault();

            if (best == null)
                return null;

            return Guid.TryParse(best.SpaceId, out Guid parsed) ? parsed : null;
        }
    }

    /// <summary>User dismissed the suggestion — never re-suggest this ext+space pairing.</summary>
    public void DismissSuggestion(string extension, Guid spaceId)
    {
        extension = NormaliseExt(extension);
        string spaceIdStr = spaceId.ToString("N");
        string key        = MakeDismissKey(extension, spaceIdStr);

        lock (_lock)
        {
            _dismissed.Add(key);
            if (_observations.TryGetValue(extension, out var bySpace)
                && bySpace.TryGetValue(spaceIdStr, out var obs))
                obs.Dismissed = true;
        }

        ScheduleSave();
    }

    /// <summary>
    /// User accepted the suggestion — automatically creates a permanent sort rule
    /// and marks this pairing so it is never suggested again.
    /// </summary>
    public SortRulesEngine.SortRule? AcceptSuggestion(string extension, Guid spaceId, string spaceTitle)
    {
        extension = NormaliseExt(extension);
        string spaceIdStr = spaceId.ToString("N");

        lock (_lock)
        {
            _dismissed.Add(MakeDismissKey(extension, spaceIdStr));
            if (_observations.TryGetValue(extension, out var bySpace)
                && bySpace.TryGetValue(spaceIdStr, out var obs))
                obs.Accepted = true;
        }

        var rule = new SortRulesEngine.SortRule
        {
            Name             = $"Auto: {extension} → {spaceTitle}",
            TargetSpaceId    = spaceId,
            TargetSpaceTitle = spaceTitle,
            Type             = SortRulesEngine.RuleType.FileExtension,
            Pattern          = extension,
            Priority         = 50,
        };

        SortRulesEngine.Instance.AddRule(rule);
        _ = SortRulesEngine.Instance.SaveAsync();
        ScheduleSave();

        Serilog.Log.Information(
            "BehaviorLearning: accepted suggestion — created rule for {Ext} → '{Space}'",
            extension, spaceTitle);

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

    private static string MakeDismissKey(string ext, string spaceIdStr) =>
        $"{ext}|{spaceIdStr}";
}
