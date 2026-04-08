# Release 1.01.001 - Installer & Plugin Delivery Architecture

## Overview

**Spaces** has been upgraded to **version 1.01.001**, marking the transition from developer-only to a professional, installable Windows application.

## What Changed

### Phase 1: Installer Infrastructure ✅

#### 1. Version Update
- Updated `AppVersion.h`: `0.0.013` → `1.01.001`
- Marked as release version (x.xx.xxx format)

#### 2. Installer (Inno Setup)
- **File**: `installer/Spaces.iss`
- Features:
  - Professional Windows `.exe` installer
  - Install to Program Files or per-user location
  - Start Menu shortcut creation
  - Optional Desktop shortcut
  - Optional launch on Windows startup
  - Uninstall support in Windows
  - Friendly wizard UI

#### 3. Plugin Catalog System (Foundation)
- **Header**: `src/core/PluginCatalogFetcher.h`
- **Impl**: `src/core/PluginCatalogFetcher.cpp`
- Fetches plugin metadata from JSON catalog
- Validates plugin compatibility
- No Git required

#### 4. Plugin Package Installer (Foundation)
- **Header**: `src/core/PluginPackageInstaller.h`
- **Impl**: `src/core/PluginPackageInstaller.cpp`
- HTTP download of plugin ZIP packages
- Extracts plugins to user data folder
- Validates plugin manifests
- Supports install/uninstall/update

#### 5. Plugin Catalog Assets
- **File**: `installer/assets/plugin-catalog.json`
- Machine-readable metadata for available plugins
- Specifies download URLs, versions, compatibility
- Can be hosted remotely or bundled locally

#### 6. Settings Keys Added
- App update checks: `settings.app.auto_check_updates`
- Launch on startup: `settings.app.launch_on_startup`
- Plugin catalog URL: `settings.plugins.catalog_url`
- Plugin cache path: `settings.plugins.cache_path`
- Cache cleanup: `settings.plugins.clear_cache_on_exit`
- Background update checks: `settings.plugins.background_update_checks`

#### 7. Documentation
- **File**: `installer/README.md` - Complete installer build guide
- **File**: `README.md` - Updated with installation and plugin instructions

---

## Installation Locations

After user installation:

```
C:\Program Files\Spaces\
├── Spaces.exe
├── required-dlls.dll
└── assets\
    └── plugin-catalog.json

%LOCALAPPDATA%\SimpleSpaces\
├── config.json
├── settings.json
├── debug.log
├── plugins\
├── themes\
└── cache\
```

---

## User Experience Flow

### End Users

1. **Download & Install**
   - Download `Spaces-Setup-1.01.001.exe`
   - Run installer
   - Click through wizard
   - App installs with Start Menu entry

2. **Launch**
   - Start from Start Menu or Desktop shortcut
   - App automatically creates user data folders
   - Settings window loads

3. **Install Plugins** (Phase 2)
   - Settings → Plugins Manager
   - Click "Install"
   - App downloads plugin from catalog
   - No Git required!

### Developers

1. **Build Release**
   ```powershell
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```

2. **Build Installer**
   ```powershell
   $env:BUILD_OUTPUT_DIR = "build\bin\Release"
   iscc.exe installer\Spaces.iss
   ```

3. **Output**: `output/Spaces-Setup-1.01.001.exe`

---

## Architecture Going Forward

### Phase 1: Installer ✅
- [x] Inno Setup script
- [x] Version bump to 1.01.001
- [x] Settings keys for app/plugins
- [x] Documentation
- [x] Plugin catalog JSON format

### Phase 2: Plugin Downloader (Ready to implement)
- [ ] Integrate PluginCatalogFetcher into PluginHost
- [ ] Integrate PluginPackageInstaller into plugin manager UI
- [ ] Implement Settings → Plugins Manager UI
- [ ] Add download/install/update/remove buttons
- [ ] HTTP downloads of plugin ZIPs
- [ ] Manifest validation
- [ ] Remove Git dependency

### Phase 3: Theme Downloader
- [ ] Extend theme catalog system
- [ ] Theme package download
- [ ] Theme import/preview UI
- [ ] One-click theme application

### Phase 4: App Auto-Updates
- [ ] Check for new app versions
- [ ] Download and run separate updater
- [ ] Restart app after update
- [ ] Update notifications

---

## What Stays the Same

- ✅ Core Space behavior (create, drag, drop, persist)
- ✅ Settings window and plugin system
- ✅ Theme support
- ✅ Diagnostics logging
- ✅ All existing appearance customization

---

## New Files & Directories

```
Spaces/
├── installer/
│   ├── Spaces.iss                     # Inno Setup script
│   ├── README.md                      # Installer guide
│   └── assets/
│       └── plugin-catalog.json        # Plugin metadata
│
└── src/core/
    ├── PluginCatalogFetcher.h         # Catalog parser
    ├── PluginCatalogFetcher.cpp
    ├── PluginPackageInstaller.h       # ZIP installer
    └── PluginPackageInstaller.cpp
```

---

## Build & Distribution

### For Release Builds

```powershell
# Build release executable
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Install Inno Setup (one-time)
# Download from: https://jrsoftware.org/isdl.php

# Build installer
$env:BUILD_OUTPUT_DIR = "build\bin\Release"
iscc.exe installer\Spaces.iss

# Distribute: output/Spaces-Setup-1.01.001.exe
```

### For Developers

```powershell
# Quick build & run
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
.\build\bin\Debug\Spaces.exe
```

---

## Next Steps

1. **Immediate**: Test installer build with Inno Setup
2. **Week 1**: Integrate plugin downloader UI (Phase 2)
3. **Week 2**: Remove Git dependency from plugin sync
4. **Week 3**: Theme manager UI (Phase 3)
5. **Month 2**: App auto-update system (Phase 4)

---

## Technical Notes

### PluginCatalogFetcher
- Uses `WinInet` for HTTP downloads
- Parses JSON catalog metadata
- Validates plugin compatibility against current host API version
- Supports both local files and remote URLs

### PluginPackageInstaller
- Uses `WinInet` for HTTPS downloads
- Supports SHA256 validation (template-ready)
- ZIP extraction (placeholder for minizip integration)
- Automatic manifest validation
- Handles install/uninstall/update lifecycle

### Settings Registry
- Per-user configuration in `%LOCALAPPDATA%\SimpleSpaces\settings.json`
- New keys pre-defined and ready for UI wiring
- Hot-apply support via theme-change broadcast mechanism

---

## Git Dependencies Removed (After Phase 2)

Currently, `PluginHubSync.cpp` uses `git.exe` for:
- Clone operations
- Fetch updates
- Direct repo sync

**Post-Phase 2**, this will be replaced with:
- HTTP-based catalog fetch
- ZIP package downloads
- Direct file extraction

**Result**: Users will NOT need Git installed.

---

## Compatibility

- Windows 10 and later
- Visual Studio 2022+
- CMake 3.20+
- Inno Setup 6.x+

---

## Questions & Support

See [installer/README.md](installer/README.md) for detailed build instructions.

Refer to [README.md](README.md) for user installation guide.
