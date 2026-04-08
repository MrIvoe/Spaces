# Manual Runtime Smoke Test

This checklist validates settings persistence and the new plugin-first portal and context flows in a running host build.

One-command runner:

- `pwsh -File docs/Run-ManualSmoke.ps1`

## Preconditions

1. Ensure no running instance:
   - tasklist /FI "IMAGENAME eq Spaces.exe" /FI "IMAGENAME eq IVOESpaces.exe"
2. Build Debug host and tests:
   - C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe --build build --config Debug --target HostCoreTests Spaces
3. Run host core tests:
   - build/Debug/HostCoreTests.exe
4. Launch app:
   - build/bin/Debug/Spaces.exe

## A. Settings Persistence Smoke

1. Open Settings from tray.
2. Change these values:
   - Appearance -> Visual Modes -> Apply visual mode to all Spaces
   - Folder Portal -> General -> Default portal mode
   - Space Organizer -> Large file threshold (MB)
3. Close settings and exit app.
4. Relaunch app.
5. Reopen settings and verify all changed values persisted.

Expected:

- Values persist across restarts.
- No reset to defaults unless explicitly changed.

## B. Folder Portal Flow Smoke

1. From tray, choose New Folder Portal.
2. If no source was assigned, verify Space title indicates needs_source state.
3. Assign a valid source path using host portal setup flow.
4. Confirm Space refreshes and lists source folder items.
5. Rename or remove source folder externally.
6. Trigger Reconnect All Portals from tray.
7. Verify state transitions:
   - connecting -> ready when source returns
   - disconnected when source unavailable
8. Use Refresh All Portals and verify no crash and expected state updates.

Expected:

- Health state updates are visible and stable.
- Portal remains present when source is temporarily unavailable.

## C. Context Payload Routing Smoke

1. Right-click a Space background and run:
   - Organize by File Type
   - Apply Focus Visual Mode
2. Right-click an item and run:
   - Copy Item Metadata
   - Refresh Provider
3. Inspect debug log:
   - %LOCALAPPDATA%/Spaces/debug.log

Expected:

- Space actions target the selected Space.
- Item actions include selected item metadata.
- No unknown-command warnings for context actions.

## D. Organizer Deep Actions Smoke

1. In a Space with mixed files, run:
   - Organize by File Type
   - Flatten Organized Folders
   - Remove Empty Subfolders
   - Archive Old Files
   - Move Large Files
2. Verify file moves in backing folder and Space refreshes each time.
3. Toggle organizer settings and repeat one action to confirm settings take effect.

Expected:

- Actions perform real filesystem changes.
- Space contents update after each command.
- No partial-move crashes; errors are logged.

## E. Regression Spot-Check

1. Create a normal file_collection Space and drop files into it.
2. Verify non-portal drop/delete behavior still works.
3. Verify tray commands still execute:
   - New Space
   - Settings
   - Exit

Expected:

- Baseline core flows remain unchanged.
