using FluentAssertions;
using IVOEFences.Core.Services;
using Xunit;

namespace IVOEFences.Tests;

public class UndoServiceTests
{
    private readonly UndoService _service;

    public UndoServiceTests()
    {
        _service = UndoService.Instance;
        _service.Clear();
    }

    [Fact]
    public void Execute_SetsUndoAvailabilityAndName()
    {
        int value = 0;
        var cmd = new UndoService.PropertyChangeCommand<int>("Set value", () => value, v => value = v, 42);

        _service.Execute(cmd);

        value.Should().Be(42);
        _service.CanUndo.Should().BeTrue();
        _service.CanRedo.Should().BeFalse();
        _service.GetNextUndoName().Should().Be("Set value");
    }

    [Fact]
    public void Undo_ThenRedo_RestoresExpectedState()
    {
        int value = 0;
        var cmd = new UndoService.PropertyChangeCommand<int>("Set value", () => value, v => value = v, 7);

        _service.Execute(cmd);
        _service.Undo();

        value.Should().Be(0);
        _service.CanRedo.Should().BeTrue();
        _service.GetNextRedoName().Should().Be("Set value");

        _service.Redo();

        value.Should().Be(7);
        _service.CanUndo.Should().BeTrue();
    }

    [Fact]
    public void ExecuteAfterUndo_ClearsRedoStack()
    {
        int value = 0;
        var cmd1 = new UndoService.PropertyChangeCommand<int>("Set value 1", () => value, v => value = v, 1);
        var cmd2 = new UndoService.PropertyChangeCommand<int>("Set value 2", () => value, v => value = v, 2);

        _service.Execute(cmd1);
        _service.Undo();
        _service.CanRedo.Should().BeTrue();

        _service.Execute(cmd2);

        _service.CanRedo.Should().BeFalse();
        _service.GetNextRedoName().Should().BeNull();
    }

    [Fact]
    public void Execute_AboveMaxUndoStack_KeepsNewestCommandsOnly()
    {
        int value = 0;
        for (int i = 1; i <= 25; i++)
        {
            int capture = i;
            var cmd = new UndoService.PropertyChangeCommand<int>($"Set value {capture}", () => value, v => value = v, capture);
            _service.Execute(cmd);
        }

        for (int i = 0; i < 20; i++)
            _service.Undo();

        value.Should().Be(5);
        _service.CanUndo.Should().BeFalse();
        _service.CanRedo.Should().BeTrue();

        _service.Redo();
        value.Should().Be(6);
    }
}
