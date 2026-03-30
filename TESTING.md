# SimpleFences - Testing Guide

## Setup Steps

### 1. Start the Application
```powershell
C:\Users\MrIvo\Github\IVOESimpleFences\build\bin\Debug\SimpleFences.exe
```

The app runs in the background with a system tray icon (bottom-right of taskbar).

### 2. Create a Test Fence

1. Right-click the **SimpleFences** tray icon
2. Select **"New Fence"**
3. A fence window will appear near your cursor
4. Move it to a visible location on screen (drag title bar)

## Feature Tests

### ✅ Test 1: Drag Files Into Fence

**Objective**: Verify files move into fence folder

**Steps**:
1. Create test files on desktop or File Explorer
   ```powershell
   mkdir C:\Users\MrIvo\Desktop\FenceTest
   "Test content" | Out-File "C:\Users\MrIvo\Desktop\FenceTest\document.txt"
   mkdir "C:\Users\MrIvo\Desktop\FenceTest\TestFolder"
   ```

2. Drag these files/folders into the fence window:
   - Drag `document.txt` → files appear in fence ✓
   - Drag `TestFolder` → folder appears in fence ✓

3. Verify files moved:
   ```powershell
   ls "C:\Users\MrIvo\Desktop\FenceTest" # Should be empty
   
   # But fence folder has them:
   $fenceId = Get-ChildItem "C:\Users\MrIvo\AppData\Local\SimpleFences\Fences" | Select-Object -First 1
   ls "C:\Users\MrIvo\AppData\Local\SimpleFences\Fences\$fenceId"
   ```

### ✅ Test 2: Double-Click to Execute Files

**Objective**: Verify files open/execute when double-clicked

**Test Cases**:

#### 2a. Text File (Opens in Default Editor)
1. Double-click `document.txt` in fence
2. **Expected**: Notepad opens with content ✓

#### 2b. Folder (Opens in Explorer)
1. Double-click `TestFolder` in fence
2. **Expected**: File Explorer opens showing folder contents ✓

#### 2c. Batch File (Executes)
1. Create batch file:
   ```batch
   @echo off
   echo "Hello from batch!" > C:\Users\MrIvo\Desktop\script_output.txt
   pause
   ```
2. Drag into fence
3. Double-click `script.bat`
4. **Expected**: Command prompt opens, runs script ✓

#### 2d. Shortcut (Runs Target)
1. Create shortcut to Notepad:
   ```powershell
   $wsh = New-Object -ComObject WScript.Shell
   $link = $wsh.CreateShortcut("C:\path\to\fence\Notepad.lnk")
   $link.TargetPath = "notepad.exe"
   $link.Save()
   ```
2. Drag into fence
3. Double-click shortcut
4. **Expected**: Notepad launches ✓

### ✅ Test 3: Mouse Hover Highlighting

**Objective**: Verify visual feedback when mouse over items

**Steps**:
1. Move mouse over items in fence
2. **Expected**: Items highlight with darker background color ✓
3. Move mouse away
4. **Expected**: Highlight disappears ✓

### ✅ Test 4: Right-Click Item Menu

**Objective**: Verify item context menu works

**Steps**:
1. Right-click a file in the fence
2. **Expected**: Menu shows:
   - "Open" (launches the file)
   - "Delete Item" (removes from fence)

**Test "Open"**:
- Click "Open" on text file
- **Expected**: File opens in default handler ✓

**Test "Delete Item"**:
1. Right-click file
2. Select "Delete Item"
3. **Expected**: File removed from fence, backing folder ✓

### ✅ Test 5: Right-Click Fence Background Menu

**Objective**: Verify fence context menu

**Steps**:
1. Right-click empty space in fence (not on an item)
2. **Expected**: Menu shows:
   - "New Fence" → creates another fence ✓
   - "Rename Fence" → (placeholder for dialog)
   - "Delete Fence" → removes fence ✓

### ✅ Test 6: Drag to Move Fence

**Objective**: Verify dragging by title bar

**Steps**:
1. Click and hold title bar
2. Drag window to new position
3. Release
4. **Expected**: Fence moves smoothly ✓

### ✅ Test 7: Resize Fence

**Objective**: Verify resizing works

**Steps**:
1. Move mouse to window edge/corner
2. Cursor changes to resize cursor
3. Drag to resize
4. **Expected**: Window resizes, items reflow ✓

### ✅ Test 8: Persistence (Restart Test)

**Objective**: Verify fences reload after close

**Steps**:
1. Create fence with files
2. Move it to specific location
3. Resize to specific size
4. Close app: Right-click tray → "Exit"
5. Relaunch: Double-click executable
6. **Expected**: 
   - Fence appears in same location ✓
   - Same size as before ✓
   - Same files in fence ✓

**Verify metadata stored**:
```powershell
Get-Content "C:\Users\MrIvo\AppData\Local\SimpleFences\config.json"
# Should show fence ID, title, position, size
```

### ✅ Test 9: Duplicate File Handling

**Objective**: Verify rename on duplicate

**Steps**:
1. Create file: `document.txt`
2. Add to fence
3. Create another `document.txt`
4. Add to fence
5. **Expected**: Second one appears as `document (2).txt` ✓

### ✅ Test 10: Multiple Fences

**Objective**: Verify multiple independent fences

**Steps**:
1. Create Fence A with files: document.txt, photo.jpg
2. Create Fence B with files: script.bat, TestFolder
3. Double-click document.txt in Fence A → opens ✓
4. Double-click TestFolder in Fence B → opens explorer ✓
5. Delete item from Fence A → doesn't affect Fence B ✓

## Logs & Diagnostics

### Debug Log
```powershell
Get-Content "C:\Users\MrIvo\AppData\Local\SimpleFences\debug.log"
```

### Fence Metadata
```powershell
Get-Content "C:\Users\MrIvo\AppData\Local\SimpleFences\config.json"
```

### Backing Folder
```powershell
# List all fences
ls "C:\Users\MrIvo\AppData\Local\SimpleFences\Fences"

# See contents of specific fence
$fenceId = "Your-Fence-UUID-Here"
ls "C:\Users\MrIvo\AppData\Local\SimpleFences\Fences\$fenceId"
```

## Acceptance Criteria Checklist

- [ ] App launches, tray icon visible
- [ ] Right-click tray → "New Fence" creates window
- [ ] Fence window appears, can be moved/resized
- [ ] Drag desktop files into fence → files move to backing folder
- [ ] Drag folders into fence → folder moves to backing folder
- [ ] Double-click file in fence → opens/executes
- [ ] Double-click folder in fence → opens in Explorer
- [ ] Mouse hover highlights items
- [ ] Right-click item → "Open" works
- [ ] Right-click item → "Delete Item" removes file
- [ ] Right-click background → "New Fence" creates another fence
- [ ] Right-click background → "Delete Fence" removes fence
- [ ] Duplicate files handled with (2), (3) suffixes
- [ ] Close and relaunch → fences reload with same position/size/contents
- [ ] App exits cleanly

## Known Limitations (Phase 1)

- No file icons (text-only list)
- Rename Fence dialog not implemented
- No folder preview
- No shell integration
- No multi-monitor optimization
