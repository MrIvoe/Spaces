using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

public sealed record FenceSuggestion(Guid? FenceId, string Reason, double Confidence);

public sealed class SimpleSuggestionService
{
    public FenceSuggestion Suggest(FenceItemModel item, IReadOnlyList<FenceModel> fences)
    {
        SmartOrganizer.ProgramCategory category = SmartOrganizer.CategorizeProgram(item.DisplayName);

        string desiredFenceName = category switch
        {
            SmartOrganizer.ProgramCategory.Development => "Development",
            SmartOrganizer.ProgramCategory.Communication => "Communication",
            SmartOrganizer.ProgramCategory.Games => "Games",
            SmartOrganizer.ProgramCategory.GameLaunchers => "Games",
            SmartOrganizer.ProgramCategory.Office => "Office",
            _ => string.Empty,
        };

        if (string.IsNullOrWhiteSpace(desiredFenceName))
            return new FenceSuggestion(null, "No strong category match", 0.0);

        FenceModel? fence = fences.FirstOrDefault(f =>
            string.Equals(f.Title, desiredFenceName, StringComparison.OrdinalIgnoreCase));

        return fence == null
            ? new FenceSuggestion(null, $"Suggested category: {desiredFenceName}", 0.45)
            : new FenceSuggestion(fence.Id, $"Matches {desiredFenceName}", 0.75);
    }
}
