using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Parses a human-readable rule DSL into <see cref="SortRulesEngine.SortRule"/> objects.
///
/// Syntax (case-insensitive):
///   rule "Display Name"
///   when &lt;condition&gt;
///   then move to "SpaceName"
///   [priority &lt;number&gt;]
///
/// Supported conditions:
///   ext == ".png"
///   ext in [".png", ".jpg", ".jpeg"]
///   ext != ".tmp"
///   name == "readme"
///   name starts_with "invoice"
///   name contains "report"
///   type == "image"              (image|document|video|audio|archive|executable|code)
///   folder contains "Downloads"
///
/// Multiple conditions are joined with AND (all must match).
/// Each condition becomes a separate Custom-type SortRule entry or, when a single
/// simple ext/type/name condition is used, maps to the corresponding RuleType.
///
/// Example:
///   rule "Images Auto Sort"
///   when ext in [".png", ".jpg", ".jpeg"]
///   then move to "Images"
///   priority 10
/// </summary>
public sealed class RuleDslParser
{
    // ── Public surface ──────────────────────────────────────────────────────

    public sealed class ParseResult
    {
        public List<SortRulesEngine.SortRule> Rules { get; } = new();
        public List<string> Errors { get; } = new();
        public bool IsSuccess => Errors.Count == 0;
    }

    /// <summary>
    /// Parse one or more rule blocks from <paramref name="dslText"/> and
    /// return the resulting <see cref="SortRulesEngine.SortRule"/> list plus any
    /// parse errors.  Rules map to spaces by title; the caller is responsible
    /// for resolving space titles to IDs after calling this method.
    /// </summary>
    public ParseResult Parse(string dslText)
    {
        var result = new ParseResult();
        if (string.IsNullOrWhiteSpace(dslText))
            return result;

        // Split into blocks delimited by "rule" keyword
        var blocks = SplitIntoBlocks(dslText);
        foreach (var (lineNo, block) in blocks)
        {
            try
            {
                var rule = ParseBlock(block, lineNo, result.Errors);
                if (rule != null)
                    result.Rules.Add(rule);
            }
            catch (Exception ex)
            {
                result.Errors.Add($"Line {lineNo}: unexpected error — {ex.Message}");
            }
        }

        return result;
    }

    /// <summary>
    /// Convert a <see cref="SortRulesEngine.SortRule"/> back to DSL text.
    /// Useful for round-tripping rules through the settings editor.
    /// </summary>
    public string Export(IEnumerable<SortRulesEngine.SortRule> rules)
    {
        var sb = new System.Text.StringBuilder();
        foreach (var rule in rules)
        {
            sb.AppendLine($"rule \"{EscapeQuotes(rule.Name)}\"");

            string conditionText = rule.Type switch
            {
                SortRulesEngine.RuleType.FileExtension =>
                    $"when ext == \"{rule.Pattern}\"",
                SortRulesEngine.RuleType.FileType =>
                    $"when type == \"{rule.Pattern}\"",
                SortRulesEngine.RuleType.NamePattern =>
                    $"when name contains \"{rule.Pattern}\"",
                SortRulesEngine.RuleType.FolderName =>
                    $"when folder contains \"{rule.Pattern}\"",
                SortRulesEngine.RuleType.Custom =>
                    $"when {BuildCustomCondition(rule.Pattern)}",
                _ => $"when ext == \"{rule.Pattern}\""
            };
            sb.AppendLine(conditionText);

            // Note: target space title is stored in the pattern for Custom; for others
            // the rule.TargetSpaceId must be resolved by the caller.
            sb.AppendLine($"then move to \"(space-id:{rule.TargetSpaceId})\"");
            sb.AppendLine($"priority {rule.Priority}");
            sb.AppendLine();
        }
        return sb.ToString();
    }

    // ── Block splitting ─────────────────────────────────────────────────────

    /// <summary>Split DSL text into (startLine, lines[]) blocks, one per "rule" keyword.</summary>
    private static List<(int startLine, string[] lines)> SplitIntoBlocks(string text)
    {
        var blocks = new List<(int, string[])>();
        var lines = text.Split('\n');
        var current = new List<string>();
        int blockStart = 1;

        for (int i = 0; i < lines.Length; i++)
        {
            string trimmed = lines[i].Trim();
            if (trimmed.StartsWith("rule ", StringComparison.OrdinalIgnoreCase) && current.Count > 0)
            {
                blocks.Add((blockStart, current.ToArray()));
                current.Clear();
                blockStart = i + 1;
            }
            current.Add(trimmed);
        }

        if (current.Count > 0)
            blocks.Add((blockStart, current.ToArray()));

        return blocks;
    }

    // ── Single block parser ─────────────────────────────────────────────────

    private static SortRulesEngine.SortRule? ParseBlock(string[] lines, int startLine, List<string> errors)
    {
        string? ruleName = null;
        string? targetSpaceTitle = null;
        int priority = 50;
        var conditionLines = new List<string>();

        foreach (string line in lines)
        {
            if (string.IsNullOrWhiteSpace(line) || line.StartsWith("//") || line.StartsWith("#"))
                continue;

            if (line.StartsWith("rule ", StringComparison.OrdinalIgnoreCase))
            {
                ruleName = ExtractQuoted(line.Substring(5));
                continue;
            }

            if (line.StartsWith("when ", StringComparison.OrdinalIgnoreCase))
            {
                conditionLines.Add(line.Substring(5).Trim());
                continue;
            }

            if (line.StartsWith("then move to ", StringComparison.OrdinalIgnoreCase))
            {
                targetSpaceTitle = ExtractQuoted(line.Substring(14).Trim());
                continue;
            }

            if (line.StartsWith("priority ", StringComparison.OrdinalIgnoreCase) &&
                int.TryParse(line.Substring(9).Trim(), out int p))
            {
                priority = Math.Clamp(p, 0, 1000);
                continue;
            }
        }

        if (ruleName is null)
        {
            errors.Add($"Line {startLine}: rule block missing 'rule \"Name\"' declaration");
            return null;
        }

        if (conditionLines.Count == 0)
        {
            errors.Add($"Line {startLine}: rule '{ruleName}' has no 'when' condition");
            return null;
        }

        if (targetSpaceTitle is null)
        {
            errors.Add($"Line {startLine}: rule '{ruleName}' has no 'then move to' target");
            return null;
        }

        // Try to map to a typed rule when there is exactly one simple condition
        if (conditionLines.Count == 1)
        {
            var typed = TryParseTypedCondition(conditionLines[0], ruleName, targetSpaceTitle, priority);
            if (typed != null)
                return typed;
        }

        // Fall back to Custom DSL pattern (multi-condition AND)
        string customPattern = BuildCustomPattern(conditionLines);
        return new SortRulesEngine.SortRule
        {
            Name           = ruleName,
            TargetSpaceTitle = targetSpaceTitle,
            Type           = SortRulesEngine.RuleType.Custom,
            Pattern        = customPattern,
            Priority       = priority,
        };
    }

    // ── Typed condition parser ──────────────────────────────────────────────

    // Ext patterns: "ext == ".png"" or "ext in [".png", ".jpg"]"
    private static readonly Regex ExtEq   = new(@"^ext\s*==\s*""([^""]+)""$",   RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex ExtNeq  = new(@"^ext\s*!=\s*""([^""]+)""$",   RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex ExtIn   = new(@"^ext\s+in\s+\[([^\]]+)\]$",   RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex TypeEq  = new(@"^type\s*==\s*""([^""]+)""$",  RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex NameEq  = new(@"^name\s*==\s*""([^""]+)""$",  RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex NameSW  = new(@"^name\s+starts_with\s+""([^""]+)""$", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex NameCon = new(@"^name\s+contains\s+""([^""]+)""$",    RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex FolderCon = new(@"^folder\s+contains\s+""([^""]+)""$", RegexOptions.IgnoreCase | RegexOptions.Compiled);

    private static SortRulesEngine.SortRule? TryParseTypedCondition(
        string condition, string ruleName, string targetSpaceTitle, int priority)
    {
        Match m;

        m = ExtEq.Match(condition);
        if (m.Success)
            return MakeRule(ruleName, targetSpaceTitle, priority,
                SortRulesEngine.RuleType.FileExtension, NormalizeExt(m.Groups[1].Value));

        m = ExtIn.Match(condition);
        if (m.Success)
        {
            // Multiple extensions → join as custom pattern
            var exts = ExtractListValues(m.Groups[1].Value);
            if (exts.Count == 1)
                return MakeRule(ruleName, targetSpaceTitle, priority,
                    SortRulesEngine.RuleType.FileExtension, NormalizeExt(exts[0]));

            // Build custom DSL pattern covering all exts
            string pattern = string.Join(" ", exts.Select(e => $"ext:{NormalizeExt(e)}"));
            // For "any of" semantics we need OR — but custom is AND.
            // Strategy: emit one rule per extension, sharing the same name+priority.
            // Return null so caller takes the multi-condition path which OR-expands.
            return null; // handled by caller
        }

        m = TypeEq.Match(condition);
        if (m.Success)
            return MakeRule(ruleName, targetSpaceTitle, priority,
                SortRulesEngine.RuleType.FileType, m.Groups[1].Value.ToLowerInvariant());

        m = NameEq.Match(condition);
        if (m.Success)
            return MakeRule(ruleName, targetSpaceTitle, priority,
                SortRulesEngine.RuleType.NamePattern, m.Groups[1].Value);

        m = NameSW.Match(condition);
        if (m.Success)
            return MakeRule(ruleName, targetSpaceTitle, priority,
                SortRulesEngine.RuleType.NamePattern, m.Groups[1].Value + "*");

        m = NameCon.Match(condition);
        if (m.Success)
            return MakeRule(ruleName, targetSpaceTitle, priority,
                SortRulesEngine.RuleType.NamePattern, "*" + m.Groups[1].Value + "*");

        m = FolderCon.Match(condition);
        if (m.Success)
            return MakeRule(ruleName, targetSpaceTitle, priority,
                SortRulesEngine.RuleType.FolderName, m.Groups[1].Value);

        return null;
    }

    /// <summary>
    /// For "ext in [".png", ".jpg"]" patterns that fall through to Custom, expand
    /// into separate extension-check tokens joined with spaces (interpreted as OR
    /// inside the Custom rule's MatchesCustomRule using the extOR: prefix).
    /// </summary>
    private static string BuildCustomPattern(List<string> conditionLines)
    {
        var tokens = new List<string>();
        foreach (string cond in conditionLines)
        {
            Match m;

            m = ExtEq.Match(cond);
            if (m.Success) { tokens.Add($"ext:{NormalizeExt(m.Groups[1].Value)}"); continue; }

            m = ExtIn.Match(cond);
            if (m.Success)
            {
                var exts = ExtractListValues(m.Groups[1].Value);
                // OR mode: encoded as extOR:ext1|ext2|ext3
                tokens.Add("extOR:" + string.Join("|", exts.Select(NormalizeExt)));
                continue;
            }

            m = TypeEq.Match(cond);
            if (m.Success) { tokens.Add($"type:{m.Groups[1].Value.ToLowerInvariant()}"); continue; }

            m = NameSW.Match(cond);
            if (m.Success) { tokens.Add($"name:{m.Groups[1].Value}*"); continue; }

            m = NameCon.Match(cond);
            if (m.Success) { tokens.Add($"name:*{m.Groups[1].Value}*"); continue; }

            m = NameEq.Match(cond);
            if (m.Success) { tokens.Add($"name:{m.Groups[1].Value}"); continue; }

            m = FolderCon.Match(cond);
            if (m.Success) { tokens.Add($"folder:{m.Groups[1].Value}"); continue; }

            // Unknown token — store as-is so the engine can at least log it
            tokens.Add($"raw:{cond}");
        }
        return string.Join(" ", tokens);
    }

    // ── Helpers ─────────────────────────────────────────────────────────────

    private static SortRulesEngine.SortRule MakeRule(string name, string spaceTitle, int priority,
        SortRulesEngine.RuleType type, string pattern) =>
        new()
        {
            Name             = name,
            TargetSpaceTitle = spaceTitle,
            Type             = type,
            Pattern          = pattern,
            Priority         = priority,
        };

    private static string? ExtractQuoted(string s)
    {
        int start = s.IndexOf('"');
        if (start < 0) return s.Trim();
        int end = s.IndexOf('"', start + 1);
        if (end < 0) return s.Substring(start + 1).Trim();
        return s.Substring(start + 1, end - start - 1);
    }

    private static List<string> ExtractListValues(string listContent)
    {
        var result = new List<string>();
        foreach (Match m in Regex.Matches(listContent, @"""([^""]+)"""))
            result.Add(m.Groups[1].Value);
        return result;
    }

    private static string NormalizeExt(string ext)
    {
        string trimmed = ext.Trim();
        return trimmed.StartsWith('.') ? trimmed.ToLowerInvariant() : "." + trimmed.ToLowerInvariant();
    }

    private static string EscapeQuotes(string s) => s.Replace("\"", "\\\"");

    private static string BuildCustomCondition(string pattern)
    {
        // Reverse of BuildCustomPattern for export
        var parts = new List<string>();
        foreach (var token in pattern.Split(' ', StringSplitOptions.RemoveEmptyEntries))
        {
            var kv = token.Split(':', 2);
            if (kv.Length != 2) continue;
            parts.Add(kv[0] switch
            {
                "ext"    => $"ext == \"{kv[1]}\"",
                "extOR"  => $"ext in [{string.Join(", ", kv[1].Split('|').Select(e => $"\"{e}\""))}]",
                "type"   => $"type == \"{kv[1]}\"",
                "name"   => $"name contains \"{kv[1].Trim('*')}\"",
                "folder" => $"folder contains \"{kv[1]}\"",
                _        => kv[1]
            });
        }
        return string.Join(" and ", parts);
    }
}
