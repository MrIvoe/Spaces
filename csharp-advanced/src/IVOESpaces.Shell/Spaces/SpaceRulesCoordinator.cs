using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;

namespace IVOESpaces.Shell.Spaces;

internal sealed class SpaceRulesCoordinator
{
    public int ApplyAutoPlacementRules(IReadOnlyList<SpaceModel> models, IEnumerable<SpaceWindow> windows)
    {
        var app = AppSettingsRepository.Instance.Current;
        if (!app.EnableGlobalPlacementRules)
            return 0;

        int moved = 0;

        foreach (SpaceModel target in models)
        {
            List<string> include = target.SettingsOverrides.IncludeRules;
            if (include.Count == 0)
                continue;

            List<string> exclude = target.SettingsOverrides.ExcludeRules;

            foreach (SpaceModel source in models)
            {
                if (source.Id == target.Id)
                    continue;

                List<SpaceItemModel> matches = source.Items
                    .Where(item => MatchesRule(item, include, exclude))
                    .ToList();

                if (matches.Count == 0)
                    continue;

                foreach (SpaceItemModel item in matches)
                {
                    source.Items.RemoveAll(i => i.Id == item.Id);
                    target.Items.Add(item);
                    moved++;
                }
            }
        }

        if (moved > 0)
        {
            foreach (SpaceModel model in models)
            {
                for (int i = 0; i < model.Items.Count; i++)
                    model.Items[i].SortOrder = i;
            }

            foreach (SpaceWindow window in windows)
                window.InvalidateContent();

            SpaceStateService.Instance.MarkDirty();
        }

        return moved;
    }

    private static bool MatchesRule(SpaceItemModel item, List<string> include, List<string> exclude)
    {
        if (include.Count == 0)
            return false;

        string path = item.TargetPath ?? string.Empty;
        string name = item.DisplayName ?? string.Empty;
        string ext = Path.GetExtension(path);

        bool included = include.Any(rule => RuleTokenMatches(rule, path, name, ext));
        if (!included)
            return false;

        bool excluded = exclude.Any(rule => RuleTokenMatches(rule, path, name, ext));
        return !excluded;
    }

    private static bool RuleTokenMatches(string token, string path, string name, string ext)
    {
        if (string.IsNullOrWhiteSpace(token))
            return false;

        string t = token.Trim();
        if (string.Equals(t, "*", StringComparison.Ordinal))
            return true;

        if (t.StartsWith("prefix:", StringComparison.OrdinalIgnoreCase))
        {
            string prefix = t["prefix:".Length..].Trim();
            return !string.IsNullOrWhiteSpace(prefix)
                && name.StartsWith(prefix, StringComparison.OrdinalIgnoreCase);
        }

        if (t.StartsWith('.'))
            return string.Equals(ext, t, StringComparison.OrdinalIgnoreCase);

        return path.Contains(t, StringComparison.OrdinalIgnoreCase)
            || name.Contains(t, StringComparison.OrdinalIgnoreCase);
    }
}
