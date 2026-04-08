using IVOESpaces.Core.Plugins;

namespace IVOESpaces.Shell.Settings;

internal static class PluginSettingsBlueprintAdapter
{
    public static IReadOnlyList<SettingDefinition> Convert(IEnumerable<PluginSettingDefinition> definitions)
    {
        return definitions.Select(definition => new SettingDefinition(
            Key: $"plugin.{definition.PluginId}.{definition.Key}",
            Scope: definition.IsSpaceScoped ? SettingsScope.Space : SettingsScope.Global,
            Tab: definition.Tab,
            Section: definition.Section,
            Label: definition.Label,
            Tooltip: definition.Tooltip,
            Kind: definition.Kind switch
            {
                PluginSettingKind.Toggle => SettingValueKind.Toggle,
                PluginSettingKind.Choice => SettingValueKind.Choice,
                PluginSettingKind.Number => SettingValueKind.Number,
                PluginSettingKind.Color => SettingValueKind.Color,
                PluginSettingKind.Action => SettingValueKind.Action,
                _ => SettingValueKind.Text,
            },
            Choices: definition.Choices)).ToList();
    }
}