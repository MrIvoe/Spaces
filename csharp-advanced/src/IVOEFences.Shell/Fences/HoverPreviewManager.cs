using IVOEFences.Core.Services;
using Serilog;

namespace IVOEFences.Shell.Fences;

internal sealed class HoverPreviewManager
{
    private readonly Dictionary<Guid, string> _currentPreviewText = new();

    public void AttachPreview(FenceWindow fence, Func<string>? getPreviewText = null)
    {
        fence.HoverPreviewChanged += preview =>
        {
            string text = getPreviewText?.Invoke() ?? $"{preview.Title}\n{preview.Subtitle}";
            _currentPreviewText[fence.ModelId] = text;
            Log.Debug("HoverPreview: {Fence} => {Preview}", fence.ModelId, text);
        };

        fence.HoverPreviewHidden += () =>
        {
            _currentPreviewText.Remove(fence.ModelId);
            Log.Debug("HoverPreview: hidden for fence {Fence}", fence.ModelId);
        };
    }

    public string? GetCurrentPreview(Guid fenceId)
    {
        return _currentPreviewText.TryGetValue(fenceId, out string? text) ? text : null;
    }
}
