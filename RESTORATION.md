# SimpleFences - File Restoration Feature

## Overview

Files added to fences can now be **automatically restored** to their original location when:
1. A fence is **deleted**
2. An individual item is **removed from a fence**
3. The app restarts without running a fence

## How It Works

### File Origin Tracking

When you drag files into a fence:
1. SimpleFences records the **original file path** 
2. A mapping file `_origins.json` is created in the fence's backing folder
3. Stores: `"filename.txt":"C:\path\where\it\came\from\filename.txt"`

### File Restoration

#### Scenario 1: Delete Individual Item from Fence
1. Right-click file in fence → **"Delete Item"**
2. SimpleF ences checks `_origins.json` for original path
3. If original path exists: **moves file back to that location** ✓
4. If no origin recorded: deletes file from fence ✗

#### Scenario 2: Delete Entire Fence
1. Right-click fence background → **"Delete Fence"**
2. SimpleFences **restores ALL items** to original locations
3. Fence metadata removed from config
4. Backing folder cleaned up after restore

#### Scenario 3: App Restart (Missed Restoration)
If a fence was deleted while app wasn't running:
1. On startup, fences reload from config
2. Check backing folders for items with origins
3. **Can manually restore** by checking _origins.json

## Technical Implementation

### Data Structure

**FenceItem** now includes:
```cpp
struct FenceItem
{
    std::wstring name;              // e.g. "document.txt"
    std::wstring fullPath;          // e.g. "C:\...\Fences\<UUID>\document.txt"
    std::wstring originalPath;      // NEW: "C:\Users\...\Desktop\document.txt"
    bool isDirectory;
};
```

### Origin Metadata File

**Location**: `%LOCALAPPDATA%\SimpleFences\Fences\<FenceId>\_origins.json`

**Format**: Simple key-value pairs (one per line)
```
"file1.txt":"C:\Users\MrIvo\Desktop\file1.txt"
"file2.txt":"C:\Users\MrIvo\Desktop\file2.txt"
"TestFolder":"C:\Users\MrIvo\Desktop\TestFolder"
```

### FenceStorage Methods

**New public methods**:
```cpp
bool RestoreItemToOrigin(const FenceItem& item);
bool RestoreAllItems(const std::wstring& fenceFolder);
```

**Private helpers**:
```cpp
void SaveItemOrigins(const std::wstring& fenceFolder, 
                     const map<wstring, wstring>& origins);
map<wstring, wstring> LoadItemOrigins(const std::wstring& fenceFolder);
std::wstring GetOriginsPath(const std::wstring& fenceFolder);
```

## Testing Guide

### Test 1: Single Item Restoration

**Setup**:
1. Create files: `C:\Users\MrIvo\Desktop\RestoreTest\file1.txt`
2. Launch SimpleFences
3. Create a fence
4. Drag `file1.txt` into fence

**Expected State**:
```
Before:
  C:\Users\MrIvo\Desktop\RestoreTest\file1.txt  → exists
  Fence folder: [empty]

After drag:
  C:\Users\MrIvo\Desktop\RestoreTest\file1.txt  → gone (moved)
  Fence folder: file1.txt                        → exists
  Fence folder: _origins.json                    → created
```

**Test Deletion**:
1. Right-click `file1.txt` in fence → "Delete Item"
2. **Expected**: File disappears from fence AND reappears in original location:
   ```
   C:\Users\MrIvo\Desktop\RestoreTest\file1.txt  → restored ✓
   Fence folder: file1.txt                        → gone
   Fence folder: _origins.json                    → updated
   ```

### Test 2: Folder Restoration

**Setup**:
1. Create folder structure:
   ```
   C:\Users\MrIvo\Desktop\RestoreTest\
   ├── TestFolder\
   │   └── nested.txt
   ```
2. Drag `TestFolder` into fence

**Test Deletion**:
1. Right-click `TestFolder` in fence → "Delete Item"
2. **Expected**: Entire folder restored to original location with all contents

### Test 3: Full Fence Restoration

**Setup**:
1. Create multiple test files
2. Create fence with 3-5 files
3. Note fence position/size

**Test**:
1. Right-click fence background → "Delete Fence"
2. **Expected Results**:
   - ✓ All files restored to original locations
   - ✓ Fence window closes
   - ✓ Fence removed from config.json
   - ✓ Backing folder cleaned

**Verify Restoration**:
```powershell
# All original paths should have files back
ls "C:\Users\MrIvo\Desktop\RestoreTest"
# Should show: file1.txt, file2.txt, TestFolder

# Fence folder should be gone or empty
ls "C:\Users\MrIvo\AppData\Local\SimpleFences\Fences\<UUID>"
# Should be empty or not exist
```

### Test 4: Duplicate Handling on Restore

**Setup**:
1. File exists in source and fence folder as different versions

**Test Restore**:
1. Original path: `C:\Desktop\file.txt` (v1)
2. Fence has: `file.txt` (v2 - modified)
3. Delete from fence

**Expected**: Fence version (v2) overwrites original with `overwrite_existing`

### Test 5: Missing Original Destination

**Setup**:
1. Add files from location to fence
2. Delete original parent folder
3. Try to restore

**Expected**: 
- Code detects missing parent directory
- Creates parent directory if needed (`create_directories`)
- Restores file to recreated location

## Implementation Details

### File Restoration Algorithm

```
RestoreItem(fenceFolder, itemName):
  1. Load origins map from _origins.json
  2. Get original path from map
  3. Check if source exists (in fence folder)
  4. Check if destination directory exists
     - If not: create_directories(destDir)
  5. Move operation:
     - Try rename first (same volume)
     - Fall back to copy+delete (different volume)
  6. Update _origins.json (remove this entry)
  7. Return success/failure
```

### Fence Deletion Flow

```
DeleteFence(fenceId):
  1. Find fence in m_fences
  2. Call m_storage->RestoreAllItems(fence.backingFolder)
     - This restores ALL items in that fence
  3. Remove from m_windows
  4. Remove from m_fences vector
  5. Update config.json (persisted)
```

## Edge Cases Handled

✓ **Same-volume moves**: Uses `rename()` (instant)  
✓ **Cross-volume moves**: Uses `copy+delete` (safer)  
✓ **Missing destination**: Creates parent directories  
✓ **File overwrite**: Uses `overwrite_existing` flag  
✓ **Folder restore**: Recursive copy/delete  
✓ **Partial failures**: Restores what's possible  
✓ **Metadata corruption**: Defaults to deletion if no origin found  

## Performance

- **restore single item**: O(1) load origins + O(n) file move
- **restore all items**: O(n) items × file move cost
- **origins file size**: ~100 bytes per item
- **no impact on normal fence operations**

## Future Enhancements

- [ ] Restore confirmation dialog
- [ ] Per-item retention toggle (keep in fence)
- [ ] Batch restore UI
- [ ] Origin path history/logging
- [ ] Backup before restore
- [ ] Restore to trash instead of delete
