using FluentAssertions;
using Xunit;

namespace IVOEFences.Tests;

public class HotkeyServiceTests
{
    private static bool TryParse(string hotkey, out uint modifiers, out uint virtualKey)
    {
        var shellAssembly = AppDomain.CurrentDomain.GetAssemblies()
            .FirstOrDefault(a => string.Equals(a.GetName().Name, "IVOEFences", StringComparison.Ordinal))
            ?? AppDomain.CurrentDomain.Load("IVOEFences");

        var type = shellAssembly.GetType("IVOEFences.Shell.HotkeyCoordinator")!;
        var method = type.GetMethod("TryParseHotkey")!;
        object?[] args = [hotkey, 0u, 0u];
        bool ok = (bool)method.Invoke(null, args)!;
        modifiers = (uint)args[1]!;
        virtualKey = (uint)args[2]!;
        return ok;
    }

    [Fact]
    public void TryParseHotkey_ParsesCtrlWinLetter()
    {
        bool ok = TryParse("Ctrl+Win+F", out uint modifiers, out uint virtualKey);

        ok.Should().BeTrue();
        modifiers.Should().Be(0x2u | 0x8u);
        virtualKey.Should().Be((uint)'F');
    }

    [Fact]
    public void TryParseHotkey_ParsesFunctionKeyWithShiftAlt()
    {
        bool ok = TryParse("Shift+Alt+F12", out uint modifiers, out uint virtualKey);

        ok.Should().BeTrue();
        modifiers.Should().Be(0x4u | 0x1u);
        virtualKey.Should().Be(0x7Bu);
    }

    [Fact]
    public void TryParseHotkey_FailsWhenPrimaryKeyIsMissing()
    {
        bool ok = TryParse("Ctrl+Alt", out _, out _);

        ok.Should().BeFalse();
    }

    [Fact]
    public void TryParseHotkey_FailsWhenMultiplePrimaryKeysProvided()
    {
        bool ok = TryParse("Ctrl+K+L", out _, out _);

        ok.Should().BeFalse();
    }

    [Fact]
    public void TryParseHotkey_FailsForUnknownToken()
    {
        bool ok = TryParse("Ctrl+Banana", out _, out _);

        ok.Should().BeFalse();
    }
}
