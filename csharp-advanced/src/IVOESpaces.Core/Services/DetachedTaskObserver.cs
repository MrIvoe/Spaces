using Serilog;

namespace IVOESpaces.Core.Services;

public static class DetachedTaskObserver
{
    public static void Run(Task operation, Action<Exception> onError)
    {
        ArgumentNullException.ThrowIfNull(operation);
        ArgumentNullException.ThrowIfNull(onError);
        _ = ObserveAsync(operation, onError);
    }

    private static async Task ObserveAsync(Task operation, Action<Exception> onError)
    {
        try
        {
            await operation.ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            try
            {
                onError(ex);
            }
            catch (Exception callbackEx)
            {
                Log.Error(callbackEx, "DetachedTaskObserver: error callback threw while handling detached task failure");
                Log.Error(ex, "DetachedTaskObserver: original detached task failure whose callback threw");
            }
        }
    }
}