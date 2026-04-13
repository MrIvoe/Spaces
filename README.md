# Spaces

[![Platform](https://img.shields.io/badge/platform-Windows-0078D6.svg)](#install)
[![Language](https://img.shields.io/badge/language-C%2B%2B17-00599C.svg)](#for-developers)
[![Build](https://img.shields.io/badge/build-CMake-064F8C.svg)](#for-developers)

Spaces is a desktop organizer for Windows.

Create resizable desktop Spaces, drop files into them, and keep your workspace clean without losing where files came from.

Current stable version: 1.01.010

## Why People Use Spaces

- Keep active files grouped by project on the desktop.
- Drag and drop files quickly into visual containers.
- Restore files back toward original locations safely.
- Keep layout and Space contents across app restarts.

## Install

1. Open the Releases page: https://github.com/MrIvoe/Spaces/releases
2. Download the newest installer named `Spaces-Setup-<version>.exe`.
3. Run the installer.
4. Launch Spaces from Start Menu.

If Windows SmartScreen appears, click More info, then Run anyway.

## Installer (Release Quality)

Spaces ships with an Inno Setup installer script at `installer/Spaces.iss`.

Installer behavior:

- Per-user installation path under `%LOCALAPPDATA%\Programs\Spaces\<version>`
- Start Menu shortcut creation
- Optional desktop shortcut
- Optional startup registration
- Uninstall registration in Windows Apps settings

Installer output:

- `installer/output/Spaces-Setup-<version>.exe`

Installer build details:

- [installer/README.md](installer/README.md)

## First 2 Minutes

1. Right-click the Spaces tray icon.
2. Select New Space.
3. Drag a file from desktop into the new Space.
4. Restart Spaces and confirm the Space comes back.

## Daily Use

- Create Space: tray icon -> New Space
- Open Settings: tray icon -> Settings
- Move files into a Space: drag and drop
- Restore an item: open item context menu and choose restore action
- Remove a Space safely: delete Space and follow prompts

## Safety Model

Spaces is designed around non-destructive behavior:

- Files are moved intentionally into each Space backing folder.
- Original locations are tracked for restore workflows.
- Restore avoids destructive overwrite by generating a safe name when needed.
- Failed operations are logged for diagnostics.

## Where Data Is Stored

Spaces user data is stored in:

- `%LOCALAPPDATA%\SimpleSpaces\Spaces\settings.json` for settings
- `%LOCALAPPDATA%\SimpleSpaces\config.json` for app metadata
- `%LOCALAPPDATA%\SimpleSpaces\debug.log` for logs
- `%LOCALAPPDATA%\SimpleSpaces\Spaces\` for Space-scoped data (themes, settings, etc.)

## Troubleshooting

If something looks wrong:

1. Check if another Spaces instance is already running.
2. Reopen the app from Start Menu.
3. Check logs in `%LOCALAPPDATA%\SimpleSpaces\`.
4. Run the manual checklists in [docs/MANUAL_RUNTIME_SMOKE_TEST.md](docs/MANUAL_RUNTIME_SMOKE_TEST.md) and [docs/MANUAL_SETTINGS_PERSISTENCE_CHECKLIST.md](docs/MANUAL_SETTINGS_PERSISTENCE_CHECKLIST.md).

## Plugins and Themes

- Plugins repository: https://github.com/MrIvoe/Spaces-Plugins
- Themes repository: https://github.com/MrIvoe/Themes

## For Developers

If you want to build, extend, or contribute:

- Start in [docs/wiki/Home.md](docs/wiki/Home.md)
- Build and test guide: [docs/wiki/Build-and-Test.md](docs/wiki/Build-and-Test.md)
- Architecture map: [docs/wiki/Architecture.md](docs/wiki/Architecture.md)
- Settings and persistence: [docs/wiki/Settings-and-Persistence.md](docs/wiki/Settings-and-Persistence.md)
- Plugin and theme integration: [docs/wiki/Plugins-and-Themes.md](docs/wiki/Plugins-and-Themes.md)
- Release process: [docs/wiki/Release-Workflow.md](docs/wiki/Release-Workflow.md)

Quick build:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
.\build\bin\Debug\Spaces.exe
```

Release build:

```powershell
cmake --build build --config Release
```

The host build also copies the latest executable to:

- `installer/output/Spaces.exe`

This is useful for installer packaging verification.

## Publish Checklist

Before pushing a release update:

1. Update version references (`src/AppVersion.h`, `installer/Spaces.iss`, release notes docs).
2. Build Debug and Release.
3. Run `build/Debug/HostCoreTests.exe`.
4. Build installer and verify `installer/output/Spaces-Setup-<version>.exe` exists.
5. Verify startup, tray menu, settings persistence, and drag/drop behavior.
6. Commit and push with release notes.

## Project Direction

Spaces is consumer-first at the product layer and extension-friendly at the platform layer.

That means:

- Everyday users should be able to install and use Spaces without reading engineering docs.
- Developers should have clear extension points and predictable host behavior.

## Support

If you hit an issue, open a GitHub issue with:

- what happened
- expected behavior
- steps to reproduce
- logs from `%LOCALAPPDATA%\\SimpleSpaces\\` when available
