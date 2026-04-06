# Theme System Design & Architecture

**Status**: Production Implementation  
**Date**: April 2026  
**Phase**: Win32ThemeSystem Consolidation with Migration

## Overview

The SimpleFences theme system implements a single-source-of-truth architecture where:
- **Win32ThemeSystem** is the canonical theme system at runtime
- **community.visual_modes** is the single allowed appearance selector plugin
- **Public theme packages** enable third-party theme authoring without host code changes
- **Migration layer** handles backward compatibility from legacy theme configs

## Core System Architecture

### 1.  Theme Identity (Single Source of Truth)

**Canonical Storage Keys** (immutable):
```
theme.source = "win32_theme_system"           # Only valid value; enforced at startup
theme.win32.theme_id = "{kebab-case-id}"     # Canonical lookup key; never display_name
theme.win32.display_name = "{Display Name}"  # UI only; never used as ID
theme.win32.catalog_version = "1.0.0"        # Version of theme package/definition set
```

**Legacy Storage Keys** (normalized on migration):
```
appearance.theme.mode       -> theme.mode (light|dark)
appearance.theme.style      -> migrated to theme.win32.theme_id
appearance.theme.custom.*   -> custom theme data
```

### 2. Migration Logic (Startup)

Run **before first render**, idempotent:

1. **Phase 1: Detect state**
   - Check if migration marker exists (`theme.migration_v2_complete`)
   - If marker exists and valid, skip to runtime (idempotency)

2. **Phase 2: Normalize legacy keys**
   - If `theme.source` missing or invalid → set to `win32_theme_system`
   - If `appearance.theme.style` exists → derive `theme.win32.theme_id` from it
   - Convert underscore theme names → kebab-case (e.g., `graphite_office` → `graphite-office`)
   - Populate missing `theme.win32.display_name` from catalog lookup

3. **Phase 3: Populate bridge keys**
   - Set `theme.win32.theme_id` to reasonable default if still missing (graphite-office)
   - Set `theme.win32.display_name` from Win32ThemeCatalog lookup
   - Set `theme.win32.catalog_version` to version of bundled catalog

4. **Phase 4: Persist & mark complete**
   - Save all populated keys
   - Set `theme.migration_v2_complete = true`
   - Log migration result with original → new mappings

### 3. Runtime Resolution

**ResolveWin32ThemeId()** → canonical `theme.win32.theme_id`:
1. Read `theme.win32.theme_id` from store
2. Validate against catalog
3. If invalid, log warning and fall back to `graphite-office`
4. Return kebab-case ID

**ResolvePalette()** → ThemePalette:
1. Get canonical theme ID from ResolveWin32ThemeId()
2. Look up in Win32ThemeCatalog
3. Return palette (or fallback if not found)

### 4. Plugin Conflict Prevention

**Community.visual_modes** is the exclusive appearance selector:
- Allowed to call `SetWin32ThemeId(id)`
- Allowed to list available themes

**Other appearance-selector plugins**:
- On discovery, detect conflict patterns (e.g., command IDs matching appearance.*  or theme.*)
- Log warning: "Plugin '{id}' declares appearance commands but community.visual_modes is the active selector"
- Disable their theme-write paths (do not call their theme set handlers)
- Keep compatibility aliases for legacy commands, but normalize before persistence

### 5. Public Theme Authoring Contract

**Theme Package Structure** (.zip archive):
```
plugin.json                    # Manifest (as per IVOEFences host contracts)
theme-metadata.json           # Theme-specific contract
theme/tokens/default.json     # Token map (required)
theme/tokens/override.*.json  # Optional: override sets
theme/assets/preview.png      # Optional: preview image
theme/assets/readme.txt       # Optional: documentation
```

**theme-metadata.json Schema**:
```json
{
  "themeId": "kebab-case-unique-id",
  "displayName": "Human-Readable Theme Name",
  "version": "1.0.0",
  "author": "Author Name",
  "description": "Theme description",
  "website": "https://example.com",
  "tokenNamespace": "win32_theme_system",
  "minimumHostVersion": "1.0.0",
  "maximumHostVersion": "2.0.0"
}
```

**Token Map Contract** (theme/tokens/default.json):
```json
{
  "win32.base.window_color": "#202124",
  "win32.base.surface_color": "#2A2B2E",
  "win32.base.nav_color": "#1C2128",
  "win32.base.text_color": "#F5F7FA",
  "win32.base.accent_color": "#5090F6",
  ...
}
```

**Validation Rules**:
- Theme ID must be kebab-case alphanumerics + hyphen only
- No version >= 99.0.0 (security/stability)
- No dynamic/executable payloads (only JSON data)
- All token names must be in registered namespace
- No file size exceeding 10MB
- No scripts, binaries, or native code

### 6. Theme Load & Apply

**ApplyTheme(id, source)** timeline:
1. Validate theme ID format (kebab-case)
2. Load theme metadata from source (bundled or package)
3. Validate token map schema
4. Resolve all tokens to on-screen colors
5. Apply tokens atomically (avoid partial repaint)
6. Persist `theme.win32.theme_id` to store
7. Fire WM_SETTINGCHANGE or Win32ThemeChanged event
8. Cache token map for fast re-apply on WM_SETTINGCHANGE

**Fallback Strategy**:
- Unknown ID → `graphite-office` (always valid, always present)
- Missing token → use default from ThemePalette struct
- Load failure → keep previous valid theme applied, log error

### 7. Plugin Appearance Selector (community.visual_modes)

**Allowed Commands**:
- `community.visual_modes.list-themes` → returns [{ id, displayName, source }]
- `community.visual_modes.apply-theme` → SetWin32ThemeId(id)
- `community.visual_modes.current-theme` → GetCurrentWin32ThemeId()

**Not Allowed**:
- Direct writes to `theme.*` keys (must go through host API)
- Changing `theme.source` value
- Loading non-Win32ThemeSystem themes

### 8. Performance & Stability Guarantees

**Atomic Theme Changes**:
- Lock theme state during apply
- All palette changes happen in single store transaction
- No partial repaints

**Debouncing**:
- Rapid theme switch requests (< 100ms apart) are coalesced
- Only one theme change processes at a time

**Caching**:
- Resolved token map cached in memory
- Invalidate cache on successful theme apply
- Warm cache on app startup for current theme

**Error Resilience**:
- Failed package load does not crash app
- Bad token does not break rendering (use default)
- Theme change timeout (5s) → keep previous theme, log error

### 9. Telemetry & Diagnostics

**Logged Events**:
1. App startup: migration start/completion/skipped
2. Migration: each key transformed (old → new mapping)
3. Theme apply: success, fallback, error
4. Plugin conflict: detected + action taken
5. Package validation: success, failure reason

**Format** (serilog/text):
```
[Migration] Normalizing appearance.theme.style=graphite_office → theme.win32.theme_id=graphite-office
[Theme] Applying graphite-office from Win32ThemeCatalog (success)
[Plugin] Conflict detected: plugin.id declares appearance.command but community.visual_modes is active
```

---

## Implementation Roadmap

### Phase 1: Core Migration & Consolidation
1. Create `ThemeMigrationService` with Migrate() entry point
2. Implement NormalizeThemeId() (underscore → kebab-case)
3. Implement FallbackThemeId() (unknown → graphite-office)
4. Wire migration into AppKernel::Initialize()
5. Update ThemePlatform::ResolveStyle() → use canonical `theme.win32.theme_id`

### Phase 2: Plugin Conflict Prevention
1. Create `PluginAppearanceConflictGuard`
2. Wire into PluginLoader on manifest discovery
3. Log conflicts, disable conflicting write paths
4. Add conflict test to HostCoreTests

### Phase 3: Public Theme Authoring
1. Create `ThemePackageValidator` with validation rules
2. Create `ThemePackageLoader` to load .zip archives
3. Document theme contract in markdown
4. Add package validation tests

### Phase 4: Tests & Docs
1. Write 8+ automated test cases (C++ custom harness)
2. Create PUBLIC_THEME_AUTHORING.md guide
3. Add manual verification checklist to release notes

---

## Settings Store Keys (Final Reference)

### Active (Win32ThemeSystem)
```
theme.source                    = "win32_theme_system"
theme.mode                      = "light" | "dark"
theme.win32.theme_id            = "kebab-case-id"
theme.win32.display_name        = "Human Name"
theme.win32.catalog_version     = "1.0.0"
theme.migration_v2_complete     = "true"
```

### Legacy (Normalized on Migration)
```
appearance.theme.mode          (migrated → theme.mode)
appearance.theme.style         (migrated → theme.win32.theme_id)
appearance.theme.custom.*      (preserved but under review for removal)
```

---

## Files to Create/Modify

### New Files
- `src/core/ThemeMigrationService.h/cpp` — migration logic
- `src/core/PluginAppearanceConflictGuard.h/cpp` — conflict detection
- `src/core/ThemePackageValidator.h/cpp` — package validation
- `src/core/ThemePackageLoader.h/cpp` — .zip loading
- `src/core/Win32ThemeCatalog.h/cpp` — bundled theme definitions
- `tests/plugins/ThemeMigrationTests.cpp` — migration tests
- `tests/plugins/ThemePackageTests.cpp` — package validation tests
- `docs/PUBLIC_THEME_AUTHORING.md` — author guide

### Modified Files
- `src/core/ThemePlatform.h/cpp` — use canonical ID, remove old style resolution
- `src/core/AppKernel.cpp` — call migration before first theme use
- `src/core/SettingsStore.h/cpp` — optional: add transactional Set for multiple keys
- `CMakeLists.txt` — add new test files
- `tests/HostCoreTests.cpp` — register new test runners

---
