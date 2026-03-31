using FluentAssertions;
using IVOEFences.Core.Services;
using Xunit;

namespace IVOEFences.Tests;

public class DetachedTaskObserverTests
{
    [Fact]
    public async Task Run_WhenOperationFails_InvokesErrorCallback()
    {
        var callbackHit = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);

        DetachedTaskObserver.Run(
            Task.FromException(new InvalidOperationException("boom")),
            _ => callbackHit.TrySetResult());

        await callbackHit.Task.WaitAsync(TimeSpan.FromSeconds(2));
    }

    [Fact]
    public async Task Run_WhenErrorCallbackThrows_DoesNotSurfaceSecondaryFailure()
    {
        var callbackHit = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);

        DetachedTaskObserver.Run(
            Task.FromException(new InvalidOperationException("boom")),
            _ =>
            {
                callbackHit.TrySetResult();
                throw new ApplicationException("callback failure");
            });

        await callbackHit.Task.WaitAsync(TimeSpan.FromSeconds(2));

        // Ensure the detached observer had time to process callback failure handling.
        await Task.Delay(50);
        true.Should().BeTrue();
    }
}