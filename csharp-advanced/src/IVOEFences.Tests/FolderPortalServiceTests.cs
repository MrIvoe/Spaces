using FluentAssertions;
using IVOEFences.Core.Models;
using IVOEFences.Core.Services;
using System.Reflection;
using Xunit;

namespace IVOEFences.Tests;

public class FolderPortalServiceTests
{
    [Fact]
    public void FolderPortalService_Type_IsClass()
    {
        var serviceType = typeof(FolderPortalService);
        serviceType.IsClass.Should().BeTrue();
    }

    [Fact]
    public void StaleWatcherEvents_AreIgnored_AfterDetach()
    {
        string root = Path.Combine(Path.GetTempPath(), $"ivoe_portal_tests_{Guid.NewGuid():N}");
        Directory.CreateDirectory(root);

        var service = new FolderPortalService();
        var fence = new FenceModel
        {
            Id = Guid.NewGuid(),
            Title = "Portal",
            Type = FenceType.Portal,
            PortalFolderPath = root,
        };

        int eventCount = 0;
        service.PortalItemsChanged += (_, _) => Interlocked.Increment(ref eventCount);

        try
        {
            service.AttachWatcher(fence);
            FileSystemWatcher currentWatcher = GetCurrentWatcher(service, fence.Id);

            InvokeWatcherCallback(service, fence.Id, currentWatcher);
            eventCount.Should().Be(1, "active watcher should publish changes");

            service.DetachWatcher(fence.Id);

            InvokeWatcherCallback(service, fence.Id, currentWatcher);
            eventCount.Should().Be(1, "detached watcher callbacks must be ignored");
        }
        finally
        {
            service.DetachWatcher(fence.Id);
            Directory.Delete(root, recursive: true);
        }
    }

    [Fact]
    public void StaleWatcherEvents_AreIgnored_AfterNavigateReplace()
    {
        string root = Path.Combine(Path.GetTempPath(), $"ivoe_portal_tests_{Guid.NewGuid():N}");
        string child = Path.Combine(root, "child");
        Directory.CreateDirectory(child);

        var service = new FolderPortalService();
        var fence = new FenceModel
        {
            Id = Guid.NewGuid(),
            Title = "Portal",
            Type = FenceType.Portal,
            PortalFolderPath = root,
        };

        int eventCount = 0;
        service.PortalItemsChanged += (_, _) => Interlocked.Increment(ref eventCount);

        try
        {
            service.AttachWatcher(fence);
            FileSystemWatcher firstWatcher = GetCurrentWatcher(service, fence.Id);

            bool navigated = service.NavigateIntoFolder(fence.Id, child);
            navigated.Should().BeTrue();

            // Navigation emits one change notification.
            eventCount.Should().Be(1);

            FileSystemWatcher secondWatcher = GetCurrentWatcher(service, fence.Id);
            ReferenceEquals(firstWatcher, secondWatcher).Should().BeFalse();

            InvokeWatcherCallback(service, fence.Id, firstWatcher);
            eventCount.Should().Be(1, "callbacks from replaced watchers must be ignored");

            InvokeWatcherCallback(service, fence.Id, secondWatcher);
            eventCount.Should().Be(2, "active watcher should still publish changes");
        }
        finally
        {
            service.DetachWatcher(fence.Id);
            Directory.Delete(root, recursive: true);
        }
    }

    private static FileSystemWatcher GetCurrentWatcher(FolderPortalService service, Guid fenceId)
    {
        FieldInfo? field = typeof(FolderPortalService).GetField("_watchers", BindingFlags.NonPublic | BindingFlags.Instance);
        field.Should().NotBeNull();

        var map = field!.GetValue(service) as Dictionary<Guid, FileSystemWatcher>;
        map.Should().NotBeNull();
        map!.TryGetValue(fenceId, out FileSystemWatcher? watcher).Should().BeTrue();
        watcher.Should().NotBeNull();
        return watcher!;
    }

    private static void InvokeWatcherCallback(FolderPortalService service, Guid fenceId, FileSystemWatcher watcher)
    {
        MethodInfo? callback = typeof(FolderPortalService).GetMethod("OnWatcherFileChanged", BindingFlags.NonPublic | BindingFlags.Instance);
        callback.Should().NotBeNull();
        callback!.Invoke(service, new object[] { fenceId, watcher });
    }

}
