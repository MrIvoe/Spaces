using FluentAssertions;
using IVOESpaces.Shell.Spaces;
using Xunit;

namespace IVOESpaces.Tests;

public class SpacePipelineOwnershipPolicyTests
{
    [Fact]
    public void UiMutationDispatcher_WhenOwnershipMissing_Throws()
    {
        var dispatcher = new SpaceUiMutationDispatcher();

        Action act = () => dispatcher.Enqueue(() => { }, ownershipName: " ");

        act.Should().Throw<ArgumentException>();
    }

    [Fact]
    public void DesktopChangeProcessor_WhenOwnershipMissing_Throws()
    {
        var dispatcher = new SpaceUiMutationDispatcher();
        var runtimeStore = new SpaceRuntimeStateStore();
        var desktopSync = new SpaceDesktopSyncCoordinator();
        var processor = new SpaceDesktopChangeProcessor(
            dispatcher,
            runtimeStore,
            desktopSync,
            snapshotWindows: () => new List<SpaceWindow>(),
            isReloadingFromState: () => false);

        Action act = () => processor.EnqueueCreated("C:\\temp\\ownership.txt", "ownership", ownershipName: "");

        act.Should().Throw<ArgumentException>();
    }
}