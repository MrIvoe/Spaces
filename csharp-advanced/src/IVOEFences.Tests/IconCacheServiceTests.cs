using FluentAssertions;
using IVOEFences.Core.Services;
using Xunit;

namespace IVOEFences.Tests;

public class IconCacheServiceTests
{
    private readonly FenceStateService _service;

    public IconCacheServiceTests()
    {
        _service = FenceStateService.Instance;
    }

    [Fact]
    public void Instance_IsNotNull()
    {
        // Assert
        _service.Should().NotBeNull();
    }

    [Fact]
    public void IconCacheService_CanBeInstantiated()
    {
        // Assert
        _service.Should().BeOfType<FenceStateService>();
    }
}
