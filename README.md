# Spaces

[![Platform](https://img.shields.io/badge/platform-Windows-0078D6.svg)](#download)
[![Version](https://img.shields.io/badge/version-1.01.010-2EA043.svg)](#current-version)

Spaces is a Windows desktop app that lets you create desktop Spaces as real windows backed by real folders.

You can drag files and folders into a Space, keep them organized visually, and restore them later with recovery-first behavior designed to avoid destructive overwrite mistakes.

Spaces works with:

- [Spaces-Plugins](https://github.com/MrIvoe/Spaces-Plugins) for extensions
- [Themes](https://github.com/MrIvoe/Themes) for shared appearance and theme tokens

## Why Spaces

- Real desktop windows
- Real backing folders on disk
- Safer restore behavior
- Startup persistence
- Plugin support
- Theme support

## What Happens To My Files

When you drop a file or folder into a Space, Spaces physically moves it into that Space's backing folder on disk.

After the move succeeds, Spaces records the original location so restore can be handled safely later.

Spaces is designed to prefer recovery over destructive actions:

- original paths are recorded only after a successful move
- restore does not intentionally overwrite an existing file
- if a conflict exists, a non-destructive restored name is generated
- if restore is only partially successful, Space deletion is aborted so recovery remains possible

Example:

```text
report.txt -> report (restored 1).txt
```

## Screenshots

Add screenshots here as they become available.

Suggested screenshots:

- main desktop view
- settings window
- plugins manager
- theme/customization view

## Download

Download the latest installer from [Releases](https://github.com/MrIvoe/Spaces/releases).

Example installer name:

```text
Spaces-Setup-1.01.010.exe
```

After installation, launch Spaces from the Start Menu or Desktop shortcut and begin creating Spaces.

## Plugins and Themes

### Plugins

Spaces supports extensions for things like:

- theme customization
- visual modes and layouts
- context actions
- integrations
- provider-style Space behavior

Learn more:

- [Spaces-Plugins](https://github.com/MrIvoe/Spaces-Plugins)

### Themes

Spaces uses shared appearance tokens and semantic mappings from:

- [Themes](https://github.com/MrIvoe/Themes)

## Safety and Recovery

Spaces is built around recovery-first behavior:

- restore does not overwrite existing destination files
- failed moves do not create stale origin metadata
- partial restore failure blocks Space deletion
- file operation failures are logged for troubleshooting

## Troubleshooting

### The app starts but expected behavior does not appear

Check:

- whether another instance is already running
- whether `%LOCALAPPDATA%\SimpleSpaces\config.json` is malformed
- whether `%LOCALAPPDATA%\SimpleSpaces\debug.log` contains startup or tray errors

### A file did not move or restore correctly

Check:

- source and destination path permissions
- whether another process is locking the file
- `%LOCALAPPDATA%\SimpleSpaces\debug.log` for details

### A Space did not disappear when you deleted it

This can happen intentionally if restore was only partially successful. The Space is kept so remaining items can still be recovered safely.

## Documentation

For deeper help and technical documentation, use the project Wiki.

Suggested Wiki pages:

- Getting Started
- Installation
- Using Spaces
- File Safety and Restore Behavior
- Plugins and Plugin Manager
- Themes and Appearance
- Troubleshooting
- FAQ

## Current Version

Current version: `1.01.010`
Current phase: `0.0.013`

## Contributing

If you want to contribute, please keep changes focused, test carefully, and update documentation when behavior changes.
