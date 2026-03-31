using FluentAssertions;
using IVOEFences.Core.Services;
using Xunit;

namespace IVOEFences.Tests;

public class ThemeEngineTests
{
    private readonly ThemeEngine _engine;

    public ThemeEngineTests()
    {
        _engine = ThemeEngine.Instance;
    }

    [Fact]
    public void Instance_IsNotNull()
    {
        // Assert
        _engine.Should().NotBeNull();
    }

    [Fact]
    public void ThemeEngine_CanBeInstantiated()
    {
        // Assert
        _engine.Should().BeOfType<ThemeEngine>();
    }

    [Fact]
    public void ThemeEngine_IsSingleton()
    {
        // Arrange
        var engine1 = ThemeEngine.Instance;
        var engine2 = ThemeEngine.Instance;

        // Act & Assert
        ReferenceEquals(engine1, engine2).Should().BeTrue();
    }
}
