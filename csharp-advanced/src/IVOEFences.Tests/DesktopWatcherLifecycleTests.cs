using FluentAssertions;
using IVOEFences.Core.Services;
using Xunit;

namespace IVOEFences.Tests;

public class DesktopWatcherLifecycleTests
{
    [Fact]
    public async Task WatcherError_RestartsCurrentWatchers_WithFreshInstances()
    {
        using var harness = new WatcherHarness(delayAsync: _ => Task.CompletedTask);
        harness.Service.Start();

        FileSystemWatcher originalUserWatcher = harness.GetUserWatcher();
        FileSystemWatcher originalCommonWatcher = harness.GetCommonWatcher();
        int generation = harness.Service.WatcherGenerationForTesting;

        await harness.Service.RestartAfterErrorForTestingAsync(originalUserWatcher, generation, new IOException("boom"));

        harness.GetUserWatcher().Should().NotBeSameAs(originalUserWatcher);
        harness.GetCommonWatcher().Should().NotBeSameAs(originalCommonWatcher);
        harness.Service.WatcherGenerationForTesting.Should().Be(generation + 2);
        harness.Service.RestartScheduledForTesting.Should().Be(0);
    }

    [Fact]
    public async Task WatcherError_WithStaleGeneration_DoesNotRestart()
    {
        using var harness = new WatcherHarness(delayAsync: _ => Task.CompletedTask);
        harness.Service.Start();

        FileSystemWatcher originalUserWatcher = harness.GetUserWatcher();
        int generation = harness.Service.WatcherGenerationForTesting;

        await harness.Service.RestartAfterErrorForTestingAsync(originalUserWatcher, generation - 1, new IOException("stale"));

        harness.GetUserWatcher().Should().BeSameAs(originalUserWatcher);
        harness.Service.RestartAttemptsForTesting.Should().Be(0);
    }

    [Fact]
    public async Task ConcurrentWatcherErrors_CollapseToSingleRestartSchedule()
    {
        var delayGate = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        using var harness = new WatcherHarness(delayAsync: _ => delayGate.Task);
        harness.Service.Start();

        FileSystemWatcher originalUserWatcher = harness.GetUserWatcher();
        int generation = harness.Service.WatcherGenerationForTesting;

        Task first = harness.Service.RestartAfterErrorForTestingAsync(originalUserWatcher, generation, new IOException("first"));
        Task second = harness.Service.RestartAfterErrorForTestingAsync(originalUserWatcher, generation, new IOException("second"));

        await Task.Yield();
        harness.Service.RestartScheduledForTesting.Should().Be(1);

        delayGate.SetResult();
        await Task.WhenAll(first, second);

        harness.Service.WatcherGenerationForTesting.Should().Be(generation + 2);
        harness.Service.RestartScheduledForTesting.Should().Be(0);
    }

    private sealed class WatcherHarness : IDisposable
    {
        private readonly string _root;

        public WatcherHarness(Func<TimeSpan, Task> delayAsync)
        {
            _root = Path.Combine(Path.GetTempPath(), $"ivoe_desktop_watchers_{Guid.NewGuid():N}");
            string userDesktop = Path.Combine(_root, "desktop");
            string commonDesktop = Path.Combine(_root, "common-desktop");
            Directory.CreateDirectory(userDesktop);
            Directory.CreateDirectory(commonDesktop);
            Service = DesktopWatcherService.CreateForTesting(userDesktop, commonDesktop, delayAsync);
        }

        public DesktopWatcherService Service { get; }

        public FileSystemWatcher GetUserWatcher()
        {
            return Service.UserWatcherForTesting
                ?? throw new InvalidOperationException("User watcher was null");
        }

        public FileSystemWatcher GetCommonWatcher()
        {
            return Service.CommonWatcherForTesting
                ?? throw new InvalidOperationException("Common watcher was null");
        }

        public void Dispose()
        {
            Service.Dispose();
            if (Directory.Exists(_root))
                Directory.Delete(_root, recursive: true);
        }
    }
}
