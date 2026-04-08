namespace IVOESpaces.Core.Plugins;

public enum PluginSettingKind
{
    Toggle,
    Choice,
    Number,
    Text,
    Color,
    Action,
}

public sealed record PluginSettingDefinition(
    string PluginId,
    string Key,
    string Tab,
    string Section,
    string Label,
    string Tooltip,
    PluginSettingKind Kind,
    string[]? Choices = null,
    string? DefaultValue = null,
    bool IsSpaceScoped = false);