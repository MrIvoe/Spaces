# Phase 2: Plugin Downloader - Implementation Roadmap

**Status**: Foundation laid in Phase 1, ready to build.

---

## Overview

Remove Git dependency by implementing HTTP-based plugin catalog and ZIP-based package installation directly in the app.

---

## Architecture Already in Place (From Phase 1)

### ✅ Plugin Catalog Fetcher
**File**: `src/core/PluginCatalogFetcher.h/cpp`

Purpose:
- Fetch plugin metadata from JSON catalog
- Validate plugin compatibility
- Parse catalog into structured format

Key Classes:
```cpp
class CatalogFetcher {
    bool FetchCatalog(const std::wstring& source);        // HTTP or file
    const Catalog& GetCatalog() const;
    const PluginEntry* FindPlugin(const std::wstring& id);
    bool IsPluginCompatible(const PluginEntry& entry);
};
```

Usage:
```cpp
PluginCatalog::CatalogFetcher fetcher;
if (fetcher.FetchCatalog(L"https://example.com/plugins.json")) {
    auto catalog = fetcher.GetCatalog();
    // Use catalog.plugins
}
```

### ✅ Plugin Package Installer
**File**: `src/core/PluginPackageInstaller.h/cpp`

Purpose:
- Download plugin ZIP packages
- Extract to plugin folder
- Validate manifests
- Handle install/uninstall

Key Classes:
```cpp
class PackageInstaller {
    InstallStatus InstallFromUrl(
        const std::wstring& pluginId,
        const std::wstring& downloadUrl,
        const std::wstring& expectedSha256);
    
    InstallStatus InstallFromZip(
        const std::wstring& pluginId,
        const std::filesystem::path& zipPath);
    
    bool Uninstall(const std::wstring& pluginId);
};
```

Usage:
```cpp
PluginPackage::PackageInstaller installer(pluginBasePath);
auto status = installer.InstallFromUrl(
    L"plugin.id",
    L"https://example.com/plugin.zip",
    L"abc123def456..."  // SHA256 for integrity
);
```

---

## What Needs to Be Done (Phase 2)

### 1. Settings Window UI Integration

**Location**: `src/ui/SettingsWindow.cpp`

Add plugin installation UI:
- "Plugins" tab in Settings
- Display available plugins from catalog
- Add "Install", "Uninstall", "Update" buttons
- Show plugin version, author, description
- Display compatibility status

### 2. Plugin Manager Page in Settings

**Location**: `src/plugins/builtins/BuiltinPlugins.cpp`

Create new settings page:
```
Settings → Plugin Manager
- List installed plugins
- Show plugin versions
- Enable/disable toggles
- Update buttons
- Uninstall buttons
```

### 3. Hook Plugin Catalog Fetching

**Location**: `src/core/AppKernel.cpp` or new `src/core/PluginManager.cpp`

Implement:
- Load plugin catalog on app startup (if auto-check enabled)
- Handle catalog download
- Compare installed vs. available versions
- Trigger update available notification

### 4. Handle Plugin Installation Workflow

**Flow**:
1. User clicks "Install" next to plugin
2. App downloads plugin ZIP
3. Shows download progress
4. Extracts to `%LOCALAPPDATA%\SimpleSpaces\plugins\`
5. Validates plugin manifest
6. Prompts to reload plugin host
7. Plugin becomes available

### 5. Implement Missing ZIP Extraction

**File**: `src/core/PluginPackageInstaller.cpp`

Currently a stub. Implement using:
- **minizip** library (lightweight, portable)
- **libzip** (more robust)
- Or Windows COM APIs

Replace:
```cpp
bool ExtractZip(const fs::path& zipPath, const fs::path& targetDir) {
    // TODO: Implement actual ZIP extraction
    return true;
}
```

### 6. Implement SHA256 Hash Validation

**File**: `src/core/PluginPackageInstaller.cpp`

Implement:
```cpp
std::wstring CalculateFileSha256(const fs::path& filePath);
bool VerifyPackageIntegrity(const fs::path& zipPath, 
                            const std::wstring& expectedSha256);
```

Use `CryptoAPI` or `Bcrypt.h`:
```cpp
#include <wincrypt.h>
// HCRYPTPROV, CryptCreateHash, etc.
```

### 7. Remove Git Dependency

**File**: `src/core/PluginHubSync.cpp`

Current implementation launches `git.exe`.

**After Phase 2**:
- Keep existing Git-based sync as fallback (dev mode)
- Make it optional compile flag
- Primary path uses PluginCatalogFetcher + PluginPackageInstaller
- Users don't need Git installed

---

## Settings Keys Already Added (Ready to Use)

```cpp
settings.app.auto_check_updates               // bool
settings.app.launch_on_startup                // bool
settings.plugins.catalog_url                  // string
settings.plugins.cache_path                   // string
settings.plugins.clear_cache_on_exit          // bool
settings.plugins.background_update_checks     // bool
settings.plugins.auto_check_updates           // bool
settings.plugins.update_channel               // enum: stable/preview
settings.plugins.allow_preview                // bool
```

All ready in `BuiltinPlugins.cpp` "App & Updates" page.

---

## CMakeLists.txt Changes Needed

Add new source files to build:

```cmake
add_executable(Spaces
    # ... existing files ...
    src/core/PluginCatalogFetcher.cpp
    src/core/PluginPackageInstaller.cpp
    # ... rest ...
)

# Dependencies
find_package(nlohmann_json 3.11.0 REQUIRED)  # For JSON parsing
target_link_libraries(Spaces nlohmann_json::nlohmann_json)

# Windows libraries (already linking)
target_link_libraries(Spaces wininet.lib)     # Already there
target_link_libraries(Spaces wincrypt.lib)    # For SHA256
```

---

## Testing Checklist

### Unit Tests
- [ ] CatalogFetcher::FetchCatalog() with local file
- [ ] CatalogFetcher::FetchCatalog() with HTTP (mock)
- [ ] CatalogFetcher::IsPluginCompatible()
- [ ] PackageInstaller::InstallFromZip()
- [ ] PackageInstaller::Uninstall()
- [ ] SHA256 verification

### Integration Tests
- [ ] Download plugin from catalog
- [ ] Install to plugin folder
- [ ] Plugin loads after install
- [ ] Uninstall removes plugin
- [ ] Update checks version correctly

### UI Tests
- [ ] Settings → Plugins shows list
- [ ] Install button works
- [ ] Download progress shown
- [ ] Error messages display
- [ ] Uninstall button works

---

## Estimated Effort

| Task | Days | Notes |
|------|------|-------|
| ZIP extraction impl | 1-2 | Use minizip library |
| SHA256 validation | 0.5 | Use CryptoAPI |
| Plugin catalog UI | 1-2 | Settings window updates |
| Install workflow | 1 | Hook all pieces together |
| Error handling | 0.5 | Graceful failures |
| Testing | 1-2 | Unit + integration |
| **Total** | **~5-7** | Sequential, 1 dev |

---

## Resource Links

### Libraries
- **minizip**: https://github.com/madler/zlib/tree/master/contrib/minizip
- **nlohmann/json**: https://github.com/nlohmann/json (already used)

### APIs
- **WinINet**: https://docs.microsoft.com/en-us/windows/win32/wininet/about-wininet
- **CryptoAPI/SHA256**: https://docs.microsoft.com/en-us/windows/win32/seccrypto/cryptography-functions

### Examples
- See `UniversalThemeLoader.cpp` for similar HTTP usage pattern
- See `ThemePackageValidator.cpp` for manifest validation pattern

---

## Success Criteria Phase 2

- ✅ No Git dependency required for plugin installation
- ✅ Users can install plugins from Settings
- ✅ Plugin download shows progress
- ✅ Plugin manifest validated
- ✅ Update checks work
- ✅ Uninstall removes plugin correctly
- ✅ Works with plugin catalog hosted remotely or locally

---

## After Phase 2

Once plugin downloader is complete:
- Release as 1.02.001
- Document in release notes
- Update user guide
- Begin Phase 3 (theme downloader)

---

**Next Phase Owner**: Ready for implementation!
