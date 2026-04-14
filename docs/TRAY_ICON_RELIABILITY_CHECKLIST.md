# Tray Icon Reliability Checklist

**Purpose:** Ensure the tray icon—the only gateway to settings—is robust, recoverable, and always functional.

**Context:** Tray icon failure = locked-out users (no UI access to settings). TaskbarCreated message recovery is implemented in TrayMenu.cpp but must be validated.

Run with:
```powershell
pwsh -File docs/Run-ManualSmoke.ps1 -ChecklistPath docs/TRAY_ICON_RELIABILITY_CHECKLIST.md
```

## Preconditions

1. Ensure no running instance:
   ```
   tasklist /FI "IMAGENAME eq Spaces.exe"
   ```

2. Build Debug host:
   ```
   cmake --build build --config Debug --target Spaces
   ```

3. Launch app:
   ```
   build/bin/Debug/Spaces.exe
   ```

4. **Verify tray icon is visible** in system notification area (bottom-right taskbar)

---

## A. Baseline Tray Visibility

**Goal:** Confirm tray icon is always visible and interactive.

1. Run app and wait for startup to complete (~2 seconds).
2. Look at system notification area (bottom-right of taskbar).
3. Confirm "SimpleSpaces" icon is visible.
4. Hover over tray icon and verify tooltip shows **"SimpleSpaces"**.

**Expected:**
- ✅ Icon visible immediately after app launch
- ✅ Tooltip readable (not blank or corrupted)
- ✅ Icon is clickable (cursor changes to hand pointer on hover)

**Evidence:**
- Screenshot of tray icon visible in notification area
- Tooltip text readable and correct

---

## B. Tray Context Menu (Right-Click)

**Goal:** Verify right-click context menu appears and functions.

1. Right-click tray icon.
2. Verify context menu appears near cursor or near icon.
3. Confirm menu contains at least these items:
   - "Add-ons" (settings entry)
   - "Exit" (app exit)
4. Click elsewhere to close menu without selecting.
5. Right-click again to verify menu re-appears (not stuck).

**Expected:**
- ✅ Menu appears consistently on right-click
- ✅ All menu items are visible and not truncated
- ✅ Menu disappears cleanly on escape/click-away
- ✅ Menu can be re-opened (not locked after one use)

**Evidence:**
- Screenshots of context menu appearance
- Note any menu item text truncation or alignment issues

---

## C. Open Settings from Tray

**Goal:** Confirm settings window opens via tray.

1. Right-click tray icon.
2. Click "Add-ons" (or equivalent Settings entry).
3. Verify Settings window opens within 1 second.
4. Confirm Settings window is focused (title bar highlighted).
5. Close Settings window.
6. Repeat 2-3 times to verify consistency.

**Expected:**
- ✅ Settings opens promptly every time
- ✅ Window appears at expected position (not off-screen)
- ✅ No error dialogs or log errors
- ✅ Multiple open/close cycles work smoothly

**Evidence:**
- Screenshots of Settings window opening from tray
- Timing notes (how long between click and window appearance)

---

## D. Tray Icon Double-Click (Toggle Spaces Visibility)

**Goal:** Verify double-click behavior works correctly.

**Preconditions:**
- At least one Space has been created (via Settings or tray menu "Create Space")

1. Double-click tray icon.
2. Verify all Space windows hide or show (toggle behavior).
3. Observe tooltip text changes to **"SimpleSpaces (hidden)"** when spaces are hidden.
4. Double-click again to show spaces.
5. Verify tooltip returns to **"SimpleSpaces"**.

**Expected:**
- ✅ Double-click toggles all spaces visibility
- ✅ Tooltip updates to reflect hidden state
- ✅ Toggle works multiple times consistently
- ✅ No spaces appear/disappear unexpectedly

**Evidence:**
- Screenshots showing tooltip text changing ("SimpleSpaces" vs "SimpleSpaces (hidden)")
- Note any delays or timing issues

---

## E. Tray Icon Recovery After Explorer Restart

**Goal:** Validate tray icon recovers when Explorer crashes/restarts.

**This tests TaskbarCreated message handling in TrayMenu::WndProc()**

1. Run app with tray icon visible.
2. Open Task Manager: `Ctrl+Shift+Esc`.
3. Find "Windows Explorer" process.
4. Right-click and select "Restart".
5. Wait 3-5 seconds for Explorer to restart (taskbar reappears).
6. Verify SimpleSpaces tray icon is **still visible**.
7. Verify tooltip still works (hover over icon).
8. Right-click and verify context menu appears.

**Expected:**
- ✅ Tray icon automatically re-registers after Explorer restart
- ✅ No app crash or hang during recovery
- ✅ Functionality fully restored (no ghosted icon)
- ✅ No duplicate tray icons created
- ✅ No error logs for this recovery

**Evidence:**
- Screenshots before/after Explorer restart showing tray icon presence
- Check `%LOCALAPPDATA%\SimpleSpaces\debug.log` for any errors mentioning "TaskbarCreated" or "NIM_" operations

---

## F. Tray Functionality with Settings Window Open

**Goal:** Verify tray remains responsive while settings window is open.

1. Right-click tray → "Add-ons" to open Settings.
2. While Settings is open, right-click tray icon again.
3. Verify context menu still appears (tray not locked by Settings window).
4. Close the menu (don't click anything).
5. Interact with Settings window (type in fields, switch tabs).
6. While interacting with Settings, right-click tray a third time.
7. Verify context menu response time is <200ms.

**Expected:**
- ✅ Tray menu appears even when Settings window is open
- ✅ Tray menu doesn't block Settings interaction
- ✅ No menu lag or freezing when Settings is active
- ✅ Settings window still has focus after tray interaction

**Evidence:**
- Screenshots showing tray menu while Settings is open
- Notes on responsiveness and any lag observed

---

## G. Tray Icon Under Low Resource Conditions

**Goal:** Verify poor system conditions don't disable tray.

1. Open Resource Monitor: `resmon.exe`.
2. Monitor Memory and CPU while app runs.
3. Launch multiple Space windows to increase resource usage.
4. Open Settings window.
5. While system is under load, verify:
   - Tray icon is still visible
   - Right-click still produces menu
   - Tooltip still updates
   - Double-click still toggles spaces

**Expected:**
- ✅ Tray icon remains functional under memory/CPU pressure
- ✅ No tray icon disappears during high load
- ✅ Menu response time is acceptable (<500ms) even under load

**Evidence:**
- Screenshots showing Resource Monitor + tray icon visible
- Performance notes (did tray become sluggish?)

---

## H. Multiple Display Configuration

**Goal:** Verify tray works consistently across multiple monitors.

1. If available, connect a second monitor.
2. Verify SimpleSpaces tray icon is visible on **primary taskbar** only.
3. Move main app window to secondary monitor (if you have Spaces created).
4. Right-click tray icon on primary taskbar.
5. Verify context menu appears near primary taskbar.
6. Open Settings from tray.
7. Verify Settings window opens on primary display (or remember which display for consistency).
8. Close Settings.
9. Repeat right-click to verify tray still responsive.

**Expected:**
- ✅ Tray icon appears only on primary taskbar
- ✅ Tray menu appears on primary taskbar (not orphaned)
- ✅ Tray functionality unchanged with multiple displays

**Evidence:**
- Screenshots showing tray on primary display
- Note how many displays were tested with

---

## I. Tray with Taskbar Hidden

**Goal:** Verify tray icon accessibility when taskbar is in auto-hide mode.

1. Right-click Windows taskbar.
2. Select "Taskbar settings" → "Automatically hide the taskbar" → Enable.
3. Move mouse away from taskbar area (it should hide).
4. Move mouse to taskbar edge to reveal it.
5. Verify SimpleSpaces icon is visible.
6. Right-click icon and verify menu appears.
7. Disable auto-hide and restore default taskbar visibility.

**Expected:**
- ✅ Tray icon accessible even with hidden taskbar
- ✅ No icon corruption when taskbar hides/shows
- ✅ Menu functions the same

**Evidence:**
- Screenshots showing tray icon with taskbar hidden
- Notes on ease of tray access in this mode

---

## J. Log Verification - TrayMenu Operations

**Goal:** Ensure no errors logged for tray operations.

1. Perform all above checks (A-I).
2. Exit app gracefully: Right-click tray → Exit.
3. Open `%LOCALAPPDATA%\SimpleSpaces\debug.log`.
4. Search for these error keywords:
   - **"Tray"**
   - **"Shell_NotifyIcon"**
   - **"WMAPP_TRAYICON"**
   - **"TaskbarCreated"**
   - **"NIM_"**
5. Confirm no ERROR lines for these topics.

**Expected:**
- ✅ No "TrayMenu::Create failed" errors
- ✅ No "Shell_NotifyIconW(NIM_ADD) failed" errors
- ✅ No "Tray class registration failed" errors
- ✅ No unhandled tray commands logged

**Evidence:**
- Paste relevant log excerpt showing tray operations completed cleanly
- Highlight any warnings/info lines (these are acceptable; only ERRORs matter)

---

## Summary

| Test | Pass/Fail | Notes |
|------|-----------|-------|
| A. Baseline Tray Visibility | | |
| B. Context Menu (Right-Click) | | |
| C. Open Settings from Tray | | |
| D. Double-Click Toggle | | |
| E. Explorer Restart Recovery | | |
| F. Tray + Settings Window Open | | |
| G. Low Resource Conditions | | |
| H. Multiple Displays | | |
| I. Taskbar Auto-Hide | | |
| J. Log Verification | | |

---

## Failure Escalation

If **any** test fails:

1. **Collect evidence:**
   - Screenshot showing the failure
   - Full `%LOCALAPPDATA%\SimpleSpaces\debug.log`
   - Record exact steps to reproduce

2. **Check for known issues:**
   - Windows Update installed recently? (can affect taskbar)
   - Third-party taskbar extensions active? (Fences, 7+ Taskbar Tweaker, etc.)
   - Anti-malware recently updated? (can block tray registration)

3. **Check TrayMenu.cpp:**
   - Line ~306: TaskbarCreated message should be handled
   - Line ~128: Shell_NotifyIconW(NIM_ADD) must succeed
   - Line ~135: Icon load fallback to standard icon

4. **Escalate:**
   - File GitHub issue with full checklist results + logs
   - Tag with `[tray-icon]` label for priority

---

## Release Gate

**This checklist must PASS before shipping any release:**
- ✅ All tests A-J pass
- ✅ No tray-related errors in log
- ✅ Multiple runs confirm consistency
- ✅ Tested on at least 2 Windows versions (e.g., Win 10 22H2, Win 11 23H2)

**Rationale:** Tray icon is the user's only way to access settings. If it fails, support burden skyrockets. This checklist prevents silent regressions.
