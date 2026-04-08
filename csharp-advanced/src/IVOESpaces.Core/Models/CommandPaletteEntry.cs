namespace IVOESpaces.Core.Models;

public enum CommandPaletteEntryType
{
    OpenItem,
    MoveItem,
    SwitchProfile,
    Script,
    QuickAction
}

public record CommandPaletteEntry
{
    public string Id { get; init; } = Guid.NewGuid().ToString("N");
    public string Title { get; set; } = string.Empty;
    public string Subtitle { get; set; } = string.Empty;
    public CommandPaletteEntryType Type { get; set; }
    public double Score { get; set; }
    public Action? Execute { get; set; }
}
