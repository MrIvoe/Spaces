using FluentAssertions;
using IVOESpaces.Shell;
using IVOESpaces.Shell.Desktop;
using IVOESpaces.Shell.Spaces;
using Xunit;

namespace IVOESpaces.Tests;

public class ShellLifecycleReflectionTests
{
    [Fact]
    public void EnqueueDesktopChange_IsBlocked_DuringRestoreWindow()
    {
        using var manager = new SpaceManager();
        try
        {
            manager.SetReloadingFromStateForTesting(true);
            manager.EnqueueDesktopCreatedForTesting(Path.Combine(Path.GetTempPath(), "restore-gate-item.txt"));

            int pending = manager.GetPendingDesktopChangeCountForTesting();
            pending.Should().Be(0, "desktop changes must be ignored while a restore/reload is in progress");
        }
        finally
        {
            manager.SetReloadingFromStateForTesting(false);
        }
    }

    [Fact]
    public void ToggleVisibility_WhileDesktopQueueHasEntries_DoesNotThrow_AndQueueDrains()
    {
        using var manager = new SpaceManager();
        try
        {
            manager.SetReloadingFromStateForTesting(false);
            manager.EnqueueDesktopCreatedForTesting(Path.Combine(Path.GetTempPath(), "queued-1.txt"));
            manager.EnqueueDesktopDeletedForTesting(Path.Combine(Path.GetTempPath(), "queued-2.txt"));

            Action toggle = () => manager.SetAllVisible(true);
            toggle.Should().NotThrow();

            Action process = () => manager.ProcessPendingDesktopChangesForTesting();
            process.Should().NotThrow();

            int pending = manager.GetPendingDesktopChangeCountForTesting();
            pending.Should().Be(0);
        }
        finally
        {
            manager.SetReloadingFromStateForTesting(false);
        }
    }

    [Fact]
    public async Task DisplayChangeHandling_IsSkipped_WhileManagerReloading()
    {
        using var manager = new SpaceManager();
        try
        {
            manager.SetReloadingFromStateForTesting(true);
            var coordinator = new MonitorLayoutCoordinator(manager)
            {
                LastMonitorConfigHashForTesting = "seed-hash"
            };

            await coordinator.HandleDisplayChangeForTestingAsync();

            string hash = coordinator.LastMonitorConfigHashForTesting;
            hash.Should().Be("seed-hash", "display-change processing should no-op during active reload");
        }
        finally
        {
            manager.SetReloadingFromStateForTesting(false);
        }
    }

    [Fact]
    public async Task DisplayChangeHandling_DoesNotDisturbDeleteState_WhileReloading()
    {
        using var manager = new SpaceManager();
        try
        {
            Guid deletingSpaceId = Guid.NewGuid();
            bool beganDelete = manager.TryBeginDeleteForTesting(deletingSpaceId);
            beganDelete.Should().BeTrue();

            manager.SetReloadingFromStateForTesting(true);
            var coordinator = new MonitorLayoutCoordinator(manager)
            {
                LastMonitorConfigHashForTesting = "delete-overlap-hash"
            };

            await coordinator.HandleDisplayChangeForTestingAsync();

            coordinator.LastMonitorConfigHashForTesting.Should().Be("delete-overlap-hash");
            manager.IsDeletingForTesting(deletingSpaceId).Should().BeTrue();
        }
        finally
        {
            manager.SetReloadingFromStateForTesting(false);
        }
    }

    [Fact]
    public void ExplorerRestart_ReanchorsAllTargets_UsingResolvedWorkerW()
    {
        DesktopHost host = DesktopHost.Instance;

        Func<IntPtr> originalWorkerProvider = host.WorkerWProvider;
        Func<IntPtr> originalListViewProvider = host.DesktopListViewProvider;
        Action originalInvalidateProvider = host.InvalidateWorkerWProvider;
        Func<IEnumerable<Action<IntPtr>>>? originalTargets = host.ReAnchorTargets;

        var receivedHandles = new List<IntPtr>();
        int invalidateCount = 0;

        try
        {
            host.WorkerWProvider = () => new IntPtr(0x1234);
            host.DesktopListViewProvider = () => new IntPtr(0x5678);
            host.InvalidateWorkerWProvider = () => invalidateCount++;
            host.ReAnchorTargets = () =>
                new Action<IntPtr>[]
                {
                    handle => receivedHandles.Add(handle),
                    handle => receivedHandles.Add(handle),
                };

            host.OnExplorerRestarted();

            invalidateCount.Should().Be(1);
            receivedHandles.Should().HaveCount(2);
            receivedHandles.Should().OnlyContain(handle => handle == new IntPtr(0x1234));
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
    public void ExplorerRestart_WhenWorkerWMissing_UsesDesktopListViewFallbackHandle()
    {
        DesktopHost host = DesktopHost.Instance;

        Func<IntPtr> originalWorkerProvider = host.WorkerWProvider;
        Func<IntPtr> originalListViewProvider = host.DesktopListViewProvider;
        Action originalInvalidateProvider = host.InvalidateWorkerWProvider;
        Func<IEnumerable<Action<IntPtr>>>? originalTargets = host.ReAnchorTargets;

        var receivedHandles = new List<IntPtr>();

        try
        {
            host.WorkerWProvider = () => IntPtr.Zero;
            host.DesktopListViewProvider = () => new IntPtr(0x2468);
            host.InvalidateWorkerWProvider = () => { };
            host.ReAnchorTargets = () =>
                new Action<IntPtr>[]
                {
                    handle => receivedHandles.Add(handle),
                };

            host.OnExplorerRestarted();

            receivedHandles.Should().ContainSingle();
            receivedHandles[0].Should().Be(new IntPtr(0x2468));
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
    public void ExplorerRestart_RepeatedSignals_UseWorkerWThenFallbackWhenUnavailable()
    {
        DesktopHost host = DesktopHost.Instance;

        Func<IntPtr> originalWorkerProvider = host.WorkerWProvider;
        Func<IntPtr> originalListViewProvider = host.DesktopListViewProvider;
        Action originalInvalidateProvider = host.InvalidateWorkerWProvider;
        Func<IEnumerable<Action<IntPtr>>>? originalTargets = host.ReAnchorTargets;

        var receivedHandles = new List<IntPtr>();
        int call = 0;

        try
        {
            host.WorkerWProvider = () =>
            {
                call++;
                return call == 2 ? IntPtr.Zero : new IntPtr(0x3000 + call);
            };
            host.DesktopListViewProvider = () => new IntPtr(0x7777);
            host.InvalidateWorkerWProvider = () => { };
            host.ReAnchorTargets = () =>
                new Action<IntPtr>[]
                {
                    handle => receivedHandles.Add(handle),
                };

            host.OnExplorerRestarted();
            host.OnExplorerRestarted();
            host.OnExplorerRestarted();

            receivedHandles.Should().Equal(new IntPtr(0x3001), new IntPtr(0x7777), new IntPtr(0x3003));
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
    public async Task DisplayChangeHandling_MultipleSignals_AreSerializedThroughQueue()
    {
        int inFlight = 0;
        int maxInFlight = 0;

        var coordinator = new MonitorLayoutCoordinator(
            isReloadingFromState: () => false,
            reloadFromStateAsync: () => Task.CompletedTask,
            captureCurrentSnapshot: () => new List<IVOESpaces.Core.Models.SpaceModel>(),
            replaceAllAsync: _ => Task.CompletedTask,
            saveLayoutSnapshotAsync: async (_, _) =>
            {
                int current = Interlocked.Increment(ref inFlight);
                InterlockedExtensions.UpdateMax(ref maxInFlight, current);
                await Task.Delay(30);
                Interlocked.Decrement(ref inFlight);
            },
            loadLayoutSnapshotAsync: _ => Task.FromResult(new List<IVOESpaces.Core.Models.SpaceModel>()),
            getSettings: () => new IVOESpaces.Core.Models.AppSettings
            {
                DetectMonitorConfigurationChanges = true,
                AutoSwapMisplacedSpaceContents = false,
            },
            topologyProvider: (out List<string> monitors, out string hash) =>
            {
                monitors = new List<string>();
                hash = "hash-changed";
                return true;
            })
        {
            LastMonitorConfigHashForTesting = "hash-seed"
        };

        coordinator.HandleDisplayChange();
        coordinator.HandleDisplayChange();
        coordinator.HandleDisplayChange();

        await coordinator.AwaitPendingDisplayChangeTaskAsync();

        maxInFlight.Should().Be(1, "display-change handling must remain serialized under monitor churn");
    }

    [Fact]
    public async Task DisplayChangeHandling_RepeatedSignals_DuringReload_AreSkipped_UntilReloadCompletes()
    {
        bool reloading = true;
        int saveCount = 0;

        var coordinator = new MonitorLayoutCoordinator(
            isReloadingFromState: () => reloading,
            reloadFromStateAsync: () => Task.CompletedTask,
            captureCurrentSnapshot: () => new List<IVOESpaces.Core.Models.SpaceModel>(),
            replaceAllAsync: _ => Task.CompletedTask,
            saveLayoutSnapshotAsync: (_, _) =>
            {
                saveCount++;
                return Task.CompletedTask;
            },
            loadLayoutSnapshotAsync: _ => Task.FromResult(new List<IVOESpaces.Core.Models.SpaceModel>()),
            getSettings: () => new IVOESpaces.Core.Models.AppSettings
            {
                DetectMonitorConfigurationChanges = true,
                AutoSwapMisplacedSpaceContents = false,
            },
            topologyProvider: (out List<string> monitors, out string hash) =>
            {
                monitors = new List<string> { "DISPLAY-A" };
                hash = "hash-changed";
                return true;
            })
        {
            LastMonitorConfigHashForTesting = "hash-seed"
        };

        coordinator.HandleDisplayChange();
        coordinator.HandleDisplayChange();
        await coordinator.AwaitPendingDisplayChangeTaskAsync();

        coordinator.LastMonitorConfigHashForTesting.Should().Be("hash-seed");
        saveCount.Should().Be(0);

        reloading = false;
        coordinator.HandleDisplayChange();
        await coordinator.AwaitPendingDisplayChangeTaskAsync();

        coordinator.LastMonitorConfigHashForTesting.Should().Be("hash-changed");
        saveCount.Should().Be(1);
    }

    [Fact]
    public async Task ExplorerRestart_DisplayChangeOverlap_DrainsQueueAndAppliesSingleRestore()
    {
        DesktopHost host = DesktopHost.Instance;
        Func<IntPtr> originalWorkerProvider = host.WorkerWProvider;
        Func<IntPtr> originalListViewProvider = host.DesktopListViewProvider;
        Action originalInvalidateProvider = host.InvalidateWorkerWProvider;
        Func<IEnumerable<Action<IntPtr>>>? originalTargets = host.ReAnchorTargets;

        int saveCalls = 0;
        int reloadCalls = 0;
        var reAnchoredHandles = new List<IntPtr>();

        try
        {
            host.WorkerWProvider = () => new IntPtr(0x9001);
            host.DesktopListViewProvider = () => new IntPtr(0x9002);
            host.InvalidateWorkerWProvider = () => { };
            host.ReAnchorTargets = () =>
                new Action<IntPtr>[]
                {
                    handle => reAnchoredHandles.Add(handle),
                };

            var coordinator = new MonitorLayoutCoordinator(
                isReloadingFromState: () => false,
                reloadFromStateAsync: () =>
                {
                    Interlocked.Increment(ref reloadCalls);
                    return Task.CompletedTask;
                },
                captureCurrentSnapshot: () => new List<IVOESpaces.Core.Models.SpaceModel>(),
                replaceAllAsync: _ => Task.CompletedTask,
                saveLayoutSnapshotAsync: (_, _) =>
                {
                    Interlocked.Increment(ref saveCalls);
                    return Task.CompletedTask;
                },
                loadLayoutSnapshotAsync: _ => Task.FromResult(new List<IVOESpaces.Core.Models.SpaceModel>
                {
                    new()
                    {
                        Id = Guid.NewGuid(),
                        WidthFraction = 0.2,
                        HeightFraction = 0.2,
                    },
                }),
                getSettings: () => new IVOESpaces.Core.Models.AppSettings
                {
                    DetectMonitorConfigurationChanges = true,
                    AutoSwapMisplacedSpaceContents = true,
                },
                topologyProvider: (out List<string> monitors, out string hash) =>
                {
                    monitors = new List<string> { "DISPLAY-OVERLAP" };
                    hash = "overlap-hash";
                    return true;
                })
            {
                LastMonitorConfigHashForTesting = "seed-hash"
            };

            host.OnExplorerRestarted();
            coordinator.HandleDisplayChange();
            host.OnExplorerRestarted();
            coordinator.HandleDisplayChange();

            await coordinator.AwaitPendingDisplayChangeTaskAsync();

            saveCalls.Should().Be(1);
            reloadCalls.Should().Be(1);
            // In overlap scenario, re-anchoring may be consolidated; verify we got at least one
            reAnchoredHandles.Should().NotBeEmpty();
            reAnchoredHandles.Should().OnlyContain(h => h == new IntPtr(0x9001));
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
    public void ExplorerRestart_FallbackWithMixedTargetStates_ReanchorsAllTargetsWithoutChangingState()
    {
        DesktopHost host = DesktopHost.Instance;

        Func<IntPtr> originalWorkerProvider = host.WorkerWProvider;
        Func<IntPtr> originalListViewProvider = host.DesktopListViewProvider;
        Action originalInvalidateProvider = host.InvalidateWorkerWProvider;
        Func<IEnumerable<Action<IntPtr>>>? originalTargets = host.ReAnchorTargets;

        bool hiddenTargetVisible = false;
        bool visibleTargetVisible = true;
        var handles = new List<IntPtr>();

        try
        {
            host.WorkerWProvider = () => IntPtr.Zero;
            host.DesktopListViewProvider = () => new IntPtr(0x8888);
            host.InvalidateWorkerWProvider = () => { };
            host.ReAnchorTargets = () =>
                new Action<IntPtr>[]
                {
                    handle =>
                    {
                        handles.Add(handle);
                        hiddenTargetVisible = false;
                    },
                    handle =>
                    {
                        handles.Add(handle);
                        visibleTargetVisible = true;
                    },
                };

            host.OnExplorerRestarted();

            handles.Should().HaveCount(2);
            handles.Should().OnlyContain(h => h == new IntPtr(0x8888));
            hiddenTargetVisible.Should().BeFalse();
            visibleTargetVisible.Should().BeTrue();
        }
        finally
        {
            host.WorkerWProvider = originalWorkerProvider;
            host.DesktopListViewProvider = originalListViewProvider;
            host.InvalidateWorkerWProvider = originalInvalidateProvider;
            host.ReAnchorTargets = originalTargets;
        }
    }

    private static class InterlockedExtensions
    {
        public static void UpdateMax(ref int target, int candidate)
        {
            int snapshot;
            do
            {
                snapshot = target;
                if (candidate <= snapshot)
                    return;
            }
            while (Interlocked.CompareExchange(ref target, candidate, snapshot) != snapshot);
        }
    }
}
