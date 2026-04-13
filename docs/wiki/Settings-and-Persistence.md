# Settings and Persistence

## Settings Philosophy

- Consumer-facing settings should be minimal and preset-driven.
- Advanced implementation variables should stay internal where possible.
- Key migrations must preserve existing installs.

## Current Direction

Recent simplification reduces settings surface to core behavior and presets:

- Appearance presets (icon size, opacity profile, density, toggle/tray sizes)
- Space defaults (size preset, title template, focus behavior)
- Core behavior toggles (startup and tray behavior)
- Plugin essentials (marketplace/update channel and refresh)

## Where Settings Live

- `%LOCALAPPDATA%\SimpleSpaces\Spaces\settings.json`

## Compatibility Pattern

When introducing new keys:

1. Read new key first.
2. Fallback to legacy key where needed.
3. Optionally mirror values for transition period.
4. Remove old key only after migration confidence.

## References

- [../settings-system.md](../settings-system.md)
- [../MANUAL_SETTINGS_PERSISTENCE_CHECKLIST.md](../MANUAL_SETTINGS_PERSISTENCE_CHECKLIST.md)
- [../../src/core/AppKernel.cpp](../../src/core/AppKernel.cpp)
- [../../src/core/ThemePlatform.cpp](../../src/core/ThemePlatform.cpp)
- [../../src/plugins/builtins/BuiltinPlugins.cpp](../../src/plugins/builtins/BuiltinPlugins.cpp)
