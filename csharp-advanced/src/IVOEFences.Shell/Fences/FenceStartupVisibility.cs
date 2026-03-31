using IVOEFences.Core.Models;
using Serilog;

namespace IVOEFences.Shell.Fences;

/// <summary>
/// Helper for fence startup visibility decision.
/// Prevents black-square flashing by validating fence state before showing.
/// 
/// Critical checks:
/// - Fence is not explicitly hidden
/// - Bounds fractions are reasonable (non-default)
/// 
/// Call early in fence window creation, before SetWindowPos shows the window.
/// Logs diagnostic info when a fence stays hidden.
/// </summary>
public static class FenceStartupVisibility
{
    /// <summary>
    /// Quick check: is this fence ready to be shown on startup?
    /// Returns false early if fence has obvious startup issues.
    /// </summary>
    public static bool IsReadyToShow(FenceModel fence)
    {
        if (fence == null)
            return false;

        // Don't show hidden fences
        if (fence.IsHidden)
        {
            Log.Debug("FenceStartupVisibility: hiding fence '{Title}' (IsHidden=true)", fence.Title);
            return false;
        }

        // Validate bounds are non-default
        if (!AreBoundsValid(fence))
        {
            Log.Debug("FenceStartupVisibility: hiding fence '{Title}' (bounds invalid)", fence.Title);
            return false;
        }

        return true;
    }

    /// <summary>
    /// Validates that fence bounds (as fractions) are reasonable (not default 0,0).
    /// </summary>
    private static bool AreBoundsValid(FenceModel fence)
    {
        const double MinFraction = 0.001; // Allow very small windows
        
        // If width or height are effectively zero, bounds are invalid
        return fence.WidthFraction >= MinFraction && fence.HeightFraction >= MinFraction;
    }
}
