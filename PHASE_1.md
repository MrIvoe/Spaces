# Phase 1 - Foundation and Core Functionality

Version range: initial baseline through pre-hardening milestones.

## Goals

- Deliver a working Win32 fence organizer.
- Support drag/drop into fence-backed folders.
- Persist and restore fence windows on startup.

## Implementations

- Core app lifecycle and message loop (`App`).
- Fence model and manager orchestration (`FenceManager`, `Models`).
- Fence UI with drag/move/resize and basic item list (`FenceWindow`).
- Backing-folder file moves and item scanning (`FenceStorage`).
- Tray icon and context menu commands (`TrayMenu`).

## Data Flow

1. App starts and initializes storage + persistence + manager.
2. Persistence loads fence metadata.
3. Manager recreates windows and refreshes items from storage folders.
4. User drops files into fence.
5. Storage moves files into backing folder.
6. Manager refreshes the fence window and saves metadata.

## Key Imports and Dependencies

- Win32 API headers: `windows.h`, `shellapi.h`, `shlobj.h`, `commctrl.h`.
- C++ STL: `filesystem`, `vector`, `map`, `string`, `fstream`.
- Link libraries: `user32`, `shell32`, `ole32`, `comctl32`, `shlwapi`, `kernel32`.

## Constraints

- Physical move model (files are moved to fence backing folder).
- Minimal UI by design; focus on stability and simplicity.

## Outcome

- A functioning SimpleFences app suitable for local testing with persistent fence windows and drag/drop workflows.
