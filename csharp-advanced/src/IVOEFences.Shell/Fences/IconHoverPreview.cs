using IVOEFences.Core.Models;
using IVOEFences.Core.Services;

namespace IVOEFences.Shell.Fences;

internal sealed class IconHoverPreview
{
    public HoverPreviewData? Build(FenceItemModel item)
    {
        return HoverPreviewService.Instance.BuildPreview(item);
    }

    public IReadOnlyList<QuickActionModel> BuildQuickActions(FenceItemModel item, Action<string>? onTag = null)
    {
        return IconQuickActionsService.Instance.BuildActions(item, onTag);
    }
}
