using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

public sealed record SpaceSuggestion(Guid? SpaceId, string Reason, double Confidence);

public sealed class SimpleSuggestionService
{
    public SpaceSuggestion Suggest(SpaceItemModel item, IReadOnlyList<SpaceModel> spaces)
    {
        SmartOrganizer.ProgramCategory category = SmartOrganizer.CategorizeProgram(item.DisplayName);

        string desiredSpaceName = category switch
        {
            SmartOrganizer.ProgramCategory.Development => "Development",
            SmartOrganizer.ProgramCategory.Communication => "Communication",
            SmartOrganizer.ProgramCategory.Games => "Games",
            SmartOrganizer.ProgramCategory.GameLaunchers => "Games",
            SmartOrganizer.ProgramCategory.Office => "Office",
            _ => string.Empty,
        };

        if (string.IsNullOrWhiteSpace(desiredSpaceName))
            return new SpaceSuggestion(null, "No strong category match", 0.0);

        SpaceModel? space = spaces.FirstOrDefault(f =>
            string.Equals(f.Title, desiredSpaceName, StringComparison.OrdinalIgnoreCase));

        return space == null
            ? new SpaceSuggestion(null, $"Suggested category: {desiredSpaceName}", 0.45)
            : new SpaceSuggestion(space.Id, $"Matches {desiredSpaceName}", 0.75);
    }
}
