using IVOESpaces.Core.Services;
using Serilog;

namespace IVOESpaces.Shell.Spaces;

internal sealed class HoverPreviewManager
{
    private readonly Dictionary<Guid, string> _currentPreviewText = new();

    public void AttachPreview(SpaceWindow space, Func<string>? getPreviewText = null)
    {
        space.HoverPreviewChanged += preview =>
        {
            string text = getPreviewText?.Invoke() ?? $"{preview.Title}\n{preview.Subtitle}";
            _currentPreviewText[space.ModelId] = text;
            Log.Debug("HoverPreview: {Space} => {Preview}", space.ModelId, text);
        };

        space.HoverPreviewHidden += () =>
        {
            _currentPreviewText.Remove(space.ModelId);
            Log.Debug("HoverPreview: hidden for space {Space}", space.ModelId);
        };
    }

    public string? GetCurrentPreview(Guid spaceId)
    {
        return _currentPreviewText.TryGetValue(spaceId, out string? text) ? text : null;
    }
}
