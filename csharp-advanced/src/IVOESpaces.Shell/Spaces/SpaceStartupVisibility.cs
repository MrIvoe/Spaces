using IVOESpaces.Core.Models;
using Serilog;

namespace IVOESpaces.Shell.Spaces;

/// <summary>
/// Helper for space startup visibility decision.
/// Prevents black-square flashing by validating space state before showing.
/// 
/// Critical checks:
/// - Space is not explicitly hidden
/// - Bounds fractions are reasonable (non-default)
/// 
/// Call early in space window creation, before SetWindowPos shows the window.
/// Logs diagnostic info when a space stays hidden.
/// </summary>
public static class SpaceStartupVisibility
{
    /// <summary>
    /// Quick check: is this space ready to be shown on startup?
    /// Returns false early if space has obvious startup issues.
    /// </summary>
    public static bool IsReadyToShow(SpaceModel space)
    {
        if (space == null)
            return false;

        // Don't show hidden spaces
        if (space.IsHidden)
        {
            Log.Debug("SpaceStartupVisibility: hiding space '{Title}' (IsHidden=true)", space.Title);
            return false;
        }

        // Validate bounds are non-default
        if (!AreBoundsValid(space))
        {
            Log.Debug("SpaceStartupVisibility: hiding space '{Title}' (bounds invalid)", space.Title);
            return false;
        }

        return true;
    }

    /// <summary>
    /// Validates that space bounds (as fractions) are reasonable (not default 0,0).
    /// </summary>
    private static bool AreBoundsValid(SpaceModel space)
    {
        const double MinFraction = 0.001; // Allow very small windows
        
        // If width or height are effectively zero, bounds are invalid
        return space.WidthFraction >= MinFraction && space.HeightFraction >= MinFraction;
    }
}
