# SimpleFences - File Execution Features

## What Was Added

### 1. **Double-Click to Open/Execute Files**
- Double-click any file in a fence to open it
- Uses Windows default handler (ShellExecuteW)
- **Text files** → open in default editor (Notepad)
- **Folders** → open in File Explorer
- **Executables** → run directly
- **Links/Shortcuts** → launch target application

### 2. **Visual Feedback on Hover**
- Items highlight when mouse hovers over them
- Darker background color when item is under cursor
- Smooth real-time feedback

### 3. **Enhanced Right-Click Menu**
Before: Only fence options (New/Rename/Delete Fence)
Now: **Two contexts**:

#### When right-clicking an ITEM:
- **"Open"** - Execute/open the file
- **"Delete Item"** - Remove file from fence backing folder

#### When right-clicking empty space:
- **"New Fence"** - Create another fence
- **"Rename Fence"** - (stub for future dialog)
- **"Delete Fence"** - Remove entire fence

### 4. **Item Deletion from Fence**
- Right-click file → "Delete Item"
- Removes file from backing folder (actual deletion, not just hiding)
- Fence refreshes immediately
- Works for both files and folders

## Implementation Details

### Code Changes

**FenceWindow.h** - Added:
```cpp
int GetItemAtPosition(int x, int y) const;     // Hit test for items
void ExecuteItem(int itemIndex);               // Launch file
void OnLButtonDblClk(int x, int y);           // Double-click handler
int m_selectedItem;                            // Track hover state
```

**FenceWindow.cpp** - Updated:
```cpp
// New double-click detection
case WM_LBUTTONDBLCLK:
    OnLButtonDblClk(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    return 0;

// Mouse leave handling
case WM_MOUSELEAVE:
    m_selectedItem = -1;
    InvalidateRect(m_hwnd, nullptr, FALSE);
    return 0;

// File execution
void ExecuteItem(int itemIndex)
{
    ShellExecuteW(m_hwnd, L"open", item.fullPath.c_str(), nullptr, nullptr, SW_SHOW);
}

// Context menu for items
// Differentiate: item menu vs fence menu based on click position
if (itemIndex >= 0) {
    // Item menu: Open, Delete Item
} else {
    // Fence menu: New Fence, Rename, Delete Fence
}
```

## How It Works

### Double-Click Flow
1. User double-clicks in fence window
2. `OnLButtonDblClk` called with (x, y) coordinates
3. `GetItemAtPosition` determines which item was clicked
4. `ExecuteItem` launches `ShellExecuteW`
5. Windows opens file with default handler

### Hit Testing
```
Item 0: y=36 to y=56 (title bar at 28, items start at 36)
Item 1: y=56 to y=76
Item 2: y=76 to y=96
... each item is 20 pixels tall
```

### File Deletion
1. Right-click item → "Delete Item"
2. Check if directory or file
3. Use `std::filesystem::remove_all()` or `remove()`
4. Call `RefreshFence()` to redraw

## Testing Checklist

✅ **Double-click text file** → opens in Notepad  
✅ **Double-click folder** → opens in File Explorer  
✅ **Double-click .bat** → runs batch script  
✅ **Double-click .exe** → launches executable  
✅ **Double-click shortcut** → runs target app  
✅ **Hover items** → items highlight  
✅ **Right-click item** → shows item menu  
✅ **Right-click empty** → shows fence menu  
✅ **Delete item** → file removed from fence  
✅ **Item menu "Open"** → launches file  

## API Details

### ShellExecuteW
```cpp
ShellExecuteW(
    m_hwnd,                    // parent window
    L"open",                   // operation (open, edit, print, etc)
    item.fullPath.c_str(),    // file to execute
    nullptr,                   // parameters
    nullptr,                   // working directory
    SW_SHOW                    // show mode
);
```

### Supported Operations
- **Documents**: .txt, .doc, .pdf → open in default handler
- **Folders**: .bat, .lnk → execute/open as appropriate
- **Executables**: .exe, .com, .cmd → run with cmd.exe
- **Shell Objects**: shortcuts resolve to actual files

## Performance

- Hit testing: O(n) per mouse move (linear with item count)
- Double-click: Instant ShellExecute delegation to Windows
- Deletion: O(1) directory operation, O(n) tree walk for folders
- Rendering: Incremental redraw on hover, full redraw on resize

## Future Enhancements (Phase 2+)

- [ ] File icons (system image list)
- [ ] Drag-to-reorder items
- [ ] Cut/copy/paste support
- [ ] Rename dialog for items
- [ ] Properties dialog
- [ ] Folder thumbnails
- [ ] Recent files support
