using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Singleton state service that keeps spaces in memory and debounce-persists to disk.
/// Replaces the pattern of LoadAll→modify→SaveAll with a central state store.
/// </summary>
public sealed class SpaceStateService : ISpaceStateService
{
    private static readonly Lazy<SpaceStateService> _instance = new(() => new SpaceStateService());
    public static SpaceStateService Instance => _instance.Value;

    private readonly SpaceRepository _repository;
    private readonly SemaphoreSlim _lock = new(1, 1);
    private readonly SemaphoreSlim _flushLock = new(1, 1);
    // Single persistent timer — Change() resets the countdown without
    // creating a new CancellationTokenSource and Task on every mutation.
    private readonly Timer _debounceTimer;
    private int _dirtyGeneration;
    private int _persistedGeneration;
    private List<SpaceModel> _spaces = new();
    private bool _initialized;

    private const int SaveDebounceMs = 500;

    public IReadOnlyList<SpaceModel> Spaces => _spaces.AsReadOnly();

    public event EventHandler? StateChanged;

    private SpaceStateService()
    {
        _repository    = SpaceRepository.Instance;
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
            _spaces = await _repository.LoadAllAsync().ConfigureAwait(false);
            
            // Migrate legacy space items to virtual ownership model
            SpaceEntityMigrationService.MigrateLoadedSpaces(_spaces);
            
            _initialized = true;
            Serilog.Log.Information("SpaceStateService initialized with {Count} spaces", _spaces.Count);
        }
        finally
        {
            _lock.Release();
        }
    }

    public SpaceModel? GetSpace(Guid id)
    {
        return _spaces.FirstOrDefault(f => f.Id == id);
    }

    public async Task AddSpaceAsync(SpaceModel space)
    {
        await _lock.WaitAsync().ConfigureAwait(false);
        try
        {
            _spaces.Add(space);
        }
        finally
        {
            _lock.Release();
        }
        OnStateChanged();
        ScheduleDebouncedSave();
    }

    public void AddSpaceNow(SpaceModel space)
    {
        _lock.Wait();
        try
        {
            _spaces.Add(space);
        }
        finally
        {
            _lock.Release();
        }
        OnStateChanged();
        ScheduleDebouncedSave();
    }

    public async Task UpdateSpaceAsync(SpaceModel space)
    {
        await _lock.WaitAsync().ConfigureAwait(false);
        try
        {
            var idx = _spaces.FindIndex(f => f.Id == space.Id);
            if (idx >= 0)
                _spaces[idx] = space;
            else
                _spaces.Add(space);
        }
        finally
        {
            _lock.Release();
        }
        OnStateChanged();
        ScheduleDebouncedSave();
    }

    public async Task RemoveSpaceAsync(Guid id)
    {
        await _lock.WaitAsync().ConfigureAwait(false);
        try
        {
            _spaces.RemoveAll(f => f.Id == id);
        }
        finally
        {
            _lock.Release();
        }
        OnStateChanged();
        ScheduleDebouncedSave();
    }

    public async Task ReplaceAllAsync(IEnumerable<SpaceModel> spaces)
    {
        await _lock.WaitAsync().ConfigureAwait(false);
        try
        {
            _spaces = spaces.ToList();
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
            Serilog.Log.Error(ex, "SpaceStateService: debounced save failed");
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
        List<SpaceModel> snapshot;
        await _lock.WaitAsync().ConfigureAwait(false);
        try
        {
            snapshot = _spaces.ToList();
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
