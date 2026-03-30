# SimpleFences - Lightweight Win32 Desktop Organizer

A simple, reliable desktop organizer for Windows that lets you create borderless "fence" windows to group and organize your desktop files.

## Features (Phase 1)

- Create draggable fence windows on the desktop
- Drag files/folders from the desktop into a fence
- Files are physically moved into a backing folder
- Persist fence position, size, and title
- Reload fences on startup
- Right-click tray icon to create new fence
- Right-click inside fence for context menu (new fence, rename, delete)

## Architecture

- **App**: Startup, shutdown, message loop
- **FenceManager**: Creates, loads, saves, and coordinates fences
- **FenceWindow**: One Win32 window per fence with drag/drop support
- **FenceStorage**: Manages backing folders and file operations
- **Persistence**: Loads/saves fence metadata to JSON
- **TrayMenu**: System tray icon and context menus
- **Win32Helpers**: Common Win32 utilities

## Build Requirements

- Windows 10 or later
- CMake 3.16 or later
- Microsoft Visual Studio 2019 or later (C++17 support required)

## Building

### Using CMake (Command Line)

```bash
mkdir build
cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Debug
```

### Using CMake (Visual Studio)

1. Open the folder in Visual Studio
2. CMake will auto-configure
3. Build → Build All

### Using Visual Studio directly

1. Generate Visual Studio project:

```bash
cmake -G "Visual Studio 16 2019" -A x64 build
```

2. Open `build\SimpleFences.sln` in Visual Studio
3. Build → Build Solution

## Running

After building, the executable is at:

```
build\bin\Debug\SimpleFences.exe
(or Release depending on configuration)
```

Double-click to run, or from command line:

```bash
.\build\bin\Debug\SimpleFences.exe
```

## File Locations

- Fence metadata: `%LOCALAPPDATA%\SimpleFences\config.json`
- Fence folders: `%LOCALAPPDATA%\SimpleFences\Fences\<FenceId>\`

## Usage

1. **Create a Fence**: Right-click the tray icon → "New Fence"
2. **Move a Fence**: Drag the title bar
3. **Resize a Fence**: Drag the window edges/corners
4. **Add Files**: Drag files from desktop or Explorer into the fence
5. **Rename Fence**: Right-click inside fence → "Rename Fence"
6. **Delete Fence**: Right-click inside fence → "Delete Fence"
7. **Exit**: Right-click tray icon → "Exit"

## Implementation Notes

- Each fence is a real `WS_POPUP` window with `WS_THICKFRAME`
- Files dropped into a fence are moved into `%LOCALAPPDATA%\SimpleFences\Fences\<id>\`
- Metadata is stored as simple JSON (one object per line)
- No shell integration or metadata-only membership
- No file hiding or desktop modification

## Next Steps (Phase 2)

- System image list for file icons
- Real file icon rendering in fences
- Better window positioning relative to desktop
- Rename fence dialog
- Delete confirmation dialog
- Multi-monitor support
- Customize fence colors/appearance

## Known Limitations

- Phase 1: Simple text-only item display
- No tab support
- No folder portals
- No shell extensions
- No cloud sync
- No plugins
