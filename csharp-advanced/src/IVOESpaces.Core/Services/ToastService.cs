using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Application = System.Windows.Application;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Floating toast notification service.
/// Queue system with max 4 visible toasts, auto-dismiss after duration.
/// </summary>
public sealed class ToastService
{
    private static readonly Lazy<ToastService> _instance = new(() => new ToastService());
    public static ToastService Instance => _instance.Value;

    private class ToastEntry
    {
        public Guid Id { get; } = Guid.NewGuid();
        public string Text { get; set; } = string.Empty;
        public DateTime CreatedAt { get; set; }
        public int DurationMs { get; set; }
        public Action? OnDismiss { get; set; }
    }

    private readonly LinkedList<ToastEntry> _stack = new();
    private const int MaxVisible = 4;

    public event EventHandler<ToastShowedEventArgs>? ToastShowed;
    public event EventHandler<ToastDismissedEventArgs>? ToastDismissed;

    private ToastService() { }

    /// <summary>
    /// Shows a toast notification.
    /// Auto-dismisses after durationMs.
    /// </summary>
    public Guid Show(string text, int durationMs = 3000, string? title = null, Action? onDismiss = null)
    {
        Guid toastId = Guid.Empty;

        Application.Current?.Dispatcher.Invoke(() =>
        {
            // Trim oldest toast if at capacity
            if (_stack.Count >= MaxVisible)
            {
                var oldest = _stack.First;
                if (oldest != null)
                {
                    _stack.RemoveFirst();
                    oldest.Value.OnDismiss?.Invoke();
                    ToastDismissed?.Invoke(this, new ToastDismissedEventArgs 
                    { 
                        ToastId = oldest.Value.Id,
                        Reason = DismissalReason.Overflow
                    });
                }
            }

            var entry = new ToastEntry
            {
                Text = text,
                CreatedAt = DateTime.UtcNow,
                DurationMs = durationMs,
                OnDismiss = onDismiss
            };

            toastId = entry.Id;
            _stack.AddLast(entry);

            ToastShowed?.Invoke(this, new ToastShowedEventArgs
            {
                ToastId = toastId,
                Title = title,
                Text = text,
                DurationMs = durationMs
            });

            Serilog.Log.Debug("Toast shown: {Text}", text);

            // Schedule dismissal
            _ = Task.Delay(durationMs).ContinueWith(_ => Dismiss(toastId, DismissalReason.Expired));
        });

        return toastId;
    }

    /// <summary>
    /// Dismisses a specific toast by ID.
    /// </summary>
    public void Dismiss(Guid toastId, DismissalReason reason = DismissalReason.UserClosed)
    {
        Application.Current?.Dispatcher.Invoke(() =>
        {
            var entry = _stack.FirstOrDefault(t => t.Id == toastId);
            if (entry != null)
            {
                _stack.Remove(entry);
                entry.OnDismiss?.Invoke();
                ToastDismissed?.Invoke(this, new ToastDismissedEventArgs
                {
                    ToastId = toastId,
                    Reason = reason
                });

                Serilog.Log.Debug("Toast dismissed: {ToastId}", toastId);
            }
        });
    }

    /// <summary>
    /// Dismisses all visible toasts.
    /// </summary>
    public void DismissAll()
    {
        Application.Current?.Dispatcher.Invoke(() =>
        {
            var allIds = _stack.Select(t => t.Id).ToList();
            foreach (var id in allIds)
            {
                Dismiss(id, DismissalReason.ClearedAll);
            }
        });
    }

    /// <summary>
    /// Gets all currently visible toasts.
    /// </summary>
    public IReadOnlyList<ToastInfo> GetVisibleToasts()
    {
        return _stack.Select(t => new ToastInfo
        {
            Id = t.Id,
            Text = t.Text,
            AgeMs = (int)(DateTime.UtcNow - t.CreatedAt).TotalMilliseconds,
            DurationMs = t.DurationMs
        }).ToList();
    }

    // ── DATA TYPES ──

    public class ToastInfo
    {
        public Guid Id { get; set; }
        public string Text { get; set; } = string.Empty;
        public int AgeMs { get; set; }
        public int DurationMs { get; set; }
        public int ProgressPercent => Math.Min(100, (AgeMs * 100) / DurationMs);
    }

    public enum DismissalReason
    {
        UserClosed,
        Expired,
        Overflow,
        ClearedAll
    }

    public class ToastShowedEventArgs : EventArgs
    {
        public Guid ToastId { get; set; }
        public string? Title { get; set; }
        public string Text { get; set; } = string.Empty;
        public int DurationMs { get; set; }
    }

    public class ToastDismissedEventArgs : EventArgs
    {
        public Guid ToastId { get; set; }
        public DismissalReason Reason { get; set; }
    }
}
