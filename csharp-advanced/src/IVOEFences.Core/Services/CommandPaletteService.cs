using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

public sealed class CommandPaletteService
{
    private static readonly Lazy<CommandPaletteService> _instance = new(() => new CommandPaletteService());
    public static CommandPaletteService Instance => _instance.Value;

    private readonly List<CommandPaletteEntry> _staticEntries = new();

    private CommandPaletteService()
    {
    }

    public void Register(CommandPaletteEntry entry)
    {
        _staticEntries.RemoveAll(e => e.Id == entry.Id);
        _staticEntries.Add(entry);
    }

    public IReadOnlyList<CommandPaletteEntry> Search(string query, int take = 20)
    {
        string q = query.Trim();
        var results = new List<CommandPaletteEntry>();

        if (AppSettingsRepository.Instance.Current.EnableFenceSearch)
        {
            foreach (var found in SearchService.Instance.Search(q))
            {
                results.Add(new CommandPaletteEntry
                {
                    Id = found.ItemId.ToString("N"),
                    Type = CommandPaletteEntryType.OpenItem,
                    Title = found.DisplayName,
                    Subtitle = found.FenceTitle,
                    Score = found.RelevanceScore,
                });
            }
        }

        foreach (var entry in _staticEntries)
        {
            if (entry.Title.Contains(q, StringComparison.OrdinalIgnoreCase) ||
                entry.Subtitle.Contains(q, StringComparison.OrdinalIgnoreCase))
            {
                results.Add(entry with { Score = Math.Max(entry.Score, 0.6) });
            }
        }

        return results
            .OrderByDescending(r => r.Score)
            .ThenBy(r => r.Title)
            .Take(take)
            .ToList();
    }
}
