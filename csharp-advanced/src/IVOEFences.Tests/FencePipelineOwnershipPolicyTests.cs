using FluentAssertions;
using IVOEFences.Shell.Fences;
using Xunit;

namespace IVOEFences.Tests;

public class FencePipelineOwnershipPolicyTests
{
    [Fact]
    public void UiMutationDispatcher_WhenOwnershipMissing_Throws()
    {
        var dispatcher = new FenceUiMutationDispatcher();

        Action act = () => dispatcher.Enqueue(() => { }, ownershipName: " ");

        act.Should().Throw<ArgumentException>();
    }

    [Fact]
    public void DesktopChangeProcessor_WhenOwnershipMissing_Throws()
    {
        var dispatcher = new FenceUiMutationDispatcher();
        var runtimeStore = new FenceRuntimeStateStore();
        var desktopSync = new FenceDesktopSyncCoordinator();
        var processor = new FenceDesktopChangeProcessor(
            dispatcher,
            runtimeStore,
            desktopSync,
            snapshotWindows: () => new List<FenceWindow>(),
            isReloadingFromState: () => false);

        Action act = () => processor.EnqueueCreated("C:\\temp\\ownership.txt", "ownership", ownershipName: "");

        act.Should().Throw<ArgumentException>();
    }
}