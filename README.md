# IVOESimpleFences

[![Platform](https://img.shields.io/badge/platform-Windows-0078D6.svg)](#build-and-run)
[![Language](https://img.shields.io/badge/language-C%2B%2B17-00599C.svg)](#technology)
[![Build System](https://img.shields.io/badge/build-CMake-064F8C.svg)](#build-and-run)
[![Version](https://img.shields.io/badge/version-0.0.008-2EA043.svg)](#release-history)

Reliable Win32 desktop fences with strong focus on file-safety, recoverability, and predictable persistence behavior.

## Table of Contents

- [Overview](#overview)
- [Repository Layout](#repository-layout)
- [Architecture](#architecture)
- [Implementation Highlights](#implementation-highlights)
- [Technology](#technology)
- [Build and Run](#build-and-run)
- [Data, Metadata, and Logs](#data-metadata-and-logs)
- [Release History](#release-history)
- [Project Phases](#project-phases)
- [Roadmap](#roadmap)

## Overview

IVOESimpleFences provides draggable/resizable desktop fence windows that physically organize files into backing folders while preserving origin metadata for safe restore.

Primary goals:

- Keep user data safe first.
- Keep file operations diagnosable through logging.
- Keep behavior stable for real desktop testing.

## Repository Layout

```text
.
|-- .github/
|   `-- copilot-instructions.md
|-- .vscode/
|-- build/
|-- csharp-advanced/
|-- IVOEFences/
|-- src/
|   |-- App.cpp / App.h
|   |-- FenceManager.cpp / FenceManager.h
|   |-- FenceStorage.cpp / FenceStorage.h
|   |-- FenceWindow.cpp / FenceWindow.h
|   |-- Models.h
|   |-- Persistence.cpp / Persistence.h
|   |-- TrayMenu.cpp / TrayMenu.h
|   |-- Win32Helpers.cpp / Win32Helpers.h
|   `-- main.cpp
|-- CMakeLists.txt
|-- test_icons.bat
`-- README.md
```

## Architecture

- App layer: startup, initialization, message loop, and ordered shutdown.
- Manager layer: canonical fence model state and fence-window orchestration.
- Storage layer: move, restore, delete, and per-fence origin metadata handling.
- Persistence layer: JSON config load/save with atomic replacement.
- Window layer: painting, drag/drop, item interaction, and context menus.
- Utility layer: app-data paths, logging, and atomic file replacement helper.

## Implementation Highlights

### Safe file replacement (Windows-native)

```cpp
bool ReplaceFileAtomically(const std::filesystem::path& tempPath,
                           const std::filesystem::path& targetPath) {
    return MoveFileExW(tempPath.c_str(), targetPath.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
```

### Structured drop results

```cpp
struct FileMoveResult {
    std::vector<std::filesystem::path> moved;
    std::vector<std::pair<std::filesystem::path, std::wstring>> failed;
};
```

### Recovery-safe fence deletion

```cpp
const RestoreResult restore = m_storage->RestoreAllItems(fence->backingFolder);
if (!restore.AllSucceeded()) {
    RefreshFence(fenceId);
    return; // keep fence for recovery
}
```

### Non-destructive restore naming

When restore target already exists, a conflict-safe name is generated:

```text
example.txt -> example (restored 1).txt
```

## Technology

- Language: C++17
- UI/API: Win32 API
- Build: CMake
- JSON: nlohmann/json
- Toolchain: Visual Studio C++ (MSVC)

## Build and Run

Requirements:

- Windows 10+
- CMake 3.16+
- Visual Studio C++ toolchain

Configure and build:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
cmake --build build --config Release
```

Run (Debug):

```powershell
.\build\bin\Debug\SimpleFences.exe
```

## Data, Metadata, and Logs

- Config: `%LOCALAPPDATA%\SimpleFences\config.json`
- Fence folders: `%LOCALAPPDATA%\SimpleFences\Fences\<FenceId>\`
- Origin metadata: `%LOCALAPPDATA%\SimpleFences\Fences\<FenceId>\_origins.json`
- Debug log: `%LOCALAPPDATA%\SimpleFences\debug.log`

Behavior guarantees:

- Restore does not overwrite existing destination files.
- Failed moves do not create stale origin metadata.
- Partial restore failures during fence delete keep the fence intact for recovery.

## Release History

### 0.0.008

- Wrote origin entries only after successful file move completion.
- Replaced delete-then-rename save path with Win32-safe atomic replacement.
- Prevented fence deletion when restore is partial/failing.
- Added keyboard-compatible `WM_CONTEXTMENU` handling.
- Reused cached image list in paint loop (removed repeated shell calls).
- Improved logging context for file-operation failures.

### 0.0.007

- Migrated persistence to `nlohmann/json`.
- Added temp-save flow and improved error handling.
- Removed hardcoded per-user log path.
- Centralized restore/delete behavior into storage layer.
- Added structured move results and safer restore naming.
- Improved geometry persistence and long-path drag/drop support.
- Improved tray startup reliability and shutdown cleanup.

### 0.0.006 and Earlier

Early iterations established the base Win32 fence workflow, persistence model, and desktop interaction scaffolding.

## Project Phases

### Phase 1 - Core Product Foundation

- Fence creation, movement, resizing, and tray-driven workflow.
- Backing-folder storage model and item scanning.
- Basic persistence and startup restore.

### Phase 2 - Reliability and Safety Hardening

- JSON robustness and atomic save reliability.
- Recoverable delete/restore workflows.
- Long-path and context-menu behavior improvements.
- Better diagnostics and operational logging.

## Roadmap

- Improve visual polish and richer item presentation.
- Expand keyboard and accessibility flows.
- Continue hardening for edge-case filesystem behavior.
