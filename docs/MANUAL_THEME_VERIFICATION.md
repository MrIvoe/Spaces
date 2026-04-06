# Theme System - Manual Verification Checklist

## Overview
This document provides a comprehensive manual verification checklist for the SimpleFences theme system implementation. It covers the full lifecycle of theme operations from migration through rendering.

**Environment Requirements:**
- Windows 7+ system
- SimpleFences.exe built in Debug mode
- Access to `%LOCALAPPDATA%\SimpleFences\` directory
- Power Shell (for log viewing)
- Text editor (for JSON inspection)

---

## Phase 1: Migration from Legacy Settings

### Scenario 1.1: Fresh Install (No Legacy Settings)
- **Steps:**
  1. Delete `%LOCALAPPDATA%\SimpleFences\Fences\settings.json`
  2. Delete `%LOCALAPPDATA%\SimpleFences\` directory entirely
  3. Launch SimpleFences.exe
  4. Exit app after 5 seconds
  
- **Verification:**
  - [ ] Settings file created at `%LOCALAPPDATA%\SimpleFences\Fences\settings.json`
  - [ ] Inspect JSON:
    - [ ] `theme.source` = `"win32_theme_system"`
    - [ ] `theme.win32.theme_id` = `"graphite-office"` (default fallback)
    - [ ] `theme.migration_v2_complete` = `"true"`
    - [ ] `theme.win32.catalog_version` = `"1.0.0"`
  - [ ] No errors in `%LOCALAPPDATA%\SimpleFences\debug.log`

### Scenario 1.2: Upgrade from Legacy Settings (aurora_light style)
- **Preparation:**
  1. Create legacy settings file with underscored theme ID:
     ```json
     {
       "version": 1,
       "values": {
         "appearance.theme.style": "aurora_light",
         "appearance.theme.mode": "dark"
       }
     }
     ```
  2. Save to `%LOCALAPPDATA%\SimpleFences\Fences\settings.json`

- **Steps:**
  1. Launch SimpleFences.exe
  2. Wait for app to start (5 sec)
  3. Exit app
  
- **Verification:**
  - [ ] Settings file updated with canonical keys
  - [ ] Inspect JSON:
    - [ ] `theme.win32.theme_id` = `"aurora-light"` (underscores → hyphens)
    - [ ] `theme.source` = `"win32_theme_system"`
    - [ ] `theme.migration_v2_complete` = `"true"`
  - [ ] Legacy keys still present but unused
  - [ ] No migration errors in debug.log

### Scenario 1.3: Idempotency - Second Launch Does Not Re-migrate
- **Preparation:**
  1. Complete Scenario 1.2
  2. Note the timestamp of `settings.json` file

- **Steps:**
  1. Launch SimpleFences.exe second time
  2. Wait 5 sec
  3. Exit app
  4. Check `settings.json` last-modified timestamp

- **Verification:**
  - [ ] Timestamp unchanged (or minimal time delta)
  - [ ] Settings values identical to previous run
  - [ ] Marker `theme.migration_v2_complete` still `"true"`
  - [ ] No re-migration occurred

### Scenario 1.4: Migration with Invalid Theme ID
- **Preparation:**
  1. Create settings with unknown theme ID:
     ```json
     {
       "version": 1,
       "values": {
         "appearance.theme.style": "unknown_xyz",
         "appearance.theme.mode": "light"
       }
     }
     ```
  2. Save to settings file

- **Steps:**
  1. Launch SimpleFences.exe
  2. Wait 5 sec
  3. Exit app

- **Verification:**
  - [ ] Settings file updated
  - [ ] `theme.win32.theme_id` = `"graphite-office"` (fallback applied)
  - [ ] `theme.migration_v2_complete` = `"true"`
  - [ ] No crash or error dialogs
  - [ ] Debug log documents fallback

---

## Phase 2: Theme Application

### Scenario 2.1: Apply Theme via Settings Window
- **Prerequisites:**
  - Fresh theme migration complete (Scenario 1.1)

- **Steps:**
  1. Launch SimpleFences.exe
  2. Right-click tray → "Settings"
  3. Navigate to "Appearance" or "Theme" settings
  4. Select "Aurora Light" theme from dropdown
  5. Click "Apply" or similar button
  6. Wait 2 sec for theme refresh
  7. Exit app

- **Verification:**
  - [ ] UI updates immediately (colors change on all visible elements)
  - [ ] No visual glitches or partial renders
  - [ ] Fence windows update in sync
  - [ ] Settings window shows selected theme
  - [ ] Inspect settings file:
    - [ ] `theme.win32.theme_id` = `"aurora-light"`
    - [ ] `theme.win32.display_name` = `"Aurora Light"`

### Scenario 2.2: Switch Between Multiple Themes
- **Prerequisites:**
  - Settings window accessible

- **Steps:**
  1. Apply "Aurora Light" (as in 2.1)
  2. Immediately apply "Nocturne Dark"
  3. Immediately apply "Graphite Office"
  4. Wait 2 sec after final apply
  5. Exit app

- **Verification:**
  - [ ] Each theme switch is visually immediate
  - [ ] Final theme is "Graphite Office"
  - [ ] No lag or delay between switches
  - [ ] Fence colors match selected theme
  - [ ] Settings persisted correctly:
    - [ ] `theme.win32.theme_id` = `"graphite-office"`

### Scenario 2.3: App Restart with Previous Theme Selection
- **Prerequisites:**
  - Scenario 2.2 completed

- **Steps:**
  1. Exit SimpleFences.exe (if not already exited)
  2. Relaunch SimpleFences.exe
  3. Wait 3 sec for app to stabilize
  4. Check theme visually

- **Verification:**
  - [ ] Graphite Office theme loaded on startup
  - [ ] Fence colors match saved theme
  - [ ] No theme override or reset occurred
  - [ ] Debug log shows theme applied correctly

---

## Phase 3: Fallback & Error Handling

### Scenario 3.1: Graceful Fallback on Missing Theme
- **Preparation:**
  1. Manually edit settings.json
  2. Set `theme.win32.theme_id` = `"invalid-xyz"`
  3. Save file

- **Steps:**
  1. Launch SimpleFences.exe
  2. Wait 5 sec
  3. Observe UI / Exits

- **Verification:**
  - [ ] App launches without crash
  - [ ] Theme renders with fallback (Graphite Office theme applied)
  - [ ] No error dialog
  - [ ] Debug log documents fallback
  - [ ] Settings corrected:
    - [ ] `theme.win32.theme_id` = `"graphite-office"`

### Scenario 3.2: Corrupted Settings File Recovery
- **Preparation:**
  1. Corrupt settings.json (introduce invalid JSON)
  2. Save corrupted file

- **Steps:**
  1. Launch SimpleFences.exe
  2. Wait 3 sec
  3. Check if app recovered

- **Verification:**
  - [ ] App falls back to empty settings (starts fresh)
  - [ ] OR app recovers from backup (if implemented)
  - [ ] No crash
  - [ ] UI renders with default theme
  - [ ] Debug log documents error and recovery

---

## Phase 4: Plugin Conflict Detection

### Scenario 4.1: No Conflicts with Canonical Selector
- **Prerequisites:**
  - App with "builtin.appearance" plugin loaded

- **Steps:**
  1. Check debug.log for plugin load messages
  2. Verify "builtin.appearance" is active

- **Verification:**
  - [ ] No conflict warnings in debug.log
  - [ ] Plugin loaded successfully
  - [ ] Theme commands from plugin work correctly

### Scenario 4.2: Plugin Conflict Detection (if custom plugin loaded)
- **Preparation:**
  1. (If custom appearance plugins are available)
  2. Load a plugin that declares appearance-related commands

- **Steps:**
  1. Launch SimpleFences.exe with custom plugin
  2. Check debug.log

- **Verification:**
  - [ ] Conflict detected for non-canonical plugins
  - [ ] Warning logged with plugin ID
  - [ ] Plugin disabled or warning issued
  - [ ] Canonical selector ("builtin.appearance") remains active

---

## Phase 5: Theme Persistence Across Sessions

### Scenario 5.1: Persistence Through Multiple Restarts
- **Steps:**
  1. Launch SimpleFences.exe
  2. Change theme to "Aurora Light"
  3. Exit app
  4. Relaunch SimpleFences.exe
  5. Verify theme visually
  6. Change theme to "Nocturne Dark"
  7. Exit app
  8. Relaunch SimpleFences.exe (3rd time)
  9. Verify theme visually

- **Verification:**
  - [ ] Each reload preserves the previously applied theme
  - [ ] Aurora Light present after 2nd launch
  - [ ] Nocturne Dark present after 3rd launch
  - [ ] No theme resets or drops

### Scenario 5.2: Settings File Persists After Crash / Force-Close
- **Steps:**
  1. Apply "Aurora Light" theme
  2. Force-close SimpleFences.exe (Task Manager or Ctrl+Shift+Esc)
  3. Relaunch SimpleFences.exe
  4. Verify theme visually

- **Verification:**
  - [ ] Aurora Light theme still applied
  - [ ] Settings file intact
  - [ ] No data loss

---

## Phase 6: Token Resolution & Color Accuracy

### Scenario 6.1: Color Token Consistency
- **Prerequisites:**
  - Two machines or VM instances with SimpleFences

- **Steps:**
  1. On Machine A: Apply "Aurora Light" theme
  2. On Machine B: Apply "Aurora Light" theme
  3. Compare visual appearance (colors)
  4. Measure RGB values if possible

- **Verification:**
  - [ ] Color appearance identical across instances
  - [ ] No random color variation
  - [ ] No color shifts between sessions
  - [ ] Tokens map consistently to COLORREF values

### Scenario 6.2: Text Readability with Different Themes
- **Steps:**
  1. Apply "Nocturne Dark" (dark background, bright text)
     - Verify text is readable
  2. Apply "Aurora Light" (light background, dark text)
     - Verify text is readable
  3. Apply "Graphite Office" (neutral tone)
     - Verify text is readable

- **Verification:**
  - [ ] All text clearly legible
  - [ ] No contrast issues
  - [ ] Sufficient color difference between text and background

---

## Phase 7: Package Validation (Third-Party Themes)

### Scenario 7.1: Valid Theme Package Acceptance
- **Preparation:**
  1. Create a valid theme .zip package:
     - File: `theme-metadata.json` (with valid JSON, theme_id, display_name)
     - Directory: `theme/tokens/` with `default.json`
     - `default.json` contains color tokens (hex values)
     - Total size < 10MB
     - No .exe, .dll, or script files

- **Steps:**
  1. If UI supports package import:
     - Right-click tray → "Import Theme"
     - Select the .zip file
     - Wait for validation
  2. Or manually extract to themes directory (if implemented)

- **Verification:**
  - [ ] Package accepted
  - [ ] No validation errors
  - [ ] Theme available in theme selector
  - [ ] Can apply and see colors

### Scenario 7.2: Invalid Package Rejection (Wrong File Type)
- **Preparation:**
  1. Create invalid .zip (e.g., .exe file inside)

- **Steps:**
  1. Attempt to import the invalid package

- **Verification:**
  - [ ] Validation rejects package
  - [ ] Clear error message displayed
  - [ ] Package not imported
  - [ ] App remains stable

### Scenario 7.3: Package Size Limit Enforcement
- **Preparation:**
  1. Create .zip > 10MB (valid structure, but large)

- **Steps:**
  1. Attempt to import

- **Verification:**
  - [ ] Validation rejects with "package too large"
  - [ ] No partial import
  - [ ] Error message clear

### Scenario 7.4: Security - Forbidden Content Rejection
- **Preparation:**
  1. Create .zip with:
     - Valid metadata
     - Executable file (.exe) inside
     - Valid structure otherwise

- **Steps:**
  1. Attempt to import

- **Verification:**
  - [ ] Validation rejects due to forbidden content
  - [ ] Error message identifies executable
  - [ ] Package rejected entirely

---

## Phase 8: Rendering Performance & Atomicity

### Scenario 8.1: Atomic Theme Apply (No Partial Renders)
- **Prerequisites:**
  - App with visible fence windows

- **Steps:**
  1. Apply theme change
  2. Observe window rendering
  3. Watch for any visual glitches (color flashing, redraw lines)

- **Verification:**
  - [ ] Theme colors apply uniformly
  - [ ] No interim visual states
  - [ ] Single cohesive repaint

### Scenario 8.2: Rapid Theme Switches
- **Steps:**
  1. Open Settings window
  2. Rapidly click through 5 different themes (Aurora → Nocturne → Graphite → Aurora → Nocturne)
  3. Observe rendering

- **Verification:**
  - [ ] No lag or stuttering
  - [ ] App remains responsive
  - [ ] Final theme correctly applied
  - [ ] No visual artifacts

### Scenario 8.3: Theme Apply During Window Drag
- **Steps:**
  1. Start dragging a fence window
  2. While dragging, apply a theme change
  3. Complete the drag

- **Verification:**
  - [ ] Drag completes normally
  - [ ] Theme updates after drag
  - [ ] No interference between drag and theme apply

---

## Phase 9: Debug Log Validation

### Log Checks (all scenarios)
After each major scenario:
- [ ] Open `%LOCALAPPDATA%\SimpleFences\debug.log`
- [ ] Search for theme-related messages
  - [ ] Look for "migration" messages
  - [ ] Look for "theme" messages
  - [ ] No ERR or FATAL messages related to theme

### Key Log Entries to Expect
- Migration startup: `"Theme migration completed"`
- Theme apply: `"Theme applied: {themeId}"`
- Fallback: `"Theme fallback applied: {reason}"`
- No entries: `"Undefined token"` or `"Color resolution failed"`

---

## Phase 10: Summary & Sign-Off

| Check | Status | Notes |
|-------|--------|-------|
| Phase 1: Migrations | [ ] Pass / [ ] Fail | |
| Phase 2: Theme Application | [ ] Pass / [ ] Fail | |
| Phase 3: Fallback & Errors | [ ] Pass / [ ] Fail | |
| Phase 4: Plugin Conflicts | [ ] Pass / [ ] Fail | |
| Phase 5: Persistence | [ ] Pass / [ ] Fail | |
| Phase 6: Token Resolution | [ ] Pass / [ ] Fail | |
| Phase 7: Package Validation | [ ] Pass / [ ] Fail | |
| Phase 8: Rendering | [ ] Pass / [ ] Fail | |
| Phase 9: Debug Logs | [ ] Pass / [ ] Fail | |

**Overall Result:** [ ] PASS / [ ] FAIL

**Tester Name:** _____________________  
**Date:** _____________________  
**Build Version:** _____________________  

**Notes / Issues Found:**
```


```

---

## Appendix: Quick Reference

### Known Theme IDs (Catalog)
- `graphite-office` (default fallback)
- `aurora-light`
- `nocturne-dark`
- `amber-terminal`
- `arctic-glass`
- `brass-steampunk`
- `copper-foundry`
- `emerald-ledger`
- `forest-organic`
- `harbor-blue`
- `ivory-bureau`
- `mono-minimal`
- `neon-cyberpunk`
- `nova-futuristic`
- `olive-terminal`
- `pop-colorburst`
- `rose-paper`
- `storm-steel`
- `sunset-retro`
- `tape-lo-fi`

### Settings File Location
`%LOCALAPPDATA%\SimpleFences\Fences\settings.json`

### Debug Log Location
`%LOCALAPPDATA%\SimpleFences\debug.log`

### Test Command (Unit Tests)
```powershell
c:\Users\MrIvo\Github\IVOESimpleFences\build\Debug\HostCoreTests.exe
```

### Expected CTest Result
```
100% tests passed, 0 tests failed out of 1
```
