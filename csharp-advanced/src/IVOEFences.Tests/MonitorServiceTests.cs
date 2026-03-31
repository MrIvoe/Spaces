using FluentAssertions;
using IVOEFences.Core.Services;
using Xunit;

namespace IVOEFences.Tests;

public class MonitorServiceTests
{
    private readonly ThemeEngine _service;

    public MonitorServiceTests()
    {
        _service = ThemeEngine.Instance;
    }

    [Fact]
    public void Instance_IsNotNull()
    {
        // Assert
        _service.Should().NotBeNull();
    }

    [Fact]
    public void MonitorService_CanBeInstantiated()
    {
        // Assert
        _service.Should().BeOfType<ThemeEngine>();
    }
}
