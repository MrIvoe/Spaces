using FluentAssertions;
using IVOEFences.Core.Models;
using IVOEFences.Shell;
using IVOEFences.Shell.Desktop;
using IVOEFences.Shell.Fences;
using IVOEFences.Shell.Native;
using Xunit;

namespace IVOEFences.Tests;

public class MonitorLayoutCoordinatorTests
{
    [Fact]
    public async Task DisplayChange_WithSavedSnapshot_RestoresStateThenReloadsInOrder()
    {
        var steps = new List<string>();
        var restored = new List<FenceModel>
        {
            new()
            {
                Id = Guid.NewGuid(),
                Title = "Restored",
                WidthFraction = 0.2,
                HeightFraction = 0.2,
            },
        };

        var coordinator = new MonitorLayoutCoordinator(
            isReloadingFromState: () => false,
            reloadFromStateAsync: () =>
            {
                steps.Add("reload");
                return Task.CompletedTask;
            },
            captureCurrentSnapshot: () =>
            {
                steps.Add("capture");
                return new List<FenceModel> { new() { Id = Guid.NewGuid(), Title = "Current", WidthFraction = 0.2, HeightFraction = 0.2 } };
            },
            replaceAllAsync: fences =>
            {
                steps.Add($"replace:{fences.Count()}");
                return Task.CompletedTask;
            },
            saveLayoutSnapshotAsync: (configHash, fencesToSave) =>
            {
                steps.Add($"save:{configHash}:{fencesToSave.Count}");
                return Task.CompletedTask;
            },
            loadLayoutSnapshotAsync: configHash =>
            {
                steps.Add($"load:{configHash}");
                return Task.FromResult(restored);
            },
            getSettings: () => new AppSettings
            {
                DetectMonitorConfigurationChanges = true,
                AutoSwapMisplacedFenceContents = true,
            },
            topologyProvider: (out List<string> monitors, out string configHash) =>
            {
                monitors = new List<string> { "DISPLAY-2" };
                configHash = "new-hash";
                return true;
            });

        coordinator.LastMonitorConfigHashForTesting = "old-hash";

        await coordinator.HandleDisplayChangeForTestingAsync();

        steps.Should().Equal("capture", "save:old-hash:1", "load:new-hash", "replace:1", "reload");
        coordinator.LastMonitorConfigHashForTesting.Should().Be("new-hash");
    }

    [Fact]
    public async Task OverlappingDisplayChanges_RunSingleRestoreSequence()
    {
        var loadGate = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        var loadStarted = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        int saveCalls = 0;
        int loadCalls = 0;
        int reloadCalls = 0;

        var coordinator = new MonitorLayoutCoordinator(
            isReloadingFromState: () => false,
            reloadFromStateAsync: () =>
            {
                Interlocked.Increment(ref reloadCalls);
                return Task.CompletedTask;
            },
            captureCurrentSnapshot: () => new List<FenceModel> { new() { Id = Guid.NewGuid(), WidthFraction = 0.2, HeightFraction = 0.2 } },
            replaceAllAsync: _ => Task.CompletedTask,
            saveLayoutSnapshotAsync: (_, _) =>
            {
                Interlocked.Increment(ref saveCalls);
                return Task.CompletedTask;
            },
            loadLayoutSnapshotAsync: async _ =>
            {
                Interlocked.Increment(ref loadCalls);
                loadStarted.TrySetResult();
                await loadGate.Task.ConfigureAwait(false);
                return new List<FenceModel>
                {
                    new() { Id = Guid.NewGuid(), WidthFraction = 0.25, HeightFraction = 0.25 },
                };
            },
            getSettings: () => new AppSettings
            {
                DetectMonitorConfigurationChanges = true,
                AutoSwapMisplacedFenceContents = true,
            },
            topologyProvider: (out List<string> monitors, out string configHash) =>
            {
                monitors = new List<string> { "DISPLAY-RESTORE" };
                configHash = "restore-hash";
                return true;
            });

        coordinator.LastMonitorConfigHashForTesting = "seed-hash";

        coordinator.HandleDisplayChange();
        coordinator.HandleDisplayChange();

        await loadStarted.Task;
        loadCalls.Should().Be(1);

        loadGate.SetResult();
        await coordinator.AwaitPendingDisplayChangeTaskAsync();

        saveCalls.Should().Be(1);
        loadCalls.Should().Be(1);
        reloadCalls.Should().Be(1);
    }

    [Fact]
    public async Task RestoredSnapshot_WithMissingMonitor_UsesFallbackPlacementDuringReload()
    {
        List<FenceModel>? replaced = null;
        bool usedFallback = false;
        var planner = new FenceRestorePlacementPlanner((string _, out Win32.RECT workArea) =>
        {
            workArea = default;
            return false;
        });

        var coordinator = new MonitorLayoutCoordinator(
            isReloadingFromState: () => false,
            reloadFromStateAsync: () =>
            {
                FenceModel restoredFence = replaced!.Single();
                FenceRestorePlacementPlanner.PlacementPlan plan = planner.Plan(
                    restoredFence,
                    new Win32.RECT { left = 0, top = 0, right = 1200, bottom = 900 });
                usedFallback = plan.UsedFallbackWorkArea;
                return Task.CompletedTask;
            },
            captureCurrentSnapshot: () => new List<FenceModel> { new() { Id = Guid.NewGuid(), WidthFraction = 0.2, HeightFraction = 0.2 } },
            replaceAllAsync: fences =>
            {
                replaced = fences.ToList();
                return Task.CompletedTask;
            },
            saveLayoutSnapshotAsync: (_, _) => Task.CompletedTask,
            loadLayoutSnapshotAsync: _ => Task.FromResult(new List<FenceModel>
            {
                new()
                {
                    Id = Guid.NewGuid(),
                    Title = "Mismatch",
                    MonitorDeviceName = "MISSING-MONITOR",
                    XFraction = 0.95,
                    YFraction = 0.9,
                    WidthFraction = 0.4,
                    HeightFraction = 0.4,
                },
            }),
            getSettings: () => new AppSettings
            {
                DetectMonitorConfigurationChanges = true,
                AutoSwapMisplacedFenceContents = true,
            },
            topologyProvider: (out List<string> monitors, out string configHash) =>
            {
                monitors = new List<string> { "DISPLAY-MISSING" };
                configHash = "topology-mismatch";
                return true;
            });

        coordinator.LastMonitorConfigHashForTesting = "saved-layout";

        await coordinator.HandleDisplayChangeForTestingAsync();

        usedFallback.Should().BeTrue();
        replaced.Should().NotBeNull();
        replaced!.Single().MonitorDeviceName.Should().Be("MISSING-MONITOR");
    }

    [Fact]
    public async Task ExplorerRestart_DoesNotBlockRestore_WhenSavedTopologyDiffers()
    {
        DesktopHost host = DesktopHost.Instance;
        Func<IntPtr> originalWorkerProvider = host.WorkerWProvider;
        Func<IntPtr> originalListViewProvider = host.DesktopListViewProvider;
        Action originalInvalidateProvider = host.InvalidateWorkerWProvider;
        Func<IEnumerable<Action<IntPtr>>>? originalTargets = host.ReAnchorTargets;
        int reloadCalls = 0;

        try
        {
            host.WorkerWProvider = () => new IntPtr(0x4000);
            host.DesktopListViewProvider = () => new IntPtr(0x5000);
            host.InvalidateWorkerWProvider = () => { };
            host.ReAnchorTargets = () => Array.Empty<Action<IntPtr>>();

            var coordinator = new MonitorLayoutCoordinator(
                isReloadingFromState: () => false,
                reloadFromStateAsync: () =>
                {
                    Interlocked.Increment(ref reloadCalls);
                    return Task.CompletedTask;
                },
                captureCurrentSnapshot: () => new List<FenceModel> { new() { Id = Guid.NewGuid(), WidthFraction = 0.2, HeightFraction = 0.2 } },
                replaceAllAsync: _ => Task.CompletedTask,
                saveLayoutSnapshotAsync: (_, _) => Task.CompletedTask,
                loadLayoutSnapshotAsync: _ => Task.FromResult(new List<FenceModel>
                {
                    new() { Id = Guid.NewGuid(), WidthFraction = 0.2, HeightFraction = 0.2 },
                }),
                getSettings: () => new AppSettings
                {
                    DetectMonitorConfigurationChanges = true,
                    AutoSwapMisplacedFenceContents = true,
                },
                topologyProvider: (out List<string> monitors, out string configHash) =>
                {
                    monitors = new List<string> { "DISPLAY-RESTART" };
                    configHash = "restart-mismatch";
                    return true;
                });

            coordinator.LastMonitorConfigHashForTesting = "old-topology";

            host.OnExplorerRestarted();
            await coordinator.HandleDisplayChangeForTestingAsync();

            reloadCalls.Should().Be(1);
            coordinator.LastMonitorConfigHashForTesting.Should().Be("restart-mismatch");
        }
        finally
        {
            host.WorkerWProvider = originalWorkerProvider;
            host.DesktopListViewProvider = originalListViewProvider;
            host.InvalidateWorkerWProvider = originalInvalidateProvider;
            host.ReAnchorTargets = originalTargets;
        }
    }

    [Fact]
    public async Task DisplayChange_WithUnchangedTopology_DoesNotPersistOrReload()
    {
        int saveCalls = 0;
        int loadCalls = 0;
        int reloadCalls = 0;

        var coordinator = new MonitorLayoutCoordinator(
            isReloadingFromState: () => false,
            reloadFromStateAsync: () =>
            {
                Interlocked.Increment(ref reloadCalls);
                return Task.CompletedTask;
            },
            captureCurrentSnapshot: () => new List<FenceModel>(),
            replaceAllAsync: _ => Task.CompletedTask,
            saveLayoutSnapshotAsync: (_, _) =>
            {
                Interlocked.Increment(ref saveCalls);
                return Task.CompletedTask;
            },
            loadLayoutSnapshotAsync: _ =>
            {
                Interlocked.Increment(ref loadCalls);
                return Task.FromResult(new List<FenceModel>());
            },
            getSettings: () => new AppSettings
            {
                DetectMonitorConfigurationChanges = true,
                AutoSwapMisplacedFenceContents = true,
            },
            topologyProvider: (out List<string> monitors, out string configHash) =>
            {
                monitors = new List<string> { "DISPLAY-STABLE" };
                configHash = "stable-hash";
                return true;
            });

        coordinator.LastMonitorConfigHashForTesting = "stable-hash";

        await coordinator.HandleDisplayChangeForTestingAsync();

        saveCalls.Should().Be(0);
        loadCalls.Should().Be(0);
        reloadCalls.Should().Be(0);
    }

    [Fact]
    public async Task DisplayChange_WithSnapshotLoadFailure_DoesNotThrowOrReload()
    {
        int reloadCalls = 0;

        var coordinator = new MonitorLayoutCoordinator(
            isReloadingFromState: () => false,
            reloadFromStateAsync: () =>
            {
                Interlocked.Increment(ref reloadCalls);
                return Task.CompletedTask;
            },
            captureCurrentSnapshot: () => new List<FenceModel>(),
            replaceAllAsync: _ => Task.CompletedTask,
            saveLayoutSnapshotAsync: (_, _) => Task.CompletedTask,
            loadLayoutSnapshotAsync: _ => throw new InvalidDataException("corrupt snapshot payload"),
            getSettings: () => new AppSettings
            {
                DetectMonitorConfigurationChanges = true,
                AutoSwapMisplacedFenceContents = true,
            },
            topologyProvider: (out List<string> monitors, out string configHash) =>
            {
                monitors = new List<string> { "DISPLAY-CORRUPT" };
                configHash = "corrupt-hash";
                return true;
            });

        coordinator.LastMonitorConfigHashForTesting = "old-hash";

        Func<Task> act = async () => await coordinator.HandleDisplayChangeForTestingAsync();
        await act.Should().NotThrowAsync();
        reloadCalls.Should().Be(0);
        coordinator.LastMonitorConfigHashForTesting.Should().Be("corrupt-hash");
    }
}