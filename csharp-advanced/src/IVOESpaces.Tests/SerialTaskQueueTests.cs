using FluentAssertions;
using IVOESpaces.Shell;
using Xunit;

namespace IVOESpaces.Tests;

public class SerialTaskQueueTests
{
    [Fact]
    public async Task Enqueue_WhenFirstOperationThrows_StillRunsNextOperation()
    {
        var queue = new SerialTaskQueue();
        var steps = new List<string>();

        Task first = queue.Enqueue(
            () =>
            {
                steps.Add("first");
                throw new InvalidOperationException("boom");
            },
            ownershipName: "test first");

        Task second = queue.Enqueue(
            () =>
            {
                steps.Add("second");
                return Task.CompletedTask;
            },
            ownershipName: "test second");

        Func<Task> awaitFirst = async () => await first;
        await awaitFirst.Should().ThrowAsync<InvalidOperationException>();
        await second;
        steps.Should().Equal("first", "second");
    }

    [Fact]
    public async Task EnqueueSafe_WhenFirstOperationThrows_StillRunsNextOperation()
    {
        var queue = new SerialTaskQueue();
        var steps = new List<string>();
        Exception? captured = null;

        Task first = queue.EnqueueSafe(
            () =>
            {
                steps.Add("first");
                throw new InvalidOperationException("boom");
            },
            ownershipName: "test safe first",
            ex => captured = ex);

        Task second = queue.EnqueueSafe(
            () =>
            {
                steps.Add("second");
                return Task.CompletedTask;
            },
            ownershipName: "test safe second",
            _ => { });

        await Task.WhenAll(first, second);

        steps.Should().Equal("first", "second");
        captured.Should().NotBeNull();
        captured.Should().BeOfType<InvalidOperationException>();
    }

    [Fact]
    public void Enqueue_WhenOwnershipNameMissing_Throws()
    {
        var queue = new SerialTaskQueue();

        Action act = () => queue.Enqueue(() => Task.CompletedTask, ownershipName: " ");

        act.Should().Throw<ArgumentException>();
    }
}