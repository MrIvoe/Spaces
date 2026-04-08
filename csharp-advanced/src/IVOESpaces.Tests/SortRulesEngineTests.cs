using FluentAssertions;
using IVOESpaces.Core.Services;
using Xunit;

namespace IVOESpaces.Tests;

public class SortRulesEngineTests
{
    private readonly SortRulesEngine _engine;

    public SortRulesEngineTests()
    {
        _engine = SortRulesEngine.Instance;
    }

    [Fact]
    public void Instance_IsNotNull()
    {
        // Assert
        _engine.Should().NotBeNull();
    }

    [Fact]
    public void SortRulesEngine_CanBeInstantiated()
    {
        // Assert
        _engine.Should().BeOfType<SortRulesEngine>();
    }

    [Fact]
    public void SortRulesEngine_IsSingleton()
    {
        // Arrange
        var engine1 = SortRulesEngine.Instance;
        var engine2 = SortRulesEngine.Instance;

        // Act & Assert
        ReferenceEquals(engine1, engine2).Should().BeTrue();
    }
}
