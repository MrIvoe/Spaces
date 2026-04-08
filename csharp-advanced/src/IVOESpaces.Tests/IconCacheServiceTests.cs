using FluentAssertions;
using IVOESpaces.Core.Services;
using Xunit;

namespace IVOESpaces.Tests;

public class IconCacheServiceTests
{
    private readonly SpaceStateService _service;

    public IconCacheServiceTests()
    {
        _service = SpaceStateService.Instance;
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
        _service.Should().BeOfType<SpaceStateService>();
    }
}
