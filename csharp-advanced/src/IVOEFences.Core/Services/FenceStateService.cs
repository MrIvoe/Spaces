using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Singleton state service that keeps fences in memory and debounce-persists to disk.
/// Replaces the pattern of LoadAll→modify→SaveAll with a central state store.
/// </summary>
public sealed class FenceStateService : IFenceStateService
{
    private static readonly Lazy<FenceStateService> _instance = new(() => new FenceStateService());
    public static FenceStateService Instance => _instance.Value;

    private readonly FenceRepository _repository;
    private readonly SemaphoreSlim _lock = new(1, 1);
    private readonly SemaphoreSlim _flushLock = new(1, 1);
    // Single persistent timer — Change() resets the countdown without
    // creating a new CancellationTokenSource and Task on every mutation.
    private readonly Timer _debounceTimer;
    private int _dirtyGeneration;
    private int _persistedGeneration;
    private List<FenceModel> _fences = new();
    private bool _initialized;

    private const int SaveDebounceMs = 500;

    public IReadOnlyList<FenceModel> Fences => _fences.AsReadOnly();

    public event EventHandler? StateChanged;

    private FenceStateService()
    {
        _repository    = FenceRepository.Instance;
        _debounceTimer = new Timer(OnDebounceElapsed, null,
                                   Timeout.Infinite, Timeout.Infinite);
    }

    public async Task InitializeAsync()
    {
        if (_initialized) return;
        await _lock.WaitAsync().ConfigureAwait(false);
        try
        {
            if (_initialized) return;
            _fences = await _repository.LoadAllAsync().ConfigureAwait(false);
            
            // Migrate legacy fence items to virtual ownership model
            FenceEntityMigrationService.MigrateLoadedFences(_fences);
            
            _initialized = true;
            Serilog.Log.Information("FenceStateService initialized with {Count} fences", _fences.Count);
        }
        finally
        {
            _lock.Release();
        }
    }

    public FenceModel? GetFence(Guid id)
    {
        return _fences.FirstOrDefault(f => f.Id == id);
    }

    public async Task AddFenceAsync(FenceModel fence)
    {
        await _lock.WaitAsync().ConfigureAwait(false);
        try
        {
            _fences.Add(fence);
        }
        finally
        {
            _lock.Release();
        }
        OnStateChanged();
        ScheduleDebouncedSave();
    }

    public void AddFenceNow(FenceModel fence)
    {
        _lock.Wait();
        try
        {
            _fences.Add(fence);
        }
        finally
        {
            _lock.Release();
        }
        OnStateChanged();
        ScheduleDebouncedSave();
    }

    public async Task UpdateFenceAsync(FenceModel fence)
    {
        await _lock.WaitAsync().ConfigureAwait(false);
        try
        {
            var idx = _fences.FindIndex(f => f.Id == fence.Id);
            if (idx >= 0)
                _fences[idx] = fence;
            else
                _fences.Add(fence);
        }
        finally
        {
            _lock.Release();
        }
        OnStateChanged();
        ScheduleDebouncedSave();
    }

    public async Task RemoveFenceAsync(Guid id)
    {
        await _lock.WaitAsync().ConfigureAwait(false);
        try
        {
            _fences.RemoveAll(f => f.Id == id);
        }
        finally
        {
            _lock.Release();
        }
        OnStateChanged();
        ScheduleDebouncedSave();
    }

    public async Task ReplaceAllAsync(IEnumerable<FenceModel> fences)
    {
        await _lock.WaitAsync().ConfigureAwait(false);
        try
        {
            _fences = fences.ToList();
        }
        finally
        {
            _lock.Release();
        }
        OnStateChanged();
        ScheduleDebouncedSave();
    }

    public async Task SaveAsync()
    {
        // Cancel any pending debounced save
        _debounceTimer.Change(Timeout.Infinite, Timeout.Infinite);
        int targetGeneration = Volatile.Read(ref _dirtyGeneration);
        await FlushUntilStableAsync(targetGeneration).ConfigureAwait(false);
    }

    public void MarkDirty()
    {
        OnStateChanged();
        ScheduleDebouncedSave();
    }

    private void ScheduleDebouncedSave()
    {
        Interlocked.Increment(ref _dirtyGeneration);
        // Resetting the timer is thread-safe and allocation-free
        _debounceTimer.Change(SaveDebounceMs, Timeout.Infinite);
    }

    private void OnDebounceElapsed(object? state)
    {
        _ = FlushDebouncedSaveSafelyAsync();
    }

    private async Task FlushDebouncedSaveSafelyAsync()
    {
        try
        {
            int targetGeneration = Volatile.Read(ref _dirtyGeneration);
            await FlushUntilStableAsync(targetGeneration).ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "FenceStateService: debounced save failed");
        }
    }

    private async Task FlushUntilStableAsync(int targetGeneration)
    {
        await _flushLock.WaitAsync().ConfigureAwait(false);
        try
        {
            while (Volatile.Read(ref _persistedGeneration) < targetGeneration)
            {
                await PersistNowAsync().ConfigureAwait(false);
                int currentGeneration = Volatile.Read(ref _dirtyGeneration);
                Volatile.Write(ref _persistedGeneration, currentGeneration);
                targetGeneration = currentGeneration;
            }
        }
        finally
        {
            _flushLock.Release();
        }
    }

    private async Task PersistNowAsync()
    {
        List<FenceModel> snapshot;
        await _lock.WaitAsync().ConfigureAwait(false);
        try
        {
            snapshot = _fences.ToList();
        }
        finally
        {
            _lock.Release();
        }
        await _repository.SaveAllAsync(snapshot).ConfigureAwait(false);
    }

    private void OnStateChanged()
    {
        StateChanged?.Invoke(this, EventArgs.Empty);
    }
}
