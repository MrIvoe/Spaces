# IVOE Fences

Version: 0.0.006

IVOE Fences is a standalone Win32 desktop-fences application under active refactor. The app still behaves like the existing simple implementation, while Phase 2 introduces metadata structures and migration scaffolding safely.

## Phase Status

- Completed: Phase 1 persistence and storage refactor
- Completed: Phase 2 metadata and migration scaffolding
- Completed: Phase 3 scaffolding slice (layout + selection models)
- In progress: metadata-first behavior alignment for Standard vs Portal fences

## Phase 2 Highlights

- Added `FenceType` (`Standard`, `Portal`) and `DesktopItemRef` data models
- Extended `FenceRepository` JSON schema to persist fence type and membership arrays
- Added a migration layer that converts legacy folder-backed fences into explicit membership records
- Kept legacy folder-backed storage readable and preserved as the source of migrated membership records
- Added `DesktopItemService` scaffolding for desktop enumeration and legacy membership construction
- Added `DesktopWatcher` scaffolding and app wiring without UI behavior changes
- Preserved existing fence UI and interactions while centralizing richer state in repository records

## Runtime Data

- Config: `%LOCALAPPDATA%\IVOEFences\fences.json`
- Folder-backed fence storage: `%LOCALAPPDATA%\IVOEFences\FenceStorage\Fence_<id>`
- Legacy import source: `IVOEFences.ini` next to the executable, if present

## Architecture Notes

- `FenceManager` remains authoritative for runtime fence state and now preserves repository metadata when saving window snapshots.
- `FenceRepository` now persists and loads fence type and per-fence membership records.
- Legacy folder-backed fences are migrated to explicit `DesktopItemRef` records using `LegacyFenceFolder` source markers.
- `DesktopItemService` and `DesktopWatcher` are intentionally scaffolded with no visible UI changes to de-risk later metadata-first behavior.

## Build

The project uses CMake with Visual Studio Build Tools. `nlohmann/json` is fetched during configure through CMake `FetchContent`.

## Changelog

### 0.0.006

- Standard fence drops are now metadata-only: dropped items are tracked as membership without moving or copying files
- Portal fence drops keep move/copy behavior against the portal folder
- `FenceWindow` now uses full `DesktopItemRef` objects instead of label-only item data
- Added a real desktop watcher loop with `ReadDirectoryChangesW` and debounced callbacks
- Marshaled desktop watcher callbacks to the UI thread before invoking `FenceManager` updates
- Extended repository persistence for richer desktop item metadata (`isFolder`, `exists`, `iconIndex`)

### 0.0.005

- Added optional visual selection highlights for selected item rows in `FenceWindow`
- Added keyboard focus cues (focused selection and focused body frame) without changing layout
- Kept current layout and icon rendering behavior unchanged

### 0.0.004

- Enabled non-visual item selection hit-testing in `FenceWindow`
- Enabled keyboard selection navigation (`Up`, `Down`, `Home`, `End`) without changing current visuals
- Kept rendering output unchanged while wiring interaction logic to `FenceSelectionModel`

### 0.0.003

- Added `FenceLayoutEngine` scaffolding for deterministic item-slot layout and hit testing
- Added `FenceSelectionModel` scaffolding for future desktop-like selection behavior
- Wired both into `FenceWindow` internals without changing rendering output
- Added test coverage for layout/selection scaffolding basics

### 0.0.002

- Added Phase 2 fence metadata model (`FenceType`, `DesktopItemRef`)
- Added repository membership persistence and legacy folder migration
- Added `DesktopItemService` and `DesktopWatcher` scaffolding
- Preserved legacy folder-backed fence readability during migration

### 0.0.001

- Completed Phase 1 persistence refactor
- Added repository-backed JSON storage and atomic saves
- Moved fence storage under LocalAppData
- Added legacy INI migration support

## Next Phase

Phase 3 will refactor icon rendering and interaction internals (layout engine, hit-testing, selection model, DPI-aware spacing, and richer text rendering) while keeping behavior stable.
