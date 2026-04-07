# Plugin System

## Host Guarantees

- Plugin faults must not block host startup.
- Plugin conflicts are isolated and surfaced through diagnostics.
- Compatibility checks run before plugin behavior is trusted.

## Current Integration Model

- Plugin discovery and load through host runtime.
- Command, menu, provider, and settings contributions.
- Appearance selector ownership guardrails (community.visual_modes canonical path).

Related implementation:

- src/extensions/PluginHost.h
- src/core/PluginAppearanceConflictGuard.h
- docs/EXTENSIBILITY.md
