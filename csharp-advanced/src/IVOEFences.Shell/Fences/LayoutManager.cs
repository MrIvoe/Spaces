using System.Text.Json;
using IVOEFences.Core;
using IVOEFences.Core.Models;
using IVOEFences.Core.Services;
using IVOEFences.Shell.Native;

namespace IVOEFences.Shell.Fences;

internal sealed class LayoutManager
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true,
        PropertyNameCaseInsensitive = true,
    };

    private static string LayoutFilePath => Path.Combine(AppPaths.DataRoot, "layout.json");

    public void SaveLayout(IEnumerable<FenceWindow> fences)
    {
        var snapshot = new LayoutSnapshot
        {
            SavedAtUtc = DateTime.UtcNow,
            Fences = fences.Select(ToFenceLayout).ToList()
        };

        Directory.CreateDirectory(Path.GetDirectoryName(LayoutFilePath)!);
        string json = JsonSerializer.Serialize(snapshot, JsonOptions);
        File.WriteAllText(LayoutFilePath, json);
    }

    public bool TryRestoreLayout(IReadOnlyList<FenceModel> fences, Win32.RECT workArea)
    {
        if (!File.Exists(LayoutFilePath))
            return false;

        LayoutSnapshot? snapshot;
        try
        {
            string json = File.ReadAllText(LayoutFilePath);
            snapshot = JsonSerializer.Deserialize<LayoutSnapshot>(json, JsonOptions);
        }
        catch
        {
            return false;
        }

        if (snapshot == null || snapshot.Fences.Count == 0)
            return false;

        var byId = fences.ToDictionary(f => f.Id, f => f);
        bool anyApplied = false;

        foreach (FenceLayout data in snapshot.Fences)
        {
            FenceModel? model = null;

            if (data.Id != Guid.Empty && byId.TryGetValue(data.Id, out FenceModel? idModel))
                model = idModel;

            if (model == null)
                model = fences.FirstOrDefault(f => string.Equals(f.Title, data.Name, StringComparison.OrdinalIgnoreCase));

            if (model == null)
                continue;

            ApplyBounds(model, data.X, data.Y, data.Width, data.Height, workArea);
            model.IsRolledUp = data.IsCollapsed;

            if (data.Icons.Count > 0)
            {
                var order = new Dictionary<Guid, int>();
                var usage = new Dictionary<Guid, (int ClickCount, DateTime? LastAccessUtc)>();
                foreach (IconLayout icon in data.Icons)
                {
                    if (icon.ItemId != Guid.Empty)
                    {
                        order[icon.ItemId] = icon.SortOrder;
                        usage[icon.ItemId] = (icon.ClickCount, icon.LastAccessUtc);
                    }
                }

                model.Items = model.Items
                    .OrderBy(i => order.TryGetValue(i.Id, out int pos) ? pos : int.MaxValue)
                    .ThenBy(i => i.SortOrder)
                    .ToList();

                for (int i = 0; i < model.Items.Count; i++)
                {
                    model.Items[i].SortOrder = i;

                    if (usage.TryGetValue(model.Items[i].Id, out var u))
                    {
                        model.Items[i].OpenCount = Math.Max(model.Items[i].OpenCount, u.ClickCount);
                        if (u.LastAccessUtc.HasValue)
                            model.Items[i].LastOpenedTime = u.LastAccessUtc;
                    }
                }
            }

            anyApplied = true;
        }

        if (anyApplied)
            FenceStateService.Instance.MarkDirty();

        return anyApplied;
    }

    private static FenceLayout ToFenceLayout(FenceWindow window)
    {
        FenceModel? model = FenceStateService.Instance.GetFence(window.ModelId);
        var icons = new List<IconLayout>();

        if (model != null)
        {
            icons = model.Items
                .OrderBy(i => i.SortOrder)
                .Select(i => new IconLayout
                {
                    ItemId = i.Id,
                    Name = i.DisplayName,
                    SortOrder = i.SortOrder,
                    ClickCount = i.OpenCount,
                    LastAccessUtc = i.LastOpenedTime
                })
                .ToList();
        }

        return new FenceLayout
        {
            Id = window.ModelId,
            Name = window.Title,
            X = window.X,
            Y = window.Y,
            Width = window.Width,
            Height = window.Height,
            IsCollapsed = window.IsCollapsed,
            Icons = icons
        };
    }

    private static void ApplyBounds(FenceModel model, int x, int y, int width, int height, Win32.RECT wa)
    {
        int waW = Math.Max(wa.right - wa.left, 1);
        int waH = Math.Max(wa.bottom - wa.top, 1);

        model.XFraction = (double)(x - wa.left) / waW;
        model.YFraction = (double)(y - wa.top) / waH;
        model.WidthFraction = (double)Math.Max(width, 80) / waW;
        model.HeightFraction = (double)Math.Max(height, 60) / waH;
    }
}

internal sealed class LayoutSnapshot
{
    public DateTime SavedAtUtc { get; set; }
    public List<FenceLayout> Fences { get; set; } = new();
}

internal sealed class FenceLayout
{
    public Guid Id { get; set; }
    public string Name { get; set; } = string.Empty;
    public int X { get; set; }
    public int Y { get; set; }
    public int Width { get; set; }
    public int Height { get; set; }
    public bool IsCollapsed { get; set; }
    public List<IconLayout> Icons { get; set; } = new();
}

internal sealed class IconLayout
{
    public Guid ItemId { get; set; }
    public string Name { get; set; } = string.Empty;
    public int SortOrder { get; set; }
    public int ClickCount { get; set; }
    public DateTime? LastAccessUtc { get; set; }
}
