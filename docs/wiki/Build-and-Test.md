# Build and Test

## Prerequisites

- Windows 10 or later
- Visual Studio 2022 Build Tools (Desktop C++)
- CMake
- Git
- PowerShell

## Workspace Layout

Common multi-repo setup:

- Spaces (host application)
- Spaces-Plugins (plugin implementations and template)
- Themes (theme token and package ecosystem)

New contributors should keep all three repositories available locally to validate end-to-end changes.

## Configure

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

## Build

```powershell
cmake --build build --config Debug
cmake --build build --config Release
```

If using VS Code tasks in this repository, prefer the default build task for daily iteration.

## Run

```powershell
.\build\bin\Debug\Spaces.exe
```

## Test

```powershell
.\build\Debug\HostCoreTests.exe
```

Minimum expected result: HostCoreTests passed.

## Recommended Daily Workflow

1. Pull latest main
2. Build Debug
3. Run HostCoreTests
4. Make focused change
5. Rebuild and retest
6. Run manual persistence checklist for settings-related changes

## Manual Validation Gates

Always verify:

- startup and tray actions
- new space creation
- drag/drop behavior
- settings save and reload behavior
- theme switching behavior when appearance-related changes are involved

## VS Code Task Path

In this workspace, the default build task is `Build Spaces Debug` and should be used for fast iteration.

## Build Troubleshooting

- If CMake path mapping gets stale after path moves, clear `build/CMakeCache.txt`, `build/CMakeFiles`, and `build/_deps`, then reconfigure.
- If binary output is missing for installer packaging, confirm Release build succeeded and verify `build/bin/Release/Spaces.exe` exists.
- If plugin-specific changes compile in Spaces-Plugins but fail in host runtime, validate manifest API compatibility and plugin id uniqueness.

## Manual Validation

- Runtime smoke: [../MANUAL_RUNTIME_SMOKE_TEST.md](../MANUAL_RUNTIME_SMOKE_TEST.md)
- Settings persistence: [../MANUAL_SETTINGS_PERSISTENCE_CHECKLIST.md](../MANUAL_SETTINGS_PERSISTENCE_CHECKLIST.md)
- Theme verification: [../MANUAL_THEME_VERIFICATION.md](../MANUAL_THEME_VERIFICATION.md)
