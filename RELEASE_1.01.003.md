# Release 1.01.003 - Tray Recovery Fix Release

## Overview

**Spaces** is now **version 1.01.003**.

This fix release restores normal tray menu behavior after the startup stability workaround that previously forced a minimal fallback menu in some runs.

## Included in 1.01.003

- App version bumped to `1.01.003` in `src/AppVersion.h`
- Installer version bumped to `1.01.003` in `installer/Spaces.iss`
- Tray menu contributions restored by loading built-in plugins during kernel startup
- Degraded-mode behavior preserved so startup remains resilient if any single plugin fails
- Installer/documentation references aligned to `Spaces-Setup-1.01.003.exe`

## Expected Installer Artifact

```text
installer/output/Spaces-Setup-1.01.003.exe
```

## Versioning Policy Reminder

For each fix release:

1. Increment patch version (`1.01.003` -> `1.01.004`)
2. Build executable for testing
3. Build installer for distribution
4. Verify runtime startup and tray behavior
