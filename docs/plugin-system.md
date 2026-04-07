# Plugin System

## Host Guarantees

- Plugin faults must not block host startup.
- Plugin conflicts are isolated and surfaced through diagnostics.
- Compatibility checks run before plugin behavior is trusted.

## Current Integration Model

- Plugin discovery and load through host runtime.
- Command, menu, provider, and settings contributions.
- Appearance selector ownership guardrails (community.visual_modes canonical path).
- Registry surfaces compatibility status and compatibility reason for each plugin.
- Diagnostics now include compatibility state on plugin load/failure messages.
- Settings shell includes a read-only Plugin Manager scaffold page with per-plugin state and compatibility visibility.
- Plugin Manager scaffold includes status and text filters to triage plugin load/compatibility issues.
- Plugin Manager scaffold now supports persisted enable/disable overrides per plugin id (`settings.plugins.enable.<pluginId>`), applied on next plugin host load.

Related implementation:

- src/extensions/PluginHost.h
- src/core/PluginAppearanceConflictGuard.h
- docs/EXTENSIBILITY.md
