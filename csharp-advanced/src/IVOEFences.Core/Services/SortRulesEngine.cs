using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;
using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Step 32: Auto-sort rules engine for automatic fence organization.
/// Categorizes and distributes desktop items to appropriate fences based on rules.
/// Rules can be based on file type, folder, name patterns, or custom logic.
/// </summary>
public sealed class SortRulesEngine
{
    private static SortRulesEngine? _instance;
    private static readonly object _lock = new();

    private readonly List<SortRule> _rules = new();

    public static SortRulesEngine Instance
    {
        get
        {
            if (_instance == null)
            {
                lock (_lock)
                {
                    if (_instance == null)
                        _instance = new SortRulesEngine();
                }
            }
            return _instance;
        }
    }

    private SortRulesEngine()
    {
        InitializeDefaultRules();
    }

    /// <summary>
    /// Represents a rule for automatically sorting items into a fence.
    /// </summary>
    public sealed class SortRule
    {
        public Guid Id { get; init; } = Guid.NewGuid();
        public string Name { get; init; } = string.Empty;
        /// <summary>Resolved fence ID (populated at runtime after loading/parsing).</summary>
        public Guid TargetFenceId { get; set; }
        /// <summary>Human-readable fence title — used by DSL parsing; resolved to TargetFenceId on load.</summary>
        public string TargetFenceTitle { get; set; } = string.Empty;
        public RuleType Type { get; init; }
        public string Pattern { get; init; } = string.Empty; // File extension, folder name, regex, etc.
        public bool Enabled { get; set; } = true;
        public int Priority { get; init; } // 0 = highest; higher number = lower priority
    }

    public enum RuleType
    {
        FileExtension = 0,  // Pattern: ".txt", ".pdf", etc.
        FolderName = 1,    // Pattern: folder name or path
        NamePattern = 2,   // Pattern: regex or simple wildcard
        FileType = 3,      // Pattern: "image", "document", "video", "executable", etc.
        Custom = 4         // Pattern: custom logic
    }

    public event EventHandler<RuleChangedEventArgs>? RulesChanged;

    public sealed class RuleChangedEventArgs : EventArgs
    {
        public SortRule? Rule { get; init; }
        public string Action { get; init; } = string.Empty; // "Added", "Modified", "Deleted"
    }

    /// <summary>
    /// Step 32: Initialize default sort rules (examples for common file types).
    /// </summary>
    private void InitializeDefaultRules()
    {
        try
        {
            var fences = FenceStateService.Instance.Fences;
            
            // Example: Create rules if we have at least 2 fences
            // In real usage, these would be user-configured or persisted
            // For now, return empty - user creates rules via settings
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to initialize default sort rules");
        }
    }

    /// <summary>
    /// Step 32: Add a new sort rule.
    /// </summary>
    public void AddRule(SortRule rule)
    {
        if (rule == null)
            return;

        _rules.Add(rule);
        _rules.Sort((a, b) => a.Priority.CompareTo(b.Priority)); // Maintain priority order

        RulesChanged?.Invoke(this, new RuleChangedEventArgs 
        { 
            Rule = rule, 
            Action = "Added" 
        });

        Serilog.Log.Information("Sort rule added: {RuleName} → Fence {FenceId}", 
            rule.Name, rule.TargetFenceId);
    }

    /// <summary>
    /// Step 32: Remove a sort rule by ID.
    /// </summary>
    public bool RemoveRule(Guid ruleId)
    {
        var rule = _rules.FirstOrDefault(r => r.Id == ruleId);
        if (rule == null)
            return false;

        _rules.Remove(rule);

        RulesChanged?.Invoke(this, new RuleChangedEventArgs 
        { 
            Rule = rule, 
            Action = "Deleted" 
        });

        Serilog.Log.Information("Sort rule removed: {RuleId}", ruleId);
        return true;
    }

    public void ClearRules()
    {
        _rules.Clear();
        Serilog.Log.Information("SortRulesEngine: cleared all rules");
    }

    /// <summary>
    /// Step 32: Determine which fence an item should go to based on rules.
    /// Returns null if no rule matches.
    /// </summary>
    public Guid? DetermineFenceForItem(FenceItemModel item)
    {
        if (string.IsNullOrEmpty(item.TargetPath))
            return null;

        var fences = FenceStateService.Instance.Fences;
        var fenceIds = new HashSet<Guid>(fences.Select(f => f.Id));

        foreach (var rule in _rules.Where(r => r.Enabled))
        {
            if (MatchesRule(item, rule))
            {
                // Verify the target fence still exists
                if (fenceIds.Contains(rule.TargetFenceId))
                    return rule.TargetFenceId;

                Serilog.Log.Warning("Sort rule {RuleName} targets non-existent fence {FenceId}, skipping",
                    rule.Name, rule.TargetFenceId);
            }
        }

        return null;
    }

    /// <summary>
    /// Step 32: Test if an item matches a sort rule.
    /// </summary>
    private bool MatchesRule(FenceItemModel item, SortRule rule)
    {
        try
        {
            return rule.Type switch
            {
                RuleType.FileExtension => MatchesFileExtension(item.TargetPath, rule.Pattern),
                RuleType.FolderName => MatchesFolderName(item.TargetPath, rule.Pattern),
                RuleType.NamePattern => MatchesNamePattern(Path.GetFileName(item.TargetPath), rule.Pattern),
                RuleType.FileType => MatchesFileType(item.TargetPath, rule.Pattern),
                RuleType.Custom => MatchesCustomRule(item, rule.Pattern),
                _ => false
            };
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "Rule matching failed for {RuleType} pattern {Pattern}", rule.Type, rule.Pattern);
            return false;
        }
    }

    private bool MatchesFileExtension(string path, string pattern)
    {
        var ext = Path.GetExtension(path).ToLowerInvariant();
        var normalizedPattern = pattern.ToLowerInvariant();
        
        if (!normalizedPattern.StartsWith("."))
            normalizedPattern = "." + normalizedPattern;
        
        return ext == normalizedPattern;
    }

    private bool MatchesFolderName(string path, string pattern)
    {
        try
        {
            var parentDir = Path.GetDirectoryName(path) ?? string.Empty;
            return parentDir.Contains(pattern, StringComparison.OrdinalIgnoreCase);
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "Folder name matching failed for path {Path}", path);
            return false;
        }
    }

    private bool MatchesNamePattern(string fileName, string pattern)
    {
        // Simple wildcard matching: * = any characters
        var regex = "^" + System.Text.RegularExpressions.Regex.Escape(pattern)
            .Replace("\\*", ".*")
            .Replace("\\?", ".") + "$";

        try
        {
            return System.Text.RegularExpressions.Regex.IsMatch(fileName, regex, 
                System.Text.RegularExpressions.RegexOptions.IgnoreCase);
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "Name pattern matching failed for {FileName} with pattern {Pattern}", fileName, pattern);
            return false;
        }
    }

    /// <summary>
    /// Custom rules support a simple multi-condition DSL.
    /// Tokens are space-separated; ALL must match (AND logic).
    /// Supported prefixes: ext:&lt;value&gt;, name:&lt;wildcard&gt;, type:&lt;category&gt;, folder:&lt;name&gt;
    /// Example: "ext:.pdf name:invoice*" matches PDF files whose name contains "invoice".
    /// </summary>
    private bool MatchesCustomRule(FenceItemModel item, string pattern)
    {
        if (string.IsNullOrWhiteSpace(pattern))
            return false;

        var tokens = pattern.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (tokens.Length == 0)
            return false;

        foreach (var token in tokens)
        {
            var parts = token.Split(':', 2);
            if (parts.Length != 2)
                return false;

            bool matches = parts[0].ToLowerInvariant() switch
            {
                "ext"    => MatchesFileExtension(item.TargetPath, parts[1]),
                // extOR: allows OR-semantics on a pipe-separated extension list, e.g. "extOR:.png|.jpg|.jpeg"
                "extor"  => parts[1].Split('|', StringSplitOptions.RemoveEmptyEntries)
                                    .Any(e => MatchesFileExtension(item.TargetPath, e)),
                "name"   => MatchesNamePattern(Path.GetFileName(item.TargetPath), parts[1]),
                "type"   => MatchesFileType(item.TargetPath, parts[1]),
                "folder" => MatchesFolderName(item.TargetPath, parts[1]),
                _        => false
            };

            if (!matches)
                return false;
        }
        return true;
    }

    private bool MatchesFileType(string path, string fileType)
    {
        var ext = Path.GetExtension(path).ToLowerInvariant();
        
        var categories = fileType.ToLowerInvariant() switch
        {
            "image" => new[] { ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tiff", ".webp", ".svg" },
            "document" => new[] { ".pdf", ".doc", ".docx", ".txt", ".xls", ".xlsx", ".ppt", ".pptx" },
            "video" => new[] { ".mp4", ".avi", ".mkv", ".mov", ".wmv", ".flv", ".webm", ".m4v" },
            "audio" => new[] { ".mp3", ".wav", ".flac", ".aac", ".ogg", ".wma", ".m4a", ".opus" },
            "archive" => new[] { ".zip", ".rar", ".7z", ".tar", ".gz", ".bz2", ".iso" },
            "executable" => new[] { ".exe", ".msi", ".bat", ".cmd", ".com", ".app", ".deb" },
            "code" => new[] { ".cs", ".cpp", ".py", ".js", ".java", ".rb", ".go", ".rs", ".ts", ".html", ".css" },
            _ => Array.Empty<string>()
        };

        return categories.Contains(ext);
    }

    /// <summary>
    /// Step 32: Get all active sort rules.
    /// </summary>
    public IReadOnlyList<SortRule> GetRules() => _rules.AsReadOnly();

    /// <summary>
    /// Step 32: Enable or disable a rule without deleting it.
    /// </summary>
    public bool SetRuleEnabled(Guid ruleId, bool enabled)
    {
        var rule = _rules.FirstOrDefault(r => r.Id == ruleId);
        if (rule == null)
            return false;

        rule.Enabled = enabled;
        RulesChanged?.Invoke(this, new RuleChangedEventArgs { Rule = rule, Action = "Modified" });
        Serilog.Log.Debug("Sort rule {RuleId} enabled={Enabled}", ruleId, enabled);
        return true;
    }

    /// <summary>
    /// Step 32: Auto-organize a list of items into fences based on rules.
    /// Returns mapping of item → target fence ID.
    /// </summary>
    public Dictionary<Guid, Guid?> ClassifyItems(IEnumerable<FenceItemModel> items)
    {
        var result = new Dictionary<Guid, Guid?>();

        foreach (var item in items)
        {
            var targetFence = DetermineFenceForItem(item);
            result[item.Id] = targetFence;
        }

        return result;
    }

    /// <summary>
    /// Resolves TargetFenceTitle → TargetFenceId for rules loaded from DSL text.
    /// Must be called after rules are added and fences are available.
    /// </summary>
    public void ResolveFenceTitles(IEnumerable<FenceModel> fences)
    {
        var fenceList = fences.ToList();
        foreach (var rule in _rules)
        {
            if (rule.TargetFenceId != Guid.Empty || string.IsNullOrEmpty(rule.TargetFenceTitle))
                continue;

            var match = fenceList.FirstOrDefault(f =>
                string.Equals(f.Title, rule.TargetFenceTitle, StringComparison.OrdinalIgnoreCase));

            if (match != null)
            {
                rule.TargetFenceId = match.Id;
                Serilog.Log.Debug("SortRulesEngine: resolved title '{Title}' → {Id}", rule.TargetFenceTitle, match.Id);
            }
            else
            {
                Serilog.Log.Warning("SortRulesEngine: could not resolve fence title '{Title}'", rule.TargetFenceTitle);
            }
        }
    }

    /// <summary>
    /// Persist the current rule set to rules.json (overwrites defaults with user rules).
    /// </summary>
    public async Task SaveAsync()
    {
        try
        {
            string dir = Path.GetDirectoryName(AppPaths.RulesConfig)!;
            Directory.CreateDirectory(dir);

            var options = new JsonSerializerOptions { WriteIndented = true };
            string json = JsonSerializer.Serialize(_rules, options);

            // Atomic write with backup rotation
            string path = AppPaths.RulesConfig;
            string bak  = path + ".bak";
            string bak2 = path + ".bak2";
            string bak3 = path + ".bak3";

            if (File.Exists(bak2)) File.Move(bak2, bak3, overwrite: true);
            if (File.Exists(bak))  File.Move(bak,  bak2, overwrite: true);
            if (File.Exists(path)) File.Move(path,  bak,  overwrite: true);

            await File.WriteAllTextAsync(path, json).ConfigureAwait(false);
            Serilog.Log.Debug("SortRulesEngine: saved {Count} rules to {Path}", _rules.Count, path);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "SortRulesEngine: failed to save rules");
        }
    }

    /// <summary>
    /// Load rules from rules.json if it exists, replacing in-memory defaults.
    /// </summary>
    public async Task LoadAsync()
    {
        string path = AppPaths.RulesConfig;
        if (!File.Exists(path))
            return;

        try
        {
            string json = await File.ReadAllTextAsync(path).ConfigureAwait(false);
            var loaded = JsonSerializer.Deserialize<List<SortRule>>(json);
            if (loaded == null || loaded.Count == 0)
                return;

            _rules.Clear();
            _rules.AddRange(loaded);
            RulesChanged?.Invoke(this, new RuleChangedEventArgs { Rule = _rules[0], Action = "Loaded" });
            Serilog.Log.Information("SortRulesEngine: loaded {Count} rules from {Path}", _rules.Count, path);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "SortRulesEngine: failed to load rules from {Path}", path);
        }
    }
}

