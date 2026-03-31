using System;
using System.Collections.Generic;
using System.Linq;

namespace IVOEFences.Core.Services;

/// <summary>
/// Undo/Redo engine with 20-item stack.
/// Supports any reversible operation via IUndoCommand.
/// </summary>
public sealed class UndoService
{
    private static readonly Lazy<UndoService> _instance = new(() => new UndoService());
    public static UndoService Instance => _instance.Value;

    private const int MaxUndoStack = 20;

    private readonly Stack<IUndoCommand> _undoStack = new();
    private readonly Stack<IUndoCommand> _redoStack = new();

    public event EventHandler<UndoRedoChangedEventArgs>? UndoRedoChanged;

    private UndoService() { }

    /// <summary>
    /// Executes a command and adds it to the undo stack.
    /// Clears redo stack automatically (new action taken).
    /// </summary>
    public void Execute(IUndoCommand command)
    {
        try
        {
            command.Execute();
            
            // Add to undo stack
            _undoStack.Push(command);
            TrimUndoStackToLimit();

            // Clear redo stack
            _redoStack.Clear();

            OnUndoRedoChanged();
            Serilog.Log.Debug("Executed command: {CommandName}", command.Name);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to execute command: {CommandName}", command.Name);
            throw;
        }
    }

    /// <summary>
    /// Undoes the most recent command.
    /// </summary>
    public void Undo()
    {
        if (_undoStack.Count == 0)
            return;

        try
        {
            IUndoCommand command = _undoStack.Pop();
            command.Undo();
            _redoStack.Push(command);

            OnUndoRedoChanged();
            Serilog.Log.Debug("Undo: {CommandName}", command.Name);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to undo");
        }
    }

    /// <summary>
    /// Redoes the most recently undone command.
    /// </summary>
    public void Redo()
    {
        if (_redoStack.Count == 0)
            return;

        try
        {
            IUndoCommand command = _redoStack.Pop();
            command.Execute();
            _undoStack.Push(command);
            TrimUndoStackToLimit();

            OnUndoRedoChanged();
            Serilog.Log.Debug("Redo: {CommandName}", command.Name);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to redo");
        }
    }

    /// <summary>
    /// Gets the name of the next undoable action.
    /// </summary>
    public string? GetNextUndoName()
    {
        return _undoStack.Count > 0 ? _undoStack.Peek().Name : null;
    }

    /// <summary>
    /// Gets the name of the next redoable action.
    /// </summary>
    public string? GetNextRedoName()
    {
        return _redoStack.Count > 0 ? _redoStack.Peek().Name : null;
    }

    /// <summary>
    /// Checks if undo is available.
    /// </summary>
    public bool CanUndo => _undoStack.Count > 0;

    /// <summary>
    /// Checks if redo is available.
    /// </summary>
    public bool CanRedo => _redoStack.Count > 0;

    /// <summary>
    /// Clears all undo/redo history.
    /// </summary>
    public void Clear()
    {
        _undoStack.Clear();
        _redoStack.Clear();
        OnUndoRedoChanged();
    }

    private void OnUndoRedoChanged()
    {
        UndoRedoChanged?.Invoke(this, new UndoRedoChangedEventArgs
        {
            CanUndo = CanUndo,
            CanRedo = CanRedo,
            UndoName = GetNextUndoName(),
            RedoName = GetNextRedoName()
        });
    }

    private void TrimUndoStackToLimit()
    {
        if (_undoStack.Count <= MaxUndoStack)
            return;

        // Keep the newest MaxUndoStack entries (top of stack = newest).
        IUndoCommand[] newestFirst = _undoStack.Take(MaxUndoStack).ToArray();
        _undoStack.Clear();
        for (int i = newestFirst.Length - 1; i >= 0; i--)
            _undoStack.Push(newestFirst[i]);
    }

    // ── COMMAND INTERFACE & EVENT ARGS ──

    /// <summary>
    /// Interface for undoable/redoable commands.
    /// </summary>
    public interface IUndoCommand
    {
        string Name { get; }
        void Execute();
        void Undo();
    }

    /// <summary>
    /// Generic command implementation for simple property changes.
    /// </summary>
    public class PropertyChangeCommand<T> : IUndoCommand
    {
        private readonly Func<T> _getter;
        private readonly Action<T> _setter;
        private readonly T _newValue;
        private readonly T _oldValue;

        public string Name { get; }

        public PropertyChangeCommand(string name, Func<T> getter, Action<T> setter, T newValue)
        {
            Name = name;
            _getter = getter;
            _setter = setter;
            _newValue = newValue;
            _oldValue = getter();
        }

        public void Execute() => _setter(_newValue);
        public void Undo() => _setter(_oldValue);
    }

    /// <summary>
    /// Command for adding/removing items from collections.
    /// </summary>
    public class CollectionChangeCommand<T> : IUndoCommand
    {
        private readonly List<T> _collection;
        private readonly T _item;
        private readonly int _index;
        private readonly bool _isAdd;

        public string Name { get; }

        public CollectionChangeCommand(string name, List<T> collection, T item, bool isAdd, int index = -1)
        {
            Name = name;
            _collection = collection;
            _item = item;
            _isAdd = isAdd;
            _index = index >= 0 ? index : collection.Count;
        }

        public void Execute()
        {
            if (_isAdd)
                _collection.Insert(_index, _item);
            else
                _collection.Remove(_item);
        }

        public void Undo()
        {
            if (_isAdd)
                _collection.Remove(_item);
            else
                _collection.Insert(_index, _item);
        }
    }

    public class UndoRedoChangedEventArgs : EventArgs
    {
        public bool CanUndo { get; set; }
        public bool CanRedo { get; set; }
        public string? UndoName { get; set; }
        public string? RedoName { get; set; }
    }
}

// Legacy interface for compatibility
public interface IUndoAction
{
    string Description { get; }
    void Execute();
    void Undo();
    void Redo();
}
