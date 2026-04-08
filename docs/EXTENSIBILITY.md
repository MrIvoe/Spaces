# Extensibility Model (Host Core)

This document defines what belongs in the `Spaces` host and what should be implemented as plugins.

## Product Direction

The host is a stable operating platform for Spaces. It should remain small, resilient, and extension-first.

Host responsibilities:

- app lifecycle and Win32 message loop integration
- tray lifecycle and command dispatch routing
- baseline Space behavior (create, move, resize, persistence, drag/drop)
- persistence safety and backward-compatible config evolution
- plugin loading, plugin contracts, registries, and capability routing
- diagnostics, failure isolation, and graceful degradation
- shared rendering/layout primitives used by all Spaces

Plugin responsibilities:

- feature-specific behavior and UX that is not required for every user
- optional menu entries and settings pages
- alternate content providers and Space behavior extensions
- experimental workflows that should not destabilize the host

## Contract Versioning

`PluginManifest` includes:

- `minHostApiVersion`
- `maxHostApiVersion`

The host API version is defined in `src/AppVersion.h` as `SpacesVersion::kPluginApiVersion`.

Plugin load behavior:

1. host validates manifest shape (`id`, `displayName`, `version`)
2. host checks plugin API compatibility range
3. host rejects duplicate plugin IDs
4. host records status and diagnostics for each plugin

Rejected plugins do not crash startup. The host continues in degraded mode.

## Failure Isolation

Host safeguards added in `0.0.011`:

- plugin `GetManifest()` and `Initialize()` calls are exception-isolated
- plugin `Shutdown()` is exception-isolated to keep app teardown safe
- unknown commands are logged as warnings instead of silently failing
- command handler exceptions are captured and reported to diagnostics

## Registry Boundaries

The host validates extension contributions before accepting them:

- `MenuContributionRegistry`
: rejects empty title/command IDs and duplicate entries
- `PluginSettingsRegistry`
: rejects invalid pages, removes invalid fields, normalizes enum defaults, and replaces duplicate page IDs deterministically
- `SpaceExtensionRegistry`
: keeps core fallback provider (`core.file_collection`) for graceful behavior when optional providers are unavailable

These checks keep plugin surfaces deterministic and easier to test.

## Application Command Surface

The host exposes generic app operations to plugins through `IApplicationCommands`.

As of plugin API `2`, plugins can query and refresh Space state without direct access to host internals:

- `GetActiveFenceMetadata()`
- `GetAllFenceIds()`
- `GetFenceMetadata(fenceId)`
- `RefreshFence(fenceId)`

These APIs are intentionally general-purpose so multiple plugins can reuse them (organizers, rules plugins, diagnostics plugins) without adding host special cases.

## Testing

`HostCoreTests` verifies core extension infrastructure without UI dependencies:

- command registration collision rules and dispatch exception safety
- menu contribution validation and deterministic ordering
- settings page normalization and duplicate handling
- plugin/Space registry lookup and fallback behavior

This test suite is intended to stay fast and focused so host contract regressions are caught early.

## What Should Be Plugins Next

Future work that should stay outside host core:

- advanced Space visual modes and custom layout packs
- specialized content providers (portals, widgets, feeds)
- premium interaction packs and policy presets
- custom menu workflows that are not baseline app behavior

If a capability can be added through a plugin contract, prefer that over adding feature-specific host logic.
