using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

public sealed record HoverPreviewData
{
    public string Title { get; init; } = string.Empty;
    public string Subtitle { get; init; } = string.Empty;
    public string Kind { get; init; } = "file";
    public string? ThumbnailPath { get; init; }
}

public sealed class HoverPreviewService
{
    private static readonly Lazy<HoverPreviewService> _instance = new(() => new HoverPreviewService());
    public static HoverPreviewService Instance => _instance.Value;

    public event EventHandler<QuickActionInvokedEventArgs>? QuickActionInvoked;

    private HoverPreviewService()
    {
    }

    public HoverPreviewData? BuildPreview(FenceItemModel item)
    {
        if (!AppSettingsRepository.Instance.Current.EnableHoverPreviews)
            return null;

        item.LastPreviewedTime = DateTime.UtcNow;

        string path = FenceItemResolver.Instance.GetPath(item);
        string? ext = FenceItemResolver.Instance.GetExtension(item);
        bool isDir = FenceItemResolver.Instance.IsDirectory(item);
        
        string extensionStr = (ext ?? Path.GetExtension(path)).ToLowerInvariant();
        string kind = extensionStr switch
        {
            ".png" or ".jpg" or ".jpeg" or ".gif" or ".webp" => "image",
            ".mp4" or ".mkv" or ".avi" or ".mov" => "video",
            ".pdf" or ".doc" or ".docx" or ".txt" => "document",
            _ => isDir ? "folder" : "file"
        };

        return new HoverPreviewData
        {
            Title = FenceItemResolver.Instance.GetDisplayName(item),
            Subtitle = path,
            Kind = kind
        };
    }

    public IReadOnlyList<QuickActionModel> BuildQuickActions(FenceItemModel item, Action<string>? onTagSelected = null)
    {
        if (!AppSettingsRepository.Instance.Current.EnableQuickActions)
            return Array.Empty<QuickActionModel>();

        return IconQuickActionsService.Instance.BuildActions(item, onTagSelected);
    }

    public void InvokeQuickAction(FenceItemModel item, QuickActionModel action)
    {
        action.Execute?.Invoke();

        QuickActionInvoked?.Invoke(this, new QuickActionInvokedEventArgs
        {
            ItemId = item.Id,
            ActionType = action.Type,
            ActionLabel = action.Label,
            InvokedAtUtc = DateTime.UtcNow
        });
    }
}

public sealed class QuickActionInvokedEventArgs : EventArgs
{
    public Guid ItemId { get; init; }
    public QuickActionType ActionType { get; init; }
    public string ActionLabel { get; init; } = string.Empty;
    public DateTime InvokedAtUtc { get; init; }
}
