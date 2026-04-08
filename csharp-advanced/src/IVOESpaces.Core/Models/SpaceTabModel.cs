using System;

namespace IVOESpaces.Core.Models;

/// <summary>
/// Step 35: Represents a tab container that can hold multiple spaces as tabs.
/// Users can merge spaces together into a tabbed interface.
/// </summary>
public sealed record SpaceTabModel
{
    /// <summary>
    /// Unique identifier for this tab container.
    /// </summary>
    public Guid Id { get; init; } = Guid.NewGuid();

    /// <summary>
    /// Index of the currently active (visible) tab (0-based).
    /// </summary>
    public int ActiveTabIndex { get; set; } = 0;

    /// <summary>
    /// Visual style for tab rendering (e.g., "Rounded", "Sharp", "Minimal").
    /// Default: "Rounded"
    /// </summary>
    public string TabStyle { get; set; } = "Rounded";

    /// <summary>
    /// Horizontal position as fraction of monitor work area (0.0 - 1.0).
    /// </summary>
    public double XFraction { get; set; }

    /// <summary>
    /// Vertical position as fraction of monitor work area (0.0 - 1.0).
    /// </summary>
    public double YFraction { get; set; }

    /// <summary>
    /// Width as fraction of monitor work area width (default: 0.18 = 18%).
    /// </summary>
    public double WidthFraction { get; set; } = 0.18;

    /// <summary>
    /// Height as fraction of monitor work area height (default: 0.22 = 22%).
    /// </summary>
    public double HeightFraction { get; set; } = 0.22;

    /// <summary>
    /// Device name of the monitor where this container is located.
    /// </summary>
    public string MonitorDeviceName { get; set; } = string.Empty;

    /// <summary>
    /// Desktop page index (0-based) for virtual desktops/pages.
    /// </summary>
    public int PageIndex { get; set; } = 0;

    /// <summary>
    /// Creation timestamp (UTC).
    /// </summary>
    public DateTime CreatedAt { get; init; } = DateTime.UtcNow;

    /// <summary>
    /// Last modification timestamp (UTC).
    /// </summary>
    public DateTime LastModifiedAt { get; set; } = DateTime.UtcNow;
}
