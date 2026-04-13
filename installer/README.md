# Spaces Installer

This directory contains the Inno Setup installer configuration for building a professional Windows installer for Spaces.

## Building the Installer

### Prerequisites

1. **Inno Setup 6.x or later** - Download from https://jrsoftware.org/isdl.php
2. **Compiled Spaces.exe** - Build the app with `cmake` and `MSBuild` first

### Build Steps

#### Option 1: Command Line (Recommended)

```powershell
# From the Spaces root directory

# 1. Build Spaces Release
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 2. Build Installer
$env:BUILD_OUTPUT_DIR = "..\build\bin\Release"
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" installer\Spaces.iss
```

#### Option 2: Inno Setup IDE

1. Open `installer/Spaces.iss` in the Inno Setup IDE
2. Click **Build** → **Compile**
3. Installer will be generated to `output/Spaces-Setup-1.01.004.exe`

### Output

The generated installer will be placed in:

```
Spaces/output/Spaces-Setup-1.01.004.exe
```

## Installer Features

- ✅ Version-isolated installs (each setup installs only its release version)
- ✅ Start Menu shortcuts
- ✅ Optional desktop shortcut
- ✅ Optional launch on Windows startup
- ✅ Professional uninstall support
- ✅ Friendly wizard UI

## Installation Locations

### After User Installation

**Per-user versioned install path:**
```
C:\Users\<user>\AppData\Local\Programs\Spaces\1.01.004\
├── Spaces.exe
├── required-runtime-files.dll
└── assets\
    └── plugin-catalog.json
```

**User Data (AppData):**
```
%LOCALAPPDATA%\SimpleSpaces\
├── config.json
├── settings.json
├── debug.log
├── plugins\
├── themes\
└── cache\
```

## First-Run Configuration

When users first launch the installed app:

1. App detects missing `%LOCALAPPDATA%\SimpleSpaces\` folder
2. Creates necessary directories:
   - `plugins/` - for plugins
   - `themes/` - for themes
   - `cache/` - for temporary downloads
3. Initializes default `config.json` and `settings.json`
4. Optionally downloads initial plugin catalog

## Plugin Distribution

The installer includes `assets/plugin-catalog.json` which:

- Contains metadata for available plugins
- Specifies download URLs (from GitHub releases, etc.)
- Includes compatibility info
- Is loaded on first run or when user refreshes

Users can then install plugins from Settings → Plugins without requiring Git.

## Customization

### Change Installation Location

Edit `Spaces.iss` - search for `DefaultDirName`:

```ini
DefaultDirName={autopf}\{#MyAppName}
```

Options:
- `{autopf}` - Program Files
- `{userappdata}` - User AppData
- `{localappdata}` - Local AppData

### Add Files to Installer

Add entries to `[Files]` section:

```ini
Source: "path\to\file.ext"; DestDir: "{app}"; Flags: ignoreversion
```

### Change App Icon

Replace or provide custom icon at:

```
installer\assets\Spaces.ico
```

Then reference in Spaces.iss:

```ini
SetupIconFile=installer\assets\Spaces.ico
```

## Troubleshooting

### Installer won't build

- Check Inno Setup version: `iscc.exe --version`
- Ensure `BUILD_OUTPUT_DIR` environment variable is set correctly
- Verify `build\bin\Release\Spaces.exe` exists

### Icon not appearing

- Place `.ico` file in `installer\assets\Spaces.ico`
- Rebuild installer

### Uninstall not working

- Run installer as Administrator
- Check Windows Add/Remove Programs → Spaces

## Next Phases

### Phase 2: Plugin Downloader

Add HTTP-based plugin download/install UI:

- Replace `git.exe` dependency completely
- Users can install plugins via Settings UI
- No Git installation required

### Phase 3: App Auto-Updates

Implement update checking:

- Check for new app versions online
- Download and install via separate updater
- Run installer for updates

### Phase 4: Theme Manager

Extend to themes:

- Download theme packages
- Visual theme preview
- One-click install/apply

## Further Reading

- [Inno Setup Documentation](https://jrsoftware.org/isinfo.php)
- [Script Sections Reference](https://jrsoftware.org/isdocs/)
- [Release Notes](https://jrsoftware.org/ishistory.php)
