namespace IVOEFences.Core.Models;

public enum QuickActionType
{
    Open,
    OpenContainingFolder,
    Tag,
    PinToFence,
    SuggestMove
}

public record QuickActionModel
{
    public QuickActionType Type { get; init; }
    public string Label { get; init; } = string.Empty;
    public bool IsEnabled { get; init; } = true;
    public Action? Execute { get; init; }
}
