using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;

namespace IVOESpaces.Shell.Spaces;

internal sealed class IconHoverPreview
{
    public HoverPreviewData? Build(SpaceItemModel item)
    {
        return HoverPreviewService.Instance.BuildPreview(item);
    }

    public IReadOnlyList<QuickActionModel> BuildQuickActions(SpaceItemModel item, Action<string>? onTag = null)
    {
        return IconQuickActionsService.Instance.BuildActions(item, onTag);
    }
}
