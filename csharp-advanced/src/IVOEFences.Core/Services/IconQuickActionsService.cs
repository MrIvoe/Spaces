using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

public sealed class IconQuickActionsService
{
    private static readonly Lazy<IconQuickActionsService> _instance = new(() => new IconQuickActionsService());
    public static IconQuickActionsService Instance => _instance.Value;

    private IconQuickActionsService()
    {
    }

    public List<QuickActionModel> BuildActions(FenceItemModel item, Action<string>? onTagSelected = null)
    {
        bool hasPath = !string.IsNullOrWhiteSpace(FenceItemResolver.Instance.GetPath(item));

        return new List<QuickActionModel>
        {
            new()
            {
                Type = QuickActionType.Open,
                Label = "Open",
                IsEnabled = hasPath
            },
            new()
            {
                Type = QuickActionType.OpenContainingFolder,
                Label = "Open Containing Folder",
                IsEnabled = hasPath
            },
            new()
            {
                Type = QuickActionType.PinToFence,
                Label = item.IsPinned ? "Unpin" : "Pin to Fence"
            },
            new()
            {
                Type = QuickActionType.Tag,
                Label = "Tag: Work",
                Execute = () => onTagSelected?.Invoke("work")
            },
            new()
            {
                Type = QuickActionType.SuggestMove,
                Label = "Suggest Better Fence"
            }
        };
    }
}
