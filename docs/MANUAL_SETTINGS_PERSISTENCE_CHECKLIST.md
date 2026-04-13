# Manual Settings Persistence Checklist

This checklist focuses only on settings-shell persistence and input/event edge cases.

Run with the existing manual runner:

- `pwsh -File docs/Run-ManualSmoke.ps1 -ChecklistPath docs/MANUAL_SETTINGS_PERSISTENCE_CHECKLIST.md`

## Preconditions

1. Ensure no running instance:
   - `tasklist /FI "IMAGENAME eq Spaces.exe" /FI "IMAGENAME eq IVOESpaces.exe"`
2. Build Debug host:
   - `C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe --build build --config Debug --target Spaces`
3. Launch app:
   - `build/bin/Debug/Spaces.exe`

## A. Baseline Persistence

1. Open Settings from tray.
2. Change one value in each field class:
   - Bool (toggle)
   - Enum (dropdown)
   - String (text)
   - Int (numeric text)
3. Close Settings window.
4. Reopen Settings and confirm values are unchanged.
5. Exit app, relaunch app, reopen Settings, confirm values persisted.

Expected:

- All edits survive close/reopen and restart.
- No silent reset to defaults.

## B. Focus and Commit Edge Cases

1. Edit an Int field but do not tab away.
2. Immediately close Settings window.
3. Reopen Settings and verify latest typed value persisted.
4. Repeat with a String field.

Expected:

- Closing Settings commits pending text edits.
- No data loss when focus remains in edit box.

## C. Sidebar Navigation and Selection Persistence

1. Click a sidebar tab and verify right pane updates.
2. Use keyboard navigation (Up/Down, Ctrl+Tab).
3. Toggle collapsed sidebar state.
4. Reopen Settings and verify:
   - selected tab remains coherent
   - right pane content matches selected tab

Expected:

- Selection changes always refresh right pane.
- No stale/previous tab content shown.

## D. Search and Filter Interaction

1. In Settings search box, type filter text that narrows fields.
2. Toggle each chip filter (All, Toggles, Choices, Text).
3. Clear search text.
4. Switch tabs and return.

Expected:

- Search/filter updates are immediate.
- Field list is restored correctly after clearing filters.
- No blank pane unless no matches exist.

## E. Marketplace and Plugin Pages Stability

1. Open Plugins tab.
2. Switch marketplace subtabs (Discover/Installed).
3. Change plugin filter/search fields.
4. Navigate away and back.

Expected:

- Subtab state updates without redraw artifacts or stale content.
- No crash, no frozen controls.

## F. Close-to-Tray Behavior vs Exit

1. Set `spaces.window.close_to_tray` to true.
2. Close Settings window and verify it hides correctly.
3. Set `spaces.window.close_to_tray` to false.
4. Close Settings window and verify it fully closes.

Expected:

- Window close behavior follows setting.
- Persistence remains intact in both modes.

## G. Optional Evidence Capture

1. Note pass/fail and notes in generated smoke report.
2. If a failure occurs, attach screenshot and relevant lines from:
   - `%LOCALAPPDATA%/Spaces/debug.log`
