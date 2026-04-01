# IVOESimpleFences

Public version: 0.0.008

IVOESimpleFences is a Win32 desktop organizer that groups files into floating fence windows while prioritizing reliability and recoverability.

## Repository Structure

```text
.
|-- .github/
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
|-- PHASE_1.md
|-- PHASE_2.md
`-- README.md
```

## Main Modules

- `App`: startup, message loop, shutdown ordering.
- `FenceManager`: authoritative in-memory model for fences and windows.
- `FenceStorage`: file move/restore/delete operations and `_origins.json` metadata.
- `Persistence`: config load/save using JSON.
- `FenceWindow`: fence UI, drag/drop, item rendering, context menus.
- `TrayMenu`: tray icon and app commands.
- `Win32Helpers`: app paths, logging, atomic replace helper.

## Build and Run

Requirements:

- Windows 10+
- CMake 3.16+
- Visual Studio C++ toolchain

Commands:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
```

Run:

```powershell
.\build\bin\Debug\SimpleFences.exe
```

## Data and Logging Paths

- Config: `%LOCALAPPDATA%\SimpleFences\config.json`
- Fence folders: `%LOCALAPPDATA%\SimpleFences\Fences\<FenceId>\`
- Origin metadata: `%LOCALAPPDATA%\SimpleFences\Fences\<FenceId>\_origins.json`
- Log: `%LOCALAPPDATA%\SimpleFences\debug.log`

## Version History

### 0.0.008

- Origin metadata now updates only after successful move operations.
- Atomic writes now use Windows-safe replacement (`MoveFileExW` replace + write-through).
- Fence deletion is now recovery-safe: partial restore failure keeps the fence record/window.
- Added keyboard-friendly `WM_CONTEXTMENU` behavior.
- Reused cached system image list in paint path.
- Expanded operation logging details for move/restore/delete/save failures.

### 0.0.007

- Migrated config persistence to robust `nlohmann/json` parsing/serialization.
- Added initial atomic temp-save flow for config/origin metadata.
- Removed hardcoded per-user log path.
- Centralized file restore/delete logic into storage layer.
- Added non-destructive restore conflict naming (`(restored N)`).
- Added structured drop results and improved error handling.
- Improved move/resize persistence and long-path drop support.
- Improved tray reliability and shutdown cleanup.

## Implementation Examples

Atomic replace helper:

```cpp
bool Win32Helpers::ReplaceFileAtomically(const std::filesystem::path& tempPath,
                                         const std::filesystem::path& targetPath) {
    return MoveFileExW(tempPath.c_str(), targetPath.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
```

Move result model:

```cpp
struct FileMoveResult {
    std::vector<std::filesystem::path> moved;
    std::vector<std::pair<std::filesystem::path, std::wstring>> failed;
};
```

Recovery-safe delete behavior:

```cpp
const RestoreResult restore = m_storage->RestoreAllItems(fence->backingFolder);
if (!restore.AllSucceeded()) {
    RefreshFence(fenceId);
    return;
}
```

## Project Phases

- Full Phase 1 details: see `PHASE_1.md`
- Full Phase 2 details: see `PHASE_2.md`
