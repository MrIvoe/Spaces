# Debugging and Troubleshooting

Use this guide when a plugin or integration fails to load, does not render settings, or breaks command/menu behavior.

## Fast Triage

1. Confirm build succeeded
2. Run host tests
3. Check plugin manifest compatibility range
4. Verify plugin id uniqueness
5. Validate command id registration matches menu contribution
6. Verify settings page ids and field definitions

## Build Failures

If configuration fails after path changes:

- remove build/CMakeCache.txt
- remove build/CMakeFiles
- remove build/_deps
- reconfigure and rebuild

## Plugin Does Not Load

Check:

- manifest id, displayName, version are non-empty
- minHostApiVersion and maxHostApiVersion include current host API
- plugin id does not collide with existing plugin

Reference:

- [../plugin-system.md](../plugin-system.md)
- [../EXTENSIBILITY.md](../EXTENSIBILITY.md)

## Settings Page Does Not Show

Check:

- plugin declares settings_pages capability
- RegisterPage is called during Initialize
- pageId is unique
- field keys are non-empty and valid
- Enum fields include options

Schema reference:

- [../../src/extensions/SettingsSchema.h](../../src/extensions/SettingsSchema.h)

## Commands Do Not Trigger

Check:

- command registered in commandDispatcher
- menu contribution command id exactly matches
- invocation context exists for space-specific commands

## Persistence Problems

Check:

- values are written to settings store path
- expected key names were not changed accidentally
- migration fallback keys still resolve

Persistence references:

- [../MANUAL_SETTINGS_PERSISTENCE_CHECKLIST.md](../MANUAL_SETTINGS_PERSISTENCE_CHECKLIST.md)
- [../../src/core/AppKernel.cpp](../../src/core/AppKernel.cpp)

## Theme/Appearance Regressions

Check:

- key observer wiring for theme/app appearance keys
- fallback behavior still applies when new keys missing
- conflicts with alternate appearance plugins are logged and isolated

## Useful Validation Commands

```powershell
cmake --build build --config Debug
.\build\Debug\HostCoreTests.exe
```

## Escalation Path

If issue is unclear after triage:

1. collect reproducible steps
2. capture relevant logs and settings keys
3. identify whether issue belongs to Spaces, Spaces-Plugins, or Themes
4. open issue with scope and reproduction details
